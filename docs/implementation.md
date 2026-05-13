# Implementation order — v1

This is the suggested build order for v1. The [roadmap](roadmap.md) says *what* v1 contains; this document says *which order to build it in* so that early weeks feel like progress instead of like staring at a hex dump wondering why the screen is blank.

**Core rule, enforced at every step: you must be able to see what just broke.** Hours spent now making things inspectable are hours not spent later debugging a silent triple-fault.

## Phase 0 — Foundations (≈1 week)

Not kernel logic yet. Make the iteration loop fast.

1. **Toolchain + dev container working end-to-end.** Either `crosstool-ng` produces both `x86_64-elf-gcc` and `aarch64-elf-gcc` locally, or the [Dockerfile](../ci/docker/Dockerfile) does. QEMU 8+ installed. Finish this before writing any C.
2. **`gen_build.py` actually emits a working `build.ninja`.** Compiles and links an empty `kmain.c` into `kernel.elf` for both arches. Doesn't need to be pretty.
3. **`gen_syscalls.py` finishes write mode.** The schemas already have starter entries; codegen should produce the kernel dispatch table stub and the libc syscall wrappers. Even when both ends are empty, this proves the codegen path works.
4. **🎯 QEMU boots an empty kernel and you can see it.** `kmain` writes `'A'` to the serial UART MMIO directly (hardcoded address, no driver yet), then halts. You see `A` in the terminal. **Do not move on until this works on both arches.**

> The trap most kernel projects fall into: build a beautiful scheduler in the abstract, then try to boot it and stare at a black screen for two weeks. The "see one character" milestone is the single best thing you can do for sanity.

## Phase 1 — Early kernel infrastructure (≈1-2 weeks)

Things kernel code itself needs. Userland is far away.

5. **Real console driver + `kprintf`.** Built on the UART/serial MMIO. **This unlocks every subsequent phase** — every other piece of code becomes inspectable. Spend time on this; it pays back constantly.
6. **Paging / MMU brought up correctly.** Identity-map the low half, set up higher-half kernel mapping, switch to it, `kprintf("higher half\n")`.
   - x86_64: GDT + page tables + long mode (you may have done some in [boot.S](../arch/x86_64/boot.S)).
   - aarch64: drop to EL1 + TTBR0/TTBR1 + MMU enable.
   - **Hardest debugging task in v1** — fault recovery doesn't work yet, so a wrong page table = silent reboot. Slow down.
7. **Physical-frame allocator + `kmalloc`.** Bitmap allocator on the physical side; bump or simple slab `kmalloc` for kernel objects. Now the kernel can dynamically allocate.
8. **IDT / exception vectors + IRQ controller.**
   - x86_64: IDT, IO-APIC.
   - aarch64: `VBAR_EL1` vector table, GIC-400.
   - Wire each exception (page fault, illegal instruction, GP fault) to a stub that `kprintf`s the trap kind + registers, then halts. **Most "silent reboot" causes become visible here.**
9. **Timer driver + scheduler tick.** ARM generic timer or HPET/PIT. Tick just `kprintf("tick\n")` once a second. Confirms IRQs arrive.

## Phase 2 — Process and thread infrastructure (≈2 weeks)

The scheduler and the core process model. No userland yet.

10. **Structures only:** [`struct thread`](../kernel/sched/sched.h), `struct process`, [handle table](../kernel/handle/handle.h), runqueue. Nothing scheduled yet.
11. **Context switch primitive.** `arch_context_switch(prev, next)` in assembly per arch. The most "OS-textbook" piece of code in v1 — save callee-saved regs, swap stacks, restore. Test with two kernel threads printing alternating letters and yielding manually.
12. **Round-robin scheduler.** `sched_add`, `sched_yield`, timer-driven `sched_tick`. Two kernel threads now alternate automatically.
13. **`wait_list` primitives** ([sched/wait.h](../kernel/sched/wait.h)). `scheduler_block`, `wait_list_wake_*`. Test with a producer/consumer kernel-thread pair.
14. **Channels** ([channel/channel.h](../kernel/channel/channel.h)). First real syscall-shaped thing, kernel-internal for now. Test: kernel-thread producer sends, kernel-thread consumer receives.
15. **🎯 User-mode execution.** Set up a user-mode page mapping, drop privilege, execute a hand-assembled "syscall+halt" stub. Kernel takes the trap, dispatches, returns. **Kernel ↔ user separation is real.**

## Phase 3 — Syscall surface and VFS (≈2-3 weeks)

Now the kernel ABI starts looking like a real OS.

16. **Syscall trap entry per arch.** Register-arg loading, status return, `copy_from_user`/`copy_to_user` with fault recovery (bad user pointer → `STATUS_INVALID_ARG`). Hook to the codegen'd dispatch table.
17. **First real syscalls:** `handle_close`, `handle_dup`, `vm_alloc`, `vm_unmap`, `vm_protect`, `proc_exit`. Trivial; exercises the dispatcher.
18. **🎯 `proc_spawn`.** ELF loader for both arches; new address space + handle table + stack setup. **Big milestone.** Demo by spawning a hand-built "exit 42" ELF blob linked into the kernel image. Watch it run, see exit code.
19. **VFS skeleton + initramfs + devfs + tmpfs.** Mount registration, `struct dir`/`struct file`, path resolution with `*_at` and `AT_BENEATH`/`AT_NOFOLLOW`. CPIO parser for the initramfs blob.
20. **`open_at`, `file_read`, `file_write`, `file_seek`.** Including dev-class dispatch through to the console driver for `/dev/console`.
21. **`dev_op` syscall + console ops.** Only the ops you actually need in v1 (probably none — defer most until a utility demands them).
22. **`chan_create`, `chan_send`, `chan_recv` exposed as syscalls.** Test with two spawned processes talking.

## Phase 4 — Userland (≈1 week)

Surprisingly fast once the kernel is done.

23. **libc skeleton + CRT.** `crt0.S` per arch, codegen'd syscall wrappers, the ~15 libc functions, bump allocator. Userland build infrastructure.
24. **`/bin/echo`.** Smallest possible userland program. If `echo hello` works, libc + syscall paths are sound.
25. **`/bin/cat`.** Exercises `open_at` + read/write loops.
26. **`/bin/sh`.** Read line, tokenize, search PATH, spawn, wait.
27. **`/bin/ls`, `/bin/pwd`, `/bin/mkdir`.** Round out the v1 set.
28. **🎯 PID 1 wiring.** Kernel `init=/bin/sh` actually spawns the shell with the hardcoded handle layout. Boot, type `echo hello`, see `hello`. **v1 ships.**

## Rhythm

- **Every phase ends with a demoable milestone** (marked 🎯). Phase 0 = one character. Phase 1 = `kprintf("tick")`. Phase 2 = user-mode trap-and-return. Phase 3 = "exit 42" userland process. Phase 4 = shell prompt.
- **Pick one architecture as primary, port the other at each milestone.** Recommend **x86_64 as primary** — QEMU's x86 support is most mature, debugging surface richer (GDB, `-d int`, isa-debug-exit). At each milestone: x86 first, then port to aarch64 before moving on. Lockstep at every feature is too much cognitive load; lockstep at every milestone keeps the arch-specific code from rotting.
- **Don't optimize anything in v1.** Round-robin, bump allocator, linear directory scan, no caching. Every optimization is v2+. Slow is fine; correct is everything.
- **Boot it constantly.** After every meaningful change. The build/QEMU loop is fast; use it. Compounding debugging is the enemy.
- **Bring up debugging tools before you need them.** `just gdb x86_64` is essential. QEMU's `-d int,cpu_reset,guest_errors` flags pay for themselves the first triple-fault.

## Don't (during v1)

- **Don't implement threads** (`thread_create`/`thread_join`) until a test program actually needs them. Single-threaded processes are enough for the v1 demo.
- **Don't fully implement `proc_chroot` or `AT_BENEATH`** until something tests them. Flag is defined; implementation can return `STATUS_NOT_IMPLEMENTED` and be filled in later.
- **Don't write the userland fault handler** (per-thread handler from [pillar 4](pillars/04-events.md)). Faults can just kill the process via the control-channel default path. User-installable handler wires up when needed.
- **Don't make the v1 shell handle pipes, redirections, or quoting.** It's `command arg arg`. Period.
- **Don't build the v2 test runner.** Hand-test interactively. CI is for projects with users.

## Rough time estimate

For full-time work without getting hopelessly stuck:

| Phase | Weeks |
|---|---|
| 0: Foundations | 1 |
| 1: Early kernel infra | 1-2 |
| 2: Process/thread infra | 2 |
| 3: Syscall surface + VFS | 2-3 |
| 4: Userland + integration | 1 |
| **Total v1** | **7-9 weeks** |

In practice, evenings/weekends: 3-4 months. Big unknowns are paging bring-up (1-3 weeks) and ELF loading + the first successful `proc_spawn` (the moment user-mode becomes real is also when a lot of subtle bugs surface together).

## When you finish v1

You boot, you see a shell, you can `echo hello`, you can `cat /etc/something`, you can `mkdir /tmp/x && ls /tmp`. That's the deliverable. Stop. Tag the commit. Take a break.

Then either go to [v2](roadmap.md) (block device + skfs + service manager + async I/O) or revisit something in v1 you cut corners on. Both are good choices.
