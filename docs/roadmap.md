# Roadmap

Tracked at the version level. Each version is "what's in scope" not a date. The work to land v1 alone is many weeks of full-time effort.

## v1 — "it boots and you can run things"

The goal: in QEMU, on both x86_64 and aarch64, the kernel boots, mounts an initramfs, and drops into `/bin/sh`. From the shell you can run `echo`, `cat`, `ls`, `pwd`, `mkdir`.

Must be working:
- Both arches' boot path (boot.S → kmain → driver inits → spawn PID 1).
- Console driver (UART/serial) on both arches.
- Timer + IRQ controller on both arches.
- Scheduler (round-robin is fine).
- Per-process handle table.
- Channels (kernel-internal and syscall-exposed).
- VFS with three mounts: initramfs (/), tmpfs (/tmp), devfs (/dev).
- Path resolution with `*_at` family and `AT_BENEATH`/`AT_NOFOLLOW`.
- `proc_spawn`, `proc_exit`, `proc_wait`.
- `vm_alloc`, `vm_unmap`, `vm_protect`. DEP enforced. ASLR enabled.
- `file_read`, `file_write`, `file_seek`. Synchronous blocking only.
- `dev_op` with at least the console class.
- Codegen for syscalls and status codes from schemas.
- libc-core: ~15 functions, bump allocator, CRT.
- /bin/sh and the five v1 utilities.

Explicitly not in v1:
- Persistent storage (no block device).
- Async I/O.
- Threads (yes really — the `thread_create` syscall can wait until something needs it).
- Service supervision (PID 1 is the shell).
- Privilege checks.
- Networking.
- USB, sound, framebuffer.
- A real allocator.

## v2 — "it has disks and a service manager"

- virtio-blk driver (works in QEMU).
- A simple on-disk FS ("skfs"). Tree-of-extents inodes; no journaling.
- `fs_mount` from a block device.
- Reaper-style PID 1 (~300 LOC). Separate `/sbin/svc-manager` process.
- Service config format (decide: TOML vs. s-expressions; YAML noted as a possibility).
- `skctl` for talking to the service manager.
- Async I/O variants: `file_read_async`, `file_write_async`, `op_cancel`, completion-on-channel.
- Privilege boolean (set by PID 1 at spawn). `fs_mount` and `system_shutdown` gated.
- `system_shutdown(mode)` syscall.
- A real `malloc` (slab/freelist, or a port of dlmalloc-style code).
- Threads (`thread_create`/`thread_join`/`thread_exit`) actually implemented.
- A QEMU-based test runner: `just test` boots, runs `/bin/test-runner`, exits with status.
- Optional: stdio `FILE*` layer, `getenv`/`setenv`.

## v3+ — open directions

None committed. Listed in rough order of plausibility:

- **A second shell.** Possibly lisp-flavored for scripting. Possibly POSIX-ish.
- **/proc-equivalent.** Read-only synthetic FS exposing process and handle introspection.
- **Typed authority handles.** Replace the v2 privilege boolean with `MOUNT_AUTHORITY`, `SYSTEM_AUTHORITY`, etc. Pre-stages a capability story without committing.
- **Submission rings on top of async I/O.** Pure io_uring shape — shared memory submission, completion via channel.
- **Networking.** virtio-net + a minimal TCP/IP stack. Likely a project the size of the rest of the OS.
- **Loadable kernel modules.** Only if there's a real reason.
- **Real-hardware Pi 4 boot.** Currently a side effect of aarch64 support; full bring-up (SD card boot, mailbox, firmware-provided peripherals) is its own project.
- **Real x86 boot.** Multiboot2 + GRUB on bare metal.
- **Self-hosting toolchain.** Far future. Probably never. But explicit aspiration.

## Cross-cutting work (any version)

- More device drivers (`/dev/null`, `/dev/zero`, `/dev/random`).
- More utilities.
- Documentation as the system grows.
- Stress and fuzz testing once the syscall surface is real.
