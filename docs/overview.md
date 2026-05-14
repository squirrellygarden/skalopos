# Skalapos design overview

This document is the five-minute mental model. For any specific pillar, follow the link to `pillars/NN-*.md`.

## What Skalapos is

A toy operating system built as an evolution of POSIX. Written in freestanding C, monolithic kernel, targeting ARM64 (Raspberry Pi 4) and x86_64, developed against QEMU with a cross-compile toolchain.

Skalapos is not POSIX-compatible and does not aim to be. The goal is to keep the parts of POSIX that work, replace the parts that don't, and ship something small enough that one person can hold the whole thing in their head.

## What "flatten the design" means here

Every pillar is judged by whether it removes a category of bug or special case. Cleverness alone doesn't earn its keep. Examples of the flattening principle in action:

- Typed handles delete `ioctl`, `fcntl`, and `prctl` — three syscalls collapse to one with type checking.
- Spawn-with-explicit-handle-list deletes `fork()`, `close-on-exec`, FD inheritance ambiguity, and most of the COW machinery.
- Typed `(Status, Value)` results delete `errno`, the `-1` sentinel, and thread-local error state.
- Channels delete the entire async-signal-safety problem by construction.
- One `dev_op` syscall deletes `ioctl`.
- A unified `ShmHandle` deletes System V shm, POSIX `shm_open`, and `MAP_SHARED|MAP_ANONYMOUS`-via-fork all at once.

When you find yourself adding a mechanism, ask: which existing mechanism does it replace?

## The Pillars

| # | Pillar | One-line summary | Detail |
|---|---|---|---|
| 1 | Handles | Per-process handle table; kernel-side type tags; no rights bits in v1 (room left for v3+). | [01-handles.md](pillars/01-handles.md) |
| 2 | Process creation | `spawn(image, args, env, handles_to_pass, opts)`; `clone()` for threads; no fork. | [02-spawn.md](pillars/02-spawn.md) |
| 3 | Errors | Every syscall returns `(Status, Value)`; no `errno`, no `-1`. | [03-errors.md](pillars/03-errors.md) |
| 4 | Events | Bucket A: per-thread trap handlers for sync faults. Bucket B: typed messages on channels for async. | [04-events.md](pillars/04-events.md) |
| 5 | Filesystem | Global tree, `cwd`, paths — but `*_at` everywhere; `AT_BENEATH`/`AT_NOFOLLOW` for safe traversal; `proc_chroot(dir_h)` for opt-in sandboxing. | [05-filesystem.md](pillars/05-filesystem.md) |
| 6 | I/O | v1: sync blocking only. v2: completion-on-channel async variants. Readiness model (epoll) skipped. | [06-io.md](pillars/06-io.md) |
| 7 | Memory | `vm_alloc`/`vm_map_file`/`vm_map_shared` split by intent; unified `ShmHandle`; DEP+ASLR always on. | [07-memory.md](pillars/07-memory.md) |
| 8 | Drivers | In-kernel, statically linked. Streaming via `file_read`/`file_write`; structured ops via one `dev_op(h, op_code, args)` typed per device class. | [08-drivers.md](pillars/08-drivers.md) |
| 9 | Init | v1: PID 1 is `/bin/sh`. v2: tiny reaper + separate service manager. Privilege deferred. | [09-init.md](pillars/09-init.md) |
| 10 | Build | GCC freestanding cross-toolchain; Python-generated Ninja; Justfile on top. | [10-build.md](pillars/10-build.md) |
| 11 | Userland | Tiny libc (~15 functions), tiny shell, a few utilities. Bump allocator in v1; real malloc deferred. | [11-userland.md](pillars/11-userland.md) |
| 12 | Scheduling | Thread is the scheduling unit. Round-robin v1 (placeholder); MLFQ v2. User-preemptive, kernel non-preemptive. Single CPU v1, SMP v2. Per-object wait lists. | [12-scheduling.md](pillars/12-scheduling.md) |
| 13 | skfs (v2 FS) | On-disk filesystem replacing the in-memory initramfs. ext2-lineage with sane modern choices (64-bit, UTF-8, xattrs). Metadata journaling lands at v2.1. Hex-readable by design. | [13-skfs.md](pillars/13-skfs.md) |

## How the pillars compose

A single example tracing through them:

> User types `cat /etc/hosts` at the shell.

1. Shell reads the line into a fixed-size buffer (pillar 11 — no malloc dependency in PID 1).
2. Shell calls `spawn("/bin/cat", argv=["cat","/etc/hosts"], env=[], handles=[stdin, stdout, stderr, root_dir, cwd_dir])` (pillar 2). The child receives exactly those handles, nothing more.
3. The child's libc startup unpacks the handles, exposes them via `<skalaps/handle.h>` and the standard FILE* objects.
4. `cat` calls `open_at(root_dir_h, "etc/hosts", O_RDONLY, AT_BENEATH)` (pillar 5). libc strips the leading `/` and translates the path internally; the kernel never sees an absolute path string.
5. Kernel resolves the path component-by-component, refusing symlink escapes because of `AT_BENEATH`. Returns `(STATUS_OK, file_h)` (pillar 3). Handle has type tag `File`, no device class (pillar 1).
6. `cat` loops: `file_read(file_h, buf, 4096, …)` → `file_write(stdout_h, buf, n_read, …)` (pillar 6 sync I/O; pillar 8 streaming).
7. EOF reached. `cat` calls `proc_exit(0)`. Kernel posts a `Terminated{pid, status}` message to the parent shell's control channel (pillar 4).
8. Shell `chnl_recv`s the message and prints a new prompt.

No `fork`. No `errno`. No `ioctl`. No `select`. No `mmap` flag soup. Each step is one well-named operation with a typed result.

## Reading order for someone new (including a future you)

1. This document.
2. [`pillars/01-handles.md`](pillars/01-handles.md) and [`pillars/03-errors.md`](pillars/03-errors.md) — they shape the ABI everything else uses.
3. [`pillars/02-spawn.md`](pillars/02-spawn.md) and [`pillars/04-events.md`](pillars/04-events.md) — they shape the process and concurrency model.
4. Then the rest in numeric order.
5. [`schemas/syscalls.toml`](../schemas/syscalls.toml) — the single source of truth that all of the above produce.

## Non-goals

Explicitly out of scope for this project, possibly forever:

- POSIX source-compat for existing programs.
- Linux ABI emulation.
- Production-grade performance or scalability.
- Multi-user. Skalapos is a single-user toy OS in v1; multi-user is a v3+ question and may never be answered.
- Networking (until v3 at earliest).
- Real-world hardware support beyond Pi 4 and standard PC.
- Graphics beyond serial/text console (until well past v2).
