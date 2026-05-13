# Pillar 4 — Events (faults and async notifications)

## Goals

- Async signal handlers do not exist. The entire category of "async-signal-safety" is eliminated by construction.
- Synchronous CPU faults (segfault, illegal instruction, divide-by-zero) get a clean per-thread handler with full register context.
- Process and thread lifecycle events, timer expirations, IO completions, peer IPC, and kill requests all flow through one uniform mechanism: typed messages on typed channels.
- Every process has a well-known **control channel** that the kernel uses for lifecycle messages.

## Contract — Bucket A: synchronous faults

A "fault" is a synchronous CPU exception caused by the currently-executing instruction. Examples: page fault, illegal instruction, alignment fault, divide-by-zero.

Each thread may register a **fault handler** with the kernel:

```c
typedef struct {
    fault_kind_t kind;        // FAULT_SEGV, FAULT_ILL, FAULT_FPE, FAULT_BUS, FAULT_ALIGN, …
    uintptr_t    fault_addr;  // valid for FAULT_SEGV, FAULT_BUS, FAULT_ALIGN; 0 otherwise
    regs_t       regs;        // full register snapshot at the faulting instruction
} fault_info_t;

typedef void (*fault_handler_fn)(const fault_info_t* info);

status_t thread_set_fault_handler(fault_handler_fn handler, void* alt_stack_base, size_t alt_stack_size);
```

Semantics:

- On fault, the kernel switches to the thread's `alt_stack`, invokes `handler(info)`, and resumes whatever the handler returns to. If the handler `return`s normally, the kernel re-runs the faulting instruction (useful for demand-paging or growing-stack handlers). If the handler calls `thread_exit(...)` or `proc_exit(...)`, the thread or process ends.
- The handler runs **on the same thread that faulted**, never asynchronously interrupting other code. There is no analog of `SA_RESTART`, no signal mask, no async-signal-safe function list.
- If no handler is registered when a fault occurs, the kernel synthesizes a `Faulted{thread_h, fault_info}` message and posts it to the **process's control channel** (Bucket B). Effectively a default handler that defers to the supervisor.

## Contract — Bucket B: channels

A **channel** is a bounded MPMC queue of typed messages. Channels are kernel objects referenced by `H_CHAN` handles. Multiple processes can hold handles to the same channel; messages flow point-to-point (one recv per send).

```c
status_t chan_create(uint32_t capacity, uint32_t flags, handle_t* out_chan_h);
status_t chan_send(handle_t chan_h, const void* msg, size_t msg_len,
                   const handle_t* handles_to_send, size_t handles_count);
status_t chan_recv(handle_t chan_h, void* buf, size_t buf_cap, size_t* out_msg_len,
                   handle_t* handles_buf, size_t handles_buf_cap, size_t* out_handles_count);
status_t chan_close(handle_t chan_h);
```

Semantics:

- **Messages are bytes.** No serialization framework. Senders and receivers agree on the layout via shared headers (e.g., `<skl/event.h>` for kernel-generated messages, application-defined structs for user messages).
- **Handles can ride with messages.** If `handles_to_send` is non-empty, the kernel translates each sender handle into a fresh entry in the receiver's handle table. The handles in the sender remain valid (they share the object by refcount). This is the *only* way to share a handle with a process you did not spawn.
- **`chan_send` blocks if the channel is full** (or returns `STATUS_CHAN_FULL` if `flags & CHAN_NONBLOCK`).
- **`chan_recv` blocks if the channel is empty** (or returns `STATUS_CHAN_EMPTY` if `flags & CHAN_NONBLOCK`).
- **Receiving among threads:** multiple threads of the same process may call `chan_recv` on the same handle. The kernel wakes one of them per message; no ordering guarantees among waiters.
- **Closing:** when all sender-side handles are closed, recv returns `STATUS_CHAN_CLOSED` after draining. When all receiver-side handles are closed, send returns `STATUS_CHAN_CLOSED`.

### The control channel

Each process has exactly one **control channel**. The kernel posts these message types to it:

```c
// kernel-generated, defined in <skl/event.h>:
typedef struct { handle_t proc_h; int32_t status; } evt_terminated_t;       // child exited
typedef struct { handle_t thread_h; fault_info_t info; } evt_faulted_t;     // unhandled fault
typedef struct { handle_t timer_h; } evt_timer_fired_t;                     // timer
typedef struct { uint32_t deadline_ms; } evt_kill_request_t;                // someone asked us to die
typedef struct { handle_t shm_h; uint32_t change; } evt_shm_change_t;       // optional, v2+
// ...
```

Each message begins with a 4-byte type tag (`EVT_TERMINATED`, etc.) so the receiver can dispatch.

The runtime's event loop looks like the user's original sketch:

```c
for (;;) {
    uint8_t buf[256];
    handle_t  handles_in[8];
    size_t    msg_len, handles_count;
    chan_recv(control_chan, buf, sizeof buf, &msg_len,
              handles_in, 8, &handles_count);

    uint32_t kind = *(uint32_t*)buf;
    switch (kind) {
        case EVT_TERMINATED:    handle_terminated(...);    break;
        case EVT_TIMER_FIRED:   handle_timer_fired(...);   break;
        case EVT_KILL_REQUEST:  handle_kill_request(...);  break;
        // ...
    }
}
```

## Kill semantics

```c
status_t process_kill(handle_t proc_h, uint32_t deadline_ms);
```

The kernel posts `evt_kill_request_t{deadline_ms}` on the target's control channel and starts a timer. The target has `deadline_ms` milliseconds to clean up and call `proc_exit`. If the deadline expires, the kernel force-terminates: all threads stopped, memory reclaimed, handles closed, no further userspace runs.

`deadline_ms == 0` is the SIGKILL equivalent — no message is posted; the kernel terminates immediately.

## Why this over alternatives

- **POSIX signals** — every detail is a footgun: tiny fixed numeric namespace (no payload), async delivery into arbitrary code, async-signal-safety, masks, `SA_RESTART`, signal stacks, fork-then-exec interactions. Skalapos splits the two real use cases (sync faults, async notifications) into two clean mechanisms.
- **Single per-process inbox** (no user-created channels) — considered as a simpler v1. Rejected because libraries within a process would fight over the inbox, and threads couldn't easily own their own queue. Multiple channels cost very little once channels exist as an object type.
- **POSIX `sigwait`-only signals** — keeps the worst part of signals (small numeric namespace, no payload) while fixing only the timing problem. Not enough flattening.
- **Mach exception ports for faults** — Skalapos's "no handler → forward to control channel" achieves this *as a fallback*; the per-thread handler is the common case. Both modes exist.

## v2+ direction

- **Async I/O completions.** The async I/O variants (pillar 6) deliver completions on a channel of the caller's choice. Same mechanism as everything else here.
- **Timer handles.** `timer_create(channel_h, duration_or_period, kind) → H_TIMER` produces a handle whose firing is delivered to a chosen channel. No special signal numbers (`SIGALRM`); timers are just another event source.
- **Multi-channel `select`.** A `chan_recv_any(chans[], n)` variant. Not needed in v1 (one control channel handles lifecycle, threads can dedicate themselves to other channels), but a natural extension.
- **Reflection / introspection.** A debug syscall to dump pending messages on a channel without consuming them. Useful for debugging hangs.
