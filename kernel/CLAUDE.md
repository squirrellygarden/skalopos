# CLAUDE.md — kernel/

You are working inside the Skalapos kernel. The top-level [`../CLAUDE.md`](../CLAUDE.md) lists the anti-patterns and overall posture; this file is kernel-specific.

## Layout

```
kernel/
  boot/        cross-arch boot orchestration (after arch/<arch>/boot.S jumps here)
  mem/         physical-frame allocator, page-table glue, kmalloc, address-space management
  sched/       run queue, context switch glue, thread/process structs
  handle/      handle table per process, handle types, refcounting
  channel/     bounded MPMC channels with handle-passing support
  vfs/         VFS layer; FS implementations (initramfs, tmpfs, devfs) live as siblings
  driver/      driver-registration, devfs publication, device-class enums
  trap/        IDT/exception vector glue, fault routing to handlers or control channels
  syscall/     trap entry, argument validation, generated dispatch table
```

Each subdirectory has its own short CLAUDE.md (or will) describing local invariants. Read it before adding code there.

## Kernel build flags

The kernel is compiled with:
`-ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector -fno-pic -fno-pie -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror -std=c11`

Plus per-arch flags from [`../arch/<arch>/arch.toml`](../arch/x86_64/arch.toml).

There is no libc in the kernel. The few string/memory primitives we need live in `kernel/lib/` (to be created when first needed) and are NOT shared with `userland/libc/`. Do not `#include` userland headers from kernel sources.

## Hard rules

1. **No floating-point or SIMD in kernel code.** Compile flags forbid them; the context switch does not save FP/SIMD state. If you find yourself reaching for floats, you've taken a wrong turn.
2. **No allocations in interrupt context.** IRQ handlers may not call `kmalloc`. Use preallocated buffers or per-CPU pools.
3. **No blocking syscalls inside the kernel itself.** Kernel code runs to completion or sleeps via the scheduler primitives; it never "blocks on a channel."
4. **Status is the only error return type from kernel-internal functions that can fail.** No `errno`, no negative pointer tricks, no setting a per-thread flag.
5. **Handles are never bare pointers.** A `handle_t` only makes sense relative to a specific process's handle table. Internal kernel code uses `struct file*`, `struct channel*`, etc., directly.
6. **v1 kernel is non-preemptive and single-CPU.** No locking in kernel data structures *except* for synchronization between IRQ handlers and the synchronous kernel path (use IRQ disable/enable critical sections). Adding mutex/spinlock infrastructure now is wrong — it'll be designed properly during the v2 SMP pass. See [`../docs/pillars/12-scheduling.md`](../docs/pillars/12-scheduling.md).
7. **Blocking always goes through a `wait_list`.** Every blocking syscall follows: acquire the relevant IRQ-disabled critical section, check the condition in a loop, `wait_list_add` + `scheduler_block` if false, re-check on wake. Never invent a one-off blocking mechanism.

## How to add a syscall

1. Add an entry to [`../schemas/syscalls.toml`](../schemas/syscalls.toml).
2. Run `just codegen`. This regenerates `kernel/syscall/dispatch_generated.c` and `kernel/syscall/numbers_generated.h`.
3. Implement the handler in the appropriate subdirectory (e.g., `kernel/vfs/open_at.c` for `open_at`). The handler's signature is determined by the schema entry; the generated dispatcher does argument validation and copy-in/copy-out from user memory.
4. Update the relevant pillar doc in [`../docs/pillars/`](../docs/pillars/).

Do not hand-edit `dispatch_generated.c` or `numbers_generated.h` — they are overwritten.

## How to add a driver

See [`../docs/pillars/08-drivers.md`](../docs/pillars/08-drivers.md). In brief:

1. Place the driver under `kernel/driver/<name>/`.
2. Implement a `static const driver_ops_t` with the appropriate `device_class`.
3. Write a small init function and tag it with `DRIVER_INIT(your_init_fn)`. This puts a function pointer in the `.drivers` section; the kernel iterates that section at boot.
4. Inside init: probe hardware, hook IRQs, allocate a `struct device`, and `devfs_publish` it under a name that will appear in `/dev`.

Drivers do NOT touch the VFS directly. They publish to devfs, which is itself a VFS implementation.

## Tests

There are no v1 in-kernel tests as such. Kernel correctness is validated by:

- Booting in QEMU and getting a shell prompt.
- Running the v1 utilities (`echo`, `cat`, `ls`, `pwd`, `mkdir`).
- Manual stress (spawn many processes, write through `/dev/console`, etc.).

v2 plans for a QEMU-based test harness; see [`../docs/pillars/10-build.md`](../docs/pillars/10-build.md).

## When in doubt

If a kernel change appears to require touching the ABI (new syscall, new status code, new handle type), surface the change as a *schema edit + pillar-doc edit* first, then implement. Don't slip ABI changes in through implementation files.
