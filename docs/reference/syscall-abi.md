# Syscall ABI

How userland actually invokes a syscall on each architecture. The libc wrappers in `userland/libc/src/sys/` hide these details; this document is the calling convention they assume.

## Both architectures

- Syscall number lives in a fixed register.
- Up to 6 register-passed arguments.
- A trap instruction transfers control to the kernel.
- The kernel returns a `status_t` in the primary return register. For syscalls with an additional scalar return (rare; most use out-parameters), it goes in a second register.
- All other registers are preserved across the syscall (the kernel saves the user context on trap entry).

## x86_64

| Role | Register |
|---|---|
| Syscall number | `rax` |
| Arg 1 | `rdi` |
| Arg 2 | `rsi` |
| Arg 3 | `rdx` |
| Arg 4 | `r10` |
| Arg 5 | `r8` |
| Arg 6 | `r9` |
| Status return | `rax` |
| Second return | `rdx` (if any) |

Trap instruction: **`syscall`** (uses MSRs `STAR`, `LSTAR`, `FMASK`). The kernel installs the trap entry at boot.

Notes:
- Argument 4 is `r10`, not `rcx`, because `syscall` clobbers `rcx` (saves user RIP). This matches the Linux convention so any code review by analogy works.
- The kernel saves the full user `gs` / `fs` state and uses `swapgs` on entry/exit to access per-CPU kernel data.

## aarch64

| Role | Register |
|---|---|
| Syscall number | `x8` |
| Args 1–6 | `x0`–`x5` |
| Status return | `x0` |
| Second return | `x1` (if any) |

Trap instruction: **`svc #0`**. Handler is installed in the EL1 vector table.

Notes:
- This follows the Linux/AArch64 syscall ABI exactly. The kernel's exception vector handles `svc` from EL0 and routes to `syscall_dispatch`.
- AAPCS64 reserves `x18` as the platform register; Skalapos's kernel uses it for the current-thread pointer (`tpidr_el1` on the kernel side; userland gets `tpidr_el0`).

## Returning a status

Every syscall returns a `status_t` (`int32_t`). The C ABI on both arches sign-extends a 32-bit return into the full 64-bit return register, which is fine — `status_t` is treated as 64-bit zero-extended-ish on the wire.

`STATUS_OK == 0` always. Callers test `if (status != STATUS_OK)`.

## Out-parameter convention

Most syscalls that produce a value write it through a caller-supplied pointer. This keeps the calling convention uniform (one return register for status, no special-casing). For example:

```c
status_t vm_alloc(size_t len, uint32_t prot, void** out_base);
```

`out_base` is a userland pointer; the kernel copies the result to that address after validating it lies inside the calling process's address space. If `out_base` is invalid, the syscall returns `STATUS_INVALID_ARG` without performing the allocation.

## Buffer-pointer validation

For every syscall that takes a userland pointer, the kernel:

1. Confirms the pointer is non-NULL (or NULL is explicitly allowed for that arg).
2. Confirms the range `[ptr, ptr+len)` lies entirely in the calling process's user address space.
3. Performs the access via the kernel-side `copy_from_user` / `copy_to_user` helpers, which trap user-page faults and convert them to `STATUS_INVALID_ARG`.

There are no shortcuts where the kernel dereferences user pointers directly. Every access goes through the helpers.

## Compiler-generated stubs

`tools/gen_syscalls.py` emits one wrapper per syscall in `userland/libc/src/sys/syscall_generated.c`. Each wrapper:

1. Loads the syscall number into the canonical register.
2. Loads arguments into the canonical registers.
3. Issues the trap.
4. Returns the status.

Hand-writing syscall wrappers in libc is **forbidden**; the generated wrappers are the only legal call path.
