# Skalapos

Skalapos (σκάλοπος, "of the mole") will be an operating system designed as a reinterpretation of Unix. Its goal is to maintain the aesthetics and general principles of *nix ("everything is a file") while using hindsight to flatten the design. Skalapos prefers simplicity of understanding (design, code, artifacts) to scalable efficient operation. 

Skalapos is a security concious operating system, but unlike many modern takes on an operating system (e.g. Zircon/Fuschia or Rust OSes), the design does not intend to optimize for use in security-critical, networked, or multi-user systems. Thus, many design decisions that a modern operating system might make (e.g., microkernel, memory safe language) were intentionally struck down in favor of technical simplicity, ease of use, and aesthetic cohesion.

**Targets:** ARM64 (Raspberry Pi 4) and x86_64.
**Language:** C (freestanding).
**Kernel architecture:** monolithic.
**Status:** Design.

## Contents

- [`docs/`](docs/) — the design. Start with [`docs/overview.md`](docs/overview.md). One file per design pillar lives under [`docs/pillars/`](docs/pillars/). Suggested build order in[`docs/implementation.md`](docs/implementation.md).
- [`schemas/`](schemas/) — single sources of truth for the syscall ABI and status codes.
- [`arch/`](arch/), [`kernel/`](kernel/), [`userland/`](userland/) — the eventual source tree. Currently placeholder.
- [`tools/`](tools/) — build tools.
- [`justfile`](justfile) — top-level commands (build, qemu, gdb, test, clean).

## Design Summary

Skalapos changes *nix in thirteen ways, all aimed at flattening the design. The pillars:

| # | Pillar | Skalapos's answer | Replaces |
|---|---|---|---|
| 1 | Handles | Per-process handle table with kernel-side type tags | Bare `int` file descriptors |
| 2 | Process creation | `spawn()` with explicit handle list; `clone()` for threads |  `clone()` and `exec()` interfaces |
| 3 | Errors | Typed `(Status, Value)` return from every syscall | disunified error conditions + `errno` |
| 4 | Events | Per-thread trap handlers (sync faults) + typed messages on channels (async) | Signals |
| 5 | Filesystem | UNIX file tree, but with modern sensibilities | Path-based TOCTOU |
| 6 | I/O | Sync blocking in v1; completion-on-channel async in v2 | `O_NONBLOCK` + `select`/`epoll` |
| 7 | Memory | Intent-split syscalls, unified `ShmHandle`; memory safety considered from day one | `mmap`-omnibus, POSIX `shm`, `brk` |
| 8 | Drivers | `file_read`/`file_write` for streams + one `dev_op(h, op, args)` for structured ops | disunified `ioctl`, `fcntl`, `prctl` |
| 9 | Init | Tiny supervisor/reaper | systemd sprawl |
| 10 | Build | GCC freestanding cross-toolchain + Python-generated Ninja + Justfile | Make recursion |
| 11 | Userland | "Skalapos ecosystem" libc, shell, and dynamic allocator | glibc, busybox |
| 12 | Scheduling | Threads as scheduling unit; round-robin; user-preemptive non-preemptive-kernel | POSIX zombies, scheduler sprawl |
| 13 | skfs (v2) | "Modern" take on FAT32 symplicity | Dense, human-unreadable filesystems. |

Details, rationale, and contracts in [`docs/pillars/`](docs/pillars/).

## Etymology of "Skalapos"

Ancient Greek for "mole," in the genetive case (σκάλοπος, *of the mole*). Moles and other common terranian "pests" (squirrels, oppossums, skunks, etc.) embody the beauty of the earth. In the endevour of Skalapos, I hope to express in code the beauty of these animals.

## Versioning intent

- **v1:**  Demonstrate ABI and core kernel functions with userland / persistant storage as an afterthought.
- **v2:** persistent storage, and async I/O variants. Potentially kernel optimizations. In userland, a proper service manager/`init` and 
- **v3+:** See ideas in [`docs/roadmap.md`](docs/roadmap.md).

## License

MIT. See [`LICENSE`](LICENSE). Just don't do anything you know in your gut is morally wrong. Artificial intellegence is involved in the process of developing this system, and as such, I can't claim to have full ownership over its artifacts.