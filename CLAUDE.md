# CLAUDE.md — Skalapos collaboration guide

This file is your orientation when working in this repo. It's deliberately short. Per-area `CLAUDE.md` files exist in subdirectories and load only when you're working in them.

## What this is

Skalapos is a toy POSIX-evolution OS in freestanding C. Monolithic kernel. ARM64 (Pi 4) + x86_64. Cross-compile + QEMU from day one. See [`README.md`](README.md) and [`docs/overview.md`](docs/overview.md).

The project is in design phase. The thirteen core decisions are locked in and documented per pillar in [`docs/pillars/`](docs/pillars/). Read them before suggesting anything that touches an ABI.

## Where to look first

1. [`docs/overview.md`](docs/overview.md) — five-minute mental model.
2. [`docs/pillars/`](docs/pillars/) — one file per locked-in decision, with **goals / contract / pseudocode / rationale / v2+ direction** sections.
3. [`docs/implementation.md`](docs/implementation.md) — suggested v1 build order (phases 0-4, ≈7-9 weeks of work). Consult this when starting any implementation task; the phase tells you what other pieces must already be in place.
4. [`schemas/`](schemas/) — the single source of truth for syscalls and status codes. Anything that changes the ABI changes this file first; codegen does the rest.
5. The subdirectory `CLAUDE.md` files in [`kernel/`](kernel/), [`userland/`](userland/), [`arch/`](arch/), [`tools/`](tools/) — task-specific guidance.

## How to add or change a syscall

1. Edit [`schemas/syscalls.toml`](schemas/syscalls.toml).
2. Run `just codegen`.
3. Implement the kernel-side handler in [`kernel/syscall/`](kernel/syscall/).
4. The libc wrapper is generated; you don't write it.
5. Add or update the corresponding pillar doc in [`docs/pillars/`](docs/pillars/) if the change is semantic, not just cosmetic.

Never hand-write a syscall number or status code in two places. The schema is the truth.

## How to add a driver

See [`docs/pillars/08-drivers.md`](docs/pillars/08-drivers.md). Short version: implement `struct driver_ops` for a known device class, call `driver_register()` from kernel init, expose any structured ops via the per-class `dev_op` codes declared in `<skl/dev/<class>.h>`. No `ioctl`.

## How to build and run

```
just build x86_64       # build kernel + initramfs for x86_64
just qemu x86_64        # build and run in QEMU
just debug x86_64       # build with -O0 -g, run QEMU with -s -S (GDB stub)
just gdb x86_64         # attach gdb-multiarch to the running stub
just clean
```

Same commands work for `aarch64`. See [`docs/dev-setup.md`](docs/dev-setup.md) for prerequisites.

## Anti-patterns — do NOT do these

These have been considered and explicitly rejected. If a piece of work seems to require any of them, surface it as a *design decision* before writing code.

- **No `ioctl`, `fcntl`, or `prctl`.** Structured ops go through `dev_op(h, op_code, args)`. See pillar 8.
- **No `errno` and no `-1` sentinel returns.** Syscalls return `(Status, Value)`. See pillar 3.
- **No `fork()`.** Process creation is `spawn(image, args, env, handles_to_pass, options)`. See pillar 2.
- **No async signal handlers.** Sync CPU faults get per-thread trap handlers; everything else is typed messages on channels. See pillar 4.
- **No capability theology.** Typed handles exist; *capabilities-with-rights* were rejected. Don't repropose Fuchsia/seL4-style designs.
- **No global ambient FS root for security purposes.** Sandboxing is a libc/init concern via `proc_chroot(dir_h)`, not the default model — but POSIX paths and `cwd` *do* exist. See pillar 5.
- **No `O_NONBLOCK` / `epoll` / `select`.** v1 is sync-blocking; v2 adds completion-on-channel async. Readiness model is explicitly skipped. See pillar 6.
- **No `mmap`-omnibus.** Split by intent: `vm_alloc`, `vm_map_file`, `vm_map_shared`. See pillar 7.
- **No `brk`, `mremap`, `mlock` in v1.** See pillar 7.
- **No `W | X` pages.** DEP enforced; rejected at `vm_alloc` and `vm_protect`. See pillar 7.
- **No loadable kernel modules.** Drivers statically linked. See pillar 8.
- **No uid/gid privilege checks in v1.** Privilege deferred entirely. See pillar 9.
- **No GNU Make.** Build is Python-generated Ninja driven by Justfile. See pillar 10.
- **No POSIX-compat libc.** Skalapos's libc is its own; no goal of porting glibc users. See pillar 11.
- **No POSIX zombies.** A dead process delivers its exit status as a channel message; `proc_wait` is a convenience wrapper over the wait list. See pillar 12.
- **No SMP in v1.** Single CPU. Locking discipline introduced subsystem-by-subsystem when SMP work begins in v2. See pillar 12.
- **No FAT32, exFAT, or ext2 byte-for-byte compat.** v2's on-disk FS is `skfs`, ext2-lineage with sane modern choices. See pillar 13.

## Coding conventions (terse)

- C11, freestanding (`-ffreestanding -nostdlib`).
- Style: 4-space indent, opening brace on same line, snake_case for functions/variables, ALL_CAPS for macros and enum members.
- Header layout: one public header per module, in `<skl/...>` (Skalapos-specific) or unprefixed (ISO-C-ish).
- Every public function gets a one-line `///` comment with its contract. No multi-paragraph docstrings.
- No comments explaining *what* code does; only *why* if non-obvious.
- Status codes are typed enums per subsystem; never raw ints in interfaces.

## When in doubt

- A locked decision conflicts with the task: stop and ask. Don't quietly bypass a pillar.
- The task seems to require a new pillar: write a short proposal in `docs/proposals/` and ask.
- Memory of past Skalapos sessions exists in your memory system under names starting with `skalapos_`.
