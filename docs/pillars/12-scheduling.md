# Pillar 12 — Process management & scheduling

## Goals

- A scheduling unit smaller than the process (so multi-threaded processes don't share a single time slice).
- A v1 policy small and dumb enough to debug by reading the runqueue; a v2 policy good enough to feel like a real OS.
- Preemption that protects against runaway user code without inviting in the entire SMP-locking story.
- Single CPU in v1, designed for clean SMP extension in v2.
- A blocking primitive that's used uniformly across every blocking syscall.

## Contract

### Scheduling unit

The **thread** is the scheduling unit. A process is a container of: an address space, a handle table, a control channel, and one or more threads. The scheduler does not know about processes for the purpose of picking what runs next; it picks threads.

This means a process with N runnable threads gets N times the CPU of a single-threaded process under round-robin. That's intentional in v1. v2's policy will revisit (a "fair share per process" tweak is one option; CFS-style task groups are another).

### Thread states

```c
enum thread_state {
    THREAD_NEW,         // created but not yet on the runqueue
    THREAD_RUNNABLE,    // on the runqueue, waiting for a CPU slot
    THREAD_RUNNING,     // currently executing on a CPU
    THREAD_BLOCKED,     // sleeping on a wait_list somewhere
    THREAD_EXITING,     // running its destructor / parent notification
    THREAD_DEAD,        // memory still allocated but not runnable; reaped soon
};
```

Transitions:
- `NEW → RUNNABLE`: `sched_add(thread)` after construction.
- `RUNNABLE → RUNNING`: scheduler picks it.
- `RUNNING → RUNNABLE`: time slice expires (timer preempt) or `sched_yield()`.
- `RUNNING → BLOCKED`: thread calls a blocking primitive (`wait_list_add` + `scheduler_block`).
- `BLOCKED → RUNNABLE`: another party calls `wait_list_wake_*` for this thread's wait list.
- `RUNNING → EXITING → DEAD`: thread calls `thread_exit` or its process is being torn down.

### Scheduling policy (v1)

**Round-robin with a fixed time slice. ~50 LOC. Explicitly placeholder.**

- Single global runqueue. Doubly-linked intrusive list of `THREAD_RUNNABLE` threads.
- Time slice: **10 ms** (review for aarch64 generic timer overhead).
- Timer IRQ fires every tick (~1 ms granularity); the scheduler decrements the running thread's remaining slice and, at zero, moves it to the tail of the runqueue and picks the head.
- A blocking syscall calls `scheduler_block()`, which picks the runqueue head directly.
- `sched_yield()` (syscall) moves caller to runqueue tail and picks head.

**Do NOT grow features on top of RR.** If v1 needs anything resembling priority, nice-ness, or fairness across users, that signals it's time to design the v2 policy properly rather than bolting on. See v2+ direction below.

### Preemption model

**User-preemptive, kernel non-preemptive.**

- Timer IRQ can preempt any user-mode code at any instruction. The handler saves user context, runs scheduler bookkeeping, may pick a different thread.
- Kernel code runs to completion or to an explicit yield/sleep point. Kernel code is NEVER preempted by another kernel thread on the same CPU.
- Practical consequence: most kernel data structures do not need locks in v1 (single CPU + non-preemptive kernel = the kernel is effectively single-threaded from the kernel's own point of view). The only synchronization concern is between IRQ handlers and the synchronous kernel path. Use simple IRQ-disable/enable critical sections for those.

This model is what early Linux ran with for years and is the right starting point. v2's SMP work will replace this with proper locking; the v2 design pass for SMP is *also* when fully-preemptive kernel becomes worth revisiting.

### SMP

**Single CPU in v1.** On Pi 4 we boot only the BSP and ignore cores 1-3. On x86 we ignore APs.

v2 plan, designed but not implemented:
- Per-CPU runqueues, each protected by its own lock.
- Work stealing: idle CPUs pull threads from the busiest CPU's queue.
- Cross-CPU wakeups via IPIs (x86: APIC IPI; aarch64: GIC SGI).
- Locking discipline introduced subsystem-by-subsystem during the SMP bring-up.

### Wait queues (the blocking primitive)

**Per-object wait list.** Every kernel object that can be blocked on embeds one (or more) `wait_list` fields.

```c
struct wait_list {
    struct thread* head;   // intrusive doubly-linked via thread->wait_next/prev
    struct thread* tail;
};

// Block the calling thread until someone wakes it on `wl`.
// Caller must hold whatever lock protects the condition being waited on
// (the lock is released across the block and reacquired on wake — TBD: in
// v1's single-CPU non-preemptive kernel, this is just IRQ disable/enable).
void wait_list_add(struct wait_list* wl, struct thread* self);
void scheduler_block(void);                  // marks current thread BLOCKED, switches

// Wake one or all threads. Caller holds the same lock.
void wait_list_wake_one(struct wait_list* wl);
void wait_list_wake_all(struct wait_list* wl);
```

Embedded in objects:
```c
struct channel {
    /* ... message ring buffer ... */
    struct wait_list send_waiters;   // blocked on a full channel
    struct wait_list recv_waiters;   // blocked on an empty channel
};

struct process {
    /* ... */
    struct wait_list exit_waiters;   // blocked on proc_wait
};

struct timer { /* ... */ struct wait_list waiters; };
```

A blocking syscall is structurally always:

```c
// (single-CPU pseudocode; IRQs disabled, no other kernel thread can interleave)
while (!condition) {
    wait_list_add(&obj->waiters, current);
    scheduler_block();   // returns when someone woke us; condition may still be false
}
```

The loop matters: spurious wakeups (the condition went false again between wake and run) must not cause incorrect behavior. v1's single-CPU kernel makes spurious wakeups rare but not impossible.

### Process lifecycle

`proc_spawn` creates a process + its initial thread (see pillar 2).

`proc_exit(status)`:
1. Marks every thread of the process `THREAD_EXITING`.
2. Closes every handle the process holds (refcount-drops the backing objects).
3. Tears down the address space (`vm_destroy`).
4. Posts `evt_terminated_t{proc_h, status}` to the parent's control channel.
5. Wakes everyone on the process's `exit_waiters` wait list (for `proc_wait`).
6. Marks the process `THREAD_DEAD`; reaper completes the rest asynchronously.

**No zombies in the POSIX sense.** A process's exit status is not held by the kernel waiting for `wait()` — it's *delivered* (as a channel message) the moment exit happens. The `proc_wait` syscall is a convenience that blocks on `exit_waiters` and returns the cached status; if no one calls `proc_wait`, the message in the control channel is still the truth.

**Orphan adoption.** If a process dies with children still alive, the children's parent pointer is reassigned to PID 1, exactly as in Unix. Their eventual `Terminated` messages flow to PID 1's control channel — which is why PID 1 in v2 includes "reap orphan terminations" in its job description.

### `thread_join` and the new syscalls

```c
status_t thread_join(handle_t thread_h, int32_t* out_status);   // blocks on exit_waiters
status_t sched_yield(void);                                     // RR: move self to tail
```

`thread_join` is `proc_wait`'s thread-scoped sibling — blocks on the target thread's exit_waiters list, returns its exit status.

## Pseudocode — scheduler core (v1, single CPU)

```c
// kernel/sched/sched.c
static struct thread* runqueue_head;
static struct thread* runqueue_tail;
struct thread* current;

void sched_add(struct thread* t) {
    t->state = THREAD_RUNNABLE;
    list_add_tail(&runqueue_head, &runqueue_tail, t);
}

void scheduler_block(void) {
    /* called with IRQs disabled by the caller */
    current->state = THREAD_BLOCKED;
    /* current is already removed from runqueue (it wasn't there — it was RUNNING) */
    scheduler_pick_and_switch();
}

static void scheduler_pick_and_switch(void) {
    struct thread* next = runqueue_pop_head();   /* may be the idle thread */
    if (next == current) return;
    struct thread* prev = current;
    current = next;
    current->state = THREAD_RUNNING;
    arch_context_switch(prev, next);            /* implemented in arch/<arch>/context.S */
}

/* Called by the timer IRQ handler. */
void sched_tick(void) {
    if (current == idle_thread) { scheduler_pick_and_switch(); return; }
    if (--current->ticks_remaining == 0) {
        current->ticks_remaining = SLICE_TICKS;
        current->state = THREAD_RUNNABLE;
        list_add_tail(&runqueue_head, &runqueue_tail, current);
        scheduler_pick_and_switch();
    }
}

void wait_list_add(struct wait_list* wl, struct thread* t) {
    /* assumes IRQs disabled */
    t->wait_next = NULL;
    t->wait_prev = wl->tail;
    if (wl->tail) wl->tail->wait_next = t; else wl->head = t;
    wl->tail = t;
}

void wait_list_wake_one(struct wait_list* wl) {
    struct thread* t = wl->head;
    if (!t) return;
    wl->head = t->wait_next;
    if (wl->head) wl->head->wait_prev = NULL; else wl->tail = NULL;
    sched_add(t);   /* state becomes RUNNABLE */
}
```

## Why this over alternatives

- **MLFQ / CFS in v1** — both are real OS-grade designs. Building either alongside the rest of v1's kernel work multiplies bug surface. RR with explicit "placeholder" framing keeps v1 tractable.
- **Fully-preemptive kernel** — required for low-latency real-time, but it forces every kernel data structure to be SMP-safe and to handle preemption mid-operation. Massive complexity tax; not worth paying in v1.
- **Cooperative scheduling everywhere** — a single runaway user loop hangs the whole system. Unacceptable even for a toy.
- **SMP from v1** — every kernel data structure becomes a locking problem. Per-CPU runqueues, cross-CPU wakeup IPIs, memory barrier discipline. v1 is hard enough without it.
- **Global address-keyed wait table (futex-style)** — composes elegantly but is action-at-a-distance: looking at a channel doesn't tell you who's waiting on it. Skalapos's posture is "explicit, typed, local"; per-object wait lists match.
- **POSIX zombies + `wait()` mandatory** — works, but means the kernel holds dead-process state indefinitely waiting for the parent to ask. We already have channels for lifecycle events; the message *is* the notification, and `proc_wait` is just a convenience over it.

## v2+ direction

- **Real scheduler policy.** Most likely MLFQ — it's the right complexity/quality tradeoff for a non-server OS. Tunable boost interval, per-class slice lengths. Design pass starts when v1 ships.
- **SMP.** Per-CPU runqueues, work stealing, cross-CPU wakeup via IPIs, locking discipline added subsystem-by-subsystem. Pi 4's 4 cores become real.
- **Preemptible kernel.** Revisit when SMP is in. Almost certainly stays non-preemptive for a long time after SMP.
- **`chan_recv_any` (multi-channel select).** Adds a small "subscription" abstraction: thread holds slots on multiple wait lists; whichever wakes it removes it from the others. ~30 LOC on top of v1's wait_list primitive.
- **Userland concurrency primitives.** If futexes ever come up, they're a new handle type (`H_FUTEX`) with an embedded wait list. Same mechanism, scaled up.
- **CPU affinity / pinning.** Once SMP exists. Not before.
- **Priorities / nice-ness.** Most schedulers grow them; we'll see if we need them. POSIX `nice` is a fine API to copy if so.
- **Idle-loop power management.** `wfi` on aarch64, `hlt` on x86 — already in v1's idle thread for QEMU sanity. Real CPU frequency scaling: v3+.
