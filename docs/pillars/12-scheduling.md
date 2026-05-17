# Pillar 12 — Process management & scheduling

## Goals

- Threads as the unit of scheduling, uniform blocking primitive.
- RR/non-preemptive kernel/no SMP for simplicity, but preempt userspace to prevent cooperative scheduling problems.

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

### Round Robin Policy

- Prefer simplicity of Round Robin, but open to replacement in later iterations.
- Single global runqueue. Doubly-linked intrusive list of `THREAD_RUNNABLE` threads.
- Timer IRQ fires every tick (~1 ms granularity); the scheduler decrements the running thread's remaining slice and, at zero, moves it to the tail of the runqueue and picks the head.
- A blocking syscall calls `scheduler_block()`, which picks the runqueue head directly.
- `sched_yield()` (syscall) moves caller to runqueue tail and picks head.

### User-preemptive, kernel non-preemptive

- Timer IRQ can preempt any user-mode code at any instruction. The handler saves user context, runs scheduler bookkeeping, may pick a different thread.
- Kernel code runs to completion or to an explicit yield/sleep point. Kernel code is NEVER preempted by another kernel thread on the same CPU.
- Practical consequence: most kernel data structures do not need locks in v1 (single CPU + non-preemptive kernel = the kernel is effectively single-threaded from the kernel's own point of view). The only synchronization concern is between IRQ handlers and the synchronous kernel path. Use simple IRQ-disable/enable critical sections for those.

Preemption model to scale to kernel-preemptive/SMP when basic system utility is achieveed single-core.

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

### `thread_join` and syscalls

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

- **MLFQ / CFS** — Prefer RR simplicity. May revisit later if scale requires it.
- **Fully-preemptive kernel / SMP** — The complexity introduced by preemption/SMP should be gradually introduced. 

## v2+ direction

- SMP
- Kernel preemption
- Userland concurrency primitives
