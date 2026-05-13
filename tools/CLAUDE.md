# CLAUDE.md — tools/

Host-side helpers. These run on the developer's machine, NOT on Skalapos.

## What's here

- `gen_syscalls.py` — emits kernel dispatch table, libc syscall wrappers, status header, from `schemas/syscalls.toml` + `schemas/status.toml`.
- `gen_build.py` — emits `build/<arch>/build.ninja` for a given arch by walking the source tree.
- `mkcpio.c` — host C program that packs `initramfs/` into a CPIO archive the kernel can mount as a ramdisk.

## Conventions

- Python tools target 3.9+ (use `tomli` shim for `tomllib` on <3.11).
- C tools (like `mkcpio`) are compiled with the host's regular `cc`, not the cross toolchain.
- Tools that generate code emit a `// DO NOT EDIT — generated from <source>` header on every output file.
- Tools that generate code check inputs for ABI-stability invariants: no renumbering, no name collisions, no removed entries (only `deprecated = true`).

## Don't

- Don't write tools that need to be installed. Everything here is `python3 tools/<name>.py` or `make`-compiled C, run from a checkout.
- Don't reach into the network from tools. The build must work offline.
- Don't put `cargo` / `go` / `node` tools here without a strong reason. Python and C are it.
