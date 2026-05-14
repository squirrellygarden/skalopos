---
name: dev
description: Implement a Skalapos feature, subsystem, or phase task. Invoke with /dev [task description]. Reads relevant pillar docs and checks the schema before writing any code.
disable-model-invocation: true
allowed-tools: Read Bash Edit Write
argument-hint: [task description]
---

You are implementing a feature or fixing a bug in **Skalapos**, a toy POSIX-evolution OS in freestanding C.

## Before writing any code

1. Read `docs/implementation.md` to find which phase the task belongs to and confirm prerequisites are in place.
2. Read the relevant pillar doc(s) in `docs/pillars/` — specifically the **contract** and **pseudocode** sections.
3. If the task touches the syscall ABI or status codes, read `schemas/syscalls.toml` first. Never hand-write a syscall number; always derive it from the schema.
4. Read the subdirectory `CLAUDE.md` for the area you're working in (`kernel/`, `userland/`, `arch/`, `tools/`) if it exists.

## Implementation rules

Apply all of the following — these are in addition to general software engineering best practices, not instead of them:

- **Language:** C11, freestanding (`-ffreestanding -nostdlib`). No hosted-environment assumptions.
- **Style:** 4-space indent, opening brace on same line, `snake_case` for functions and variables, `ALL_CAPS` for macros and enum members.
- **Headers:** One public header per module under `<skl/...>`. Every public function gets a one-line `///` doc comment stating its contract.
- **Comments:** Only when the *why* is non-obvious. Never explain what the code does.
- **Errors:** Syscalls return `(Status, Value)` pairs. No `-1` sentinel returns. No `errno`. Status codes are typed enums per subsystem.
- **No forbidden patterns** — do not introduce any of the following (these were explicitly rejected in the design):
  - `ioctl`, `fcntl`, `prctl` — use `dev_op(h, op_code, args)` instead
  - `fork()` — use `spawn()` / `clone()`
  - Async signal handlers — use per-thread trap handlers or typed channel messages
  - `O_NONBLOCK`, `epoll`, `select` — v1 is sync-blocking only
  - `mmap` omnibus — use `vm_alloc`, `vm_map_file`, `vm_map_shared`
  - `brk`, `mremap`, `mlock` — not in v1
  - `W|X` pages — rejected at `vm_alloc` and `vm_protect`
  - Loadable kernel modules — drivers are statically linked
  - uid/gid privilege checks — deferred entirely in v1
  - POSIX-compat libc — Skalapos libc is its own

## After writing code

Run `just build x86_64` (or `aarch64` if relevant) and fix any errors before reporting done. If the build system isn't set up yet, note that explicitly rather than claiming success.

## If the task requires a new pillar or changes the ABI

Stop, explain why, and ask the user before proceeding. Don't quietly bypass a locked decision. If a new design decision is needed, say so and offer to write a proposal in `docs/proposals/`.

## Task

$ARGUMENTS
