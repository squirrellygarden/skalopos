---
name: security-review
description: Security review of Skalapos code changes. Invoke with /security-review to audit staged or recent changes, or /security-review [file or area] to focus.
disable-model-invocation: true
allowed-tools: Read Bash
argument-hint: [file, area, or leave blank for git diff]
---

You are performing a security review of changes to **Skalapos**, a toy POSIX-evolution OS in freestanding C (ARM64 + x86_64).

## What to review

If no argument is given, review the current diff:
!`git diff HEAD 2>/dev/null || git diff --cached 2>/dev/null`

If an argument is given, focus on those files or areas but still pull context from git diff.

## How to review

Apply your full general security analysis first — memory safety, integer overflow/underflow, information leaks, privilege escalation paths, input validation at trust boundaries, uninitialized memory, race conditions, and any other security-relevant issue you would flag in any C kernel codebase. Then apply the Skalapos-specific security properties below as additional checks.

## Skalapos-specific security properties

**Memory safety and DEP (pillar 7):**
- No page may be mapped both writable and executable simultaneously — `vm_alloc` and `vm_protect` must reject `W|X`
- ASLR must be enabled from day one; no fixed-address mappings for user allocations
- `vm_alloc` / `vm_map_file` / `vm_map_shared` — check that permissions are validated before mapping
- Verify that kernel and user address spaces are properly separated; no user-controlled pointer used without validation before kernel dereference

**Handle integrity (pillar 1):**
- Handles are typed and kernel-side — verify that handle table lookups check the type tag before use
- No handle value accepted from userland without a type-checked table lookup
- Handle indices must be bounds-checked before table access
- Closed/revoked handles must not be usable (use-after-close)

**Filesystem safety (pillar 5):**
- `*_at` syscalls with `AT_BENEATH` and `AT_NOFOLLOW` — verify these flags are checked in the implementation, not silently ignored
- TOCTOU windows: check for gaps between path resolution and operation
- Symlink traversal: confirm `AT_NOFOLLOW` is honored

**Channel and event safety (pillar 4):**
- Typed channel messages — verify message type is validated on receipt before dispatch
- Per-thread trap handlers — verify they cannot be set to arbitrary kernel addresses
- No async signal handler that can interrupt kernel state inconsistently

**Process isolation (pillar 2):**
- `spawn()` explicit handle list — verify only the listed handles are inherited; no ambient handle leak
- No unintended sharing of address space between processes

**Scope of privilege (pillar 9):**
- v1 has no uid/gid checks — confirm no code is silently assuming privilege by the absence of a check (i.e., no "root-only" path that would be universally accessible)
- No ambient authority beyond what the process was explicitly given handles for

**Input validation:**
- All userland-supplied values (syscall arguments, buffer sizes, handle indices, flag fields) must be validated before use in the kernel
- Lengths and offsets must be checked for overflow before pointer arithmetic

## Output format

Report findings as a prioritized list:
- **Critical** — exploitable: kernel privilege escalation, arbitrary write, information leak to userland, sandbox escape
- **High** — likely exploitable with effort, or invariant violation that breaks a security guarantee
- **Medium** — defense-in-depth gaps, missing validation that isn't immediately exploitable
- **Low / Informational** — hardening suggestions, style issues with security implications

If the diff introduces no security concerns, say so and explain what you checked.

## Focus

$ARGUMENTS
