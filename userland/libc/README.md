# Skalapos libc (v1)

This is Skalapos's libc. It is small, intentional, and NOT a POSIX libc. It does not aim to be glibc-, musl-, or newlib-compatible.

## Status

Stubs. The directory structure is laid out; implementations follow once the kernel is bootable.

## Surface

See [`../../docs/pillars/11-userland.md`](../../docs/pillars/11-userland.md) for the v1 function list and design.

Header layout:

- `include/skl/*.h` — Skalapos-specific primitives (handles, spawn, channels, dev ops).
- `include/sys/*.h` — syscall-adjacent low-level (status codes, raw syscall numbers).
- `include/<name>.h` — ISO-C-ish (string.h, stdio.h, stdlib.h, etc.).

## Big v1 warning: malloc is a leaky bump allocator

`malloc()` advances a cursor into a 64 KiB BSS arena. `free()` is a no-op. **Long-running processes will exhaust the arena.** Specifically:

- `/bin/sh` does NOT use malloc; it uses a fixed-size BSS line buffer.
- Utilities (`echo`, `cat`, `ls`, etc.) run to exit and don't care.
- Do not write long-running services against v1 libc.

A real allocator is planned for v2 or v3. The libc ABI (`malloc`/`free`) is stable; swapping the implementation will not require touching callers.

## What's missing (intentionally) in v1

- `FILE*` and friends. v1 stdio uses raw handles.
- `errno` and all functions that touch it. Errors come back as `status_t`.
- `getenv` / `setenv`. `envp` is passed to `main` but not interpreted.
- Threads (`pthread_*`). The thread API is `<skl/thread.h>`; no pthread shim.
- Locales, wide chars, multibyte, complex math.
- Signal handlers. Faults use `<skl/fault.h>`; everything else is channels.
- Networking. None.

## How to add a libc function

See [`../CLAUDE.md`](../CLAUDE.md).
