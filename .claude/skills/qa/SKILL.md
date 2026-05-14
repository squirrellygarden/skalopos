---
name: qa
description: Quality review of Skalapos code changes. Invoke with /qa to review staged or recent changes, or /qa [file or area] to focus on specific code.
disable-model-invocation: true
allowed-tools: Read Bash
argument-hint: [file, area, or leave blank for git diff]
---

You are performing a quality review of changes to **Skalapos**, a toy POSIX-evolution OS in freestanding C.

## What to review

If no argument is given, review the current diff:
!`git diff HEAD 2>/dev/null || git diff --cached 2>/dev/null`

If an argument is given, focus on those files or areas but still pull context from git diff.

## How to review

Do a thorough general code review first — correctness, logic, edge cases, resource management, naming clarity, undefined behavior, off-by-one errors, and anything else a careful reviewer would flag. Then additionally check the Skalapos-specific constraints below. The project-specific rules are *extra* checks, not a replacement for general engineering judgment.

## Skalapos-specific checks

**ABI and error handling:**
- [ ] No hand-written syscall numbers — they must come from `schemas/syscalls.toml` via codegen
- [ ] No raw `int` status returns; status codes are typed enums per subsystem
- [ ] No `-1` sentinel returns; all syscalls return `(Status, Value)` pairs
- [ ] No `errno`

**Forbidden patterns** — flag any use of:
- `ioctl`, `fcntl`, `prctl` (use `dev_op(h, op_code, args)`)
- `fork()` (use `spawn()` / `clone()`)
- Async signal handlers (use per-thread trap handlers or channel messages)
- `O_NONBLOCK`, `epoll`, `select`, `poll` (v1 is sync-blocking only)
- `mmap` used as an omnibus (split by intent: `vm_alloc`, `vm_map_file`, `vm_map_shared`)
- `brk`, `mremap`, `mlock` (not in v1)
- Pages mapped `W|X` simultaneously
- `dlopen`, `dlsym`, runtime module loading
- uid/gid privilege checks

**Coding conventions:**
<<<<<<< HEAD
- [ ] 4-space indent, no tabs
- [ ] Opening brace on the same line as the control or function statement
- [ ] `snake_case` for functions and variables; `ALL_CAPS` for macros and enum members
- [ ] One public header per module under `<skl/...>`
=======
- [ ] `clang-format` clean (`.clang-format` at repo root): 4-space indent, same-line braces, pointer as `void * foo`, column limit ~100
- [ ] `snake_case` for functions/variables; `ALL_CAPS` for macros and enum members
- [ ] `//` comments only — never `/* */`; one space after `//`
- [ ] Explicit-size types (`uint32_t`, `uint64_t`…); `char` only for ASCII strings
- [ ] No `typedef struct` without justification
- [ ] No indent-alignment of variable declarations (only `#define`/`#include` groups)
- [ ] One public header per module under `<skalops/...>`
>>>>>>> 157367e (fixup! skl -> skalaps; chan -> chnl)
- [ ] Every public function has a one-line `///` comment stating its contract
- [ ] No comments that merely describe what the code does; only non-obvious *why* comments

**Consistency with pillars:**
- If the diff touches a subsystem covered by a pillar doc (`docs/pillars/`), verify the implementation matches the contract and pseudocode in that pillar. Read the relevant pillar(s) if needed.

## Output format

Report findings as a prioritized list:
- **Must fix** — correctness bugs, forbidden pattern violations, ABI breaks
- **Should fix** — convention violations, missing contracts, logic smell
- **Consider** — style nits, suggestions, anything optional

If the diff is clean, say so briefly and explain why.

## Focus

$ARGUMENTS
