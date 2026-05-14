# CLAUDE.md — userland/

You are working inside Skalapos userland. The top-level [`../CLAUDE.md`](../CLAUDE.md) is the broad guide; this is userland-specific.

## Layout

```
userland/
  libc/                 the small libc — see CLAUDE.md inside
    include/
      skalops/              Skalapos primitives: <skalops/spawn.h>, <skalops/channel.h>, <skalops/handle.h>, …
      sys/              syscall-adjacent: <sys/status.h>, <sys/syscall.h>
      (unprefixed)      ISO-C-ish: <string.h>, <stdio.h>, <stdlib.h>, …
    src/
      sys/              crt0, raw syscall wrappers (generated)
      string/           memcpy, memset, str*, …
      stdio/            printf, puts, fputs
      stdlib/           malloc (bump in v1), abort, atoi
  sh/                   /bin/sh — PID 1 in v1
  utils/                /bin/echo, /bin/cat, /bin/ls, /bin/pwd, /bin/mkdir
```

## Coding posture

1. **Use libc functions where they exist; don't dial syscalls directly.** Utilities should not `#include <sys/syscall.h>` — that's a libc-internal detail. They use `<skalops/...>` and ISO-C-ish headers.
2. **No `errno`, no `-1` returns.** Functions return `status_t` directly or as part of a result struct.
3. **No malloc in `/bin/sh`.** The shell uses a fixed-size BSS buffer for input. This is intentional (v1 malloc is a leaky bump allocator).
4. **No threads in v1 utilities.** They are single-threaded run-to-exit programs.
5. **No environment variables read in v1.** `envp` is plumbed to `main` but not interpreted; `getenv` does not exist.
6. **Programs receive their handles via the kernel-set-up table.** Conventional slots: stdin=1, stdout=2, stderr=3, then anything the parent passed via `proc_spawn`'s `handles_to_pass`.

## Build flags

Userland is compiled with the same `-ffreestanding -nostdlib` set as the kernel, minus `-fno-pic -fno-pie` (userland is PIC, for ASLR). Programs link against `libc.a` and are linked with `userland/libc/src/sys/crt0.o` as the first object.

## How to add a utility

1. Create `userland/utils/<name>/<name>.c` with a `main(int argc, char** argv, char** envp)`.
2. Keep it small. The v1 set is deliberately tiny; if a utility wants more than ~150 LOC, ask before writing it.
3. The build will pick it up automatically if `gen_build.py` is implemented to walk `userland/utils/*/`.
4. Add the binary to the initramfs by listing it in `initramfs/manifest.txt` (or whatever the build's CPIO step ends up reading).

## How to add a libc function

1. Pick the right header. ISO-C-ish things go under `userland/libc/include/<name>.h`. Skalapos-specific things go under `userland/libc/include/skalops/<name>.h`.
2. Implement under `userland/libc/src/<subsystem>/<name>.c`.
3. Add a single-line `///` comment with the function's contract.
4. If the function wraps a syscall, do not write the wrapper by hand — add the syscall to the schema and let codegen produce it.

## Don't

- Don't `#include <linux/...>` or any other host-OS header.
- Don't add POSIX-compat shims (no `_GNU_SOURCE`, no `_POSIX_C_SOURCE`).
- Don't link against the host `libc.so` or `libgcc` (the cross toolchain handles compiler runtime via `libgcc.a`).
- Don't reintroduce `errno`. If you find yourself missing it, you want a result struct instead.
