# Skalapos

Skalapos (σκάλοπος, "of the mole") is a toy operating system designed as an evolution of POSIX. Its goal is to keep what POSIX got right and discard or replace what POSIX got wrong, in a small enough codebase that one person can hold the whole thing in their head.

**Targets:** ARM64 (Raspberry Pi 4) and x86_64.
**Workflow:** cross-compile + QEMU from day one.
**Language:** C (freestanding).
**Kernel architecture:** monolithic.
**Status:** design phase. No code yet; this repository contains the blueprint and scaffolding.

## What's in here

- [`docs/`](docs/) — the design. Start with [`docs/overview.md`](docs/overview.md). One file per design pillar lives under [`docs/pillars/`](docs/pillars/).
- [`schemas/`](schemas/) — single sources of truth for the syscall ABI and status codes.
- [`arch/`](arch/), [`kernel/`](kernel/), [`userland/`](userland/) — the eventual source tree, currently with placeholder files and per-area `CLAUDE.md` orientation notes.
- [`tools/`](tools/) — host-side helpers: build-system generator, initramfs packer.
- [`justfile`](justfile) — top-level commands (build, qemu, gdb, test, clean).

## The five-minute pitch

Skalapos changes POSIX in eleven ways, all aimed at flattening the design — removing categories of bugs and special cases, not adding novelty. The pillars:

| # | Pillar | Skalapos's answer | Replaces |
|---|---|---|---|
| 1 | Handles | Per-process handle table with kernel-side type tags | Bare `int` file descriptors |
| 2 | Process creation | `spawn()` with explicit handle list, `clone()` for threads | `fork()` + `exec()` |
| 3 | Errors | Typed `(Status, Value)` return from every syscall | `-1` sentinel + `errno` |
| 4 | Events | Per-thread trap handlers (sync faults) + typed messages on channels (async) | Signal handlers |
| 5 | Filesystem | POSIX-style global tree, but `*_at` everywhere with `AT_BENEATH`/`AT_NOFOLLOW` | Path-based syscalls with TOCTOU |
| 6 | I/O | Sync blocking in v1; completion-on-channel async in v2 | `O_NONBLOCK` + `select`/`epoll` |
| 7 | Memory | Split-by-intent syscalls, unified `ShmHandle`; DEP+ASLR from day one | `mmap`-omnibus, System V shm, POSIX shm, `brk` |
| 8 | Drivers | In-kernel, statically linked; `file_read`/`file_write` for streams + one `dev_op(h, op, args)` for structured ops | `ioctl`, `fcntl`, `prctl` |
| 9 | Init | PID 1 is `/bin/sh` in v1; reaper + separate service manager in v2 | System V init / systemd sprawl |
| 10 | Build | GCC freestanding cross-toolchain + Python-generated Ninja + Justfile | Make recursion |
| 11 | Userland | Tiny libc, tiny shell, a few utilities; bump allocator until v2/v3 | glibc, busybox |

Details, rationale, and contracts in [`docs/pillars/`](docs/pillars/).

## Why "Skalapos"?

Ancient Greek for "mole," genitive case (σκάλοπος, *of the mole*). Moles dig. So does this.

## Versioning intent

- **v1:** boots in QEMU on x86_64 and aarch64, drops into a shell, can run a handful of utilities. Demonstrates every locked-in ABI.
- **v2:** persistent storage (virtio-blk + simple on-disk FS), service manager, async I/O variants.
- **v3+:** wide open. Some directions documented in [`docs/roadmap.md`](docs/roadmap.md); none committed.

## License

MIT. See [`LICENSE`](LICENSE).
