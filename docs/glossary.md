# Glossary

Terms used throughout the Skalapos docs and source.

| Term | Meaning |
|---|---|
| **`AT_BENEATH`** | Resolve-flag on `*_at` syscalls: path resolution must stay within the supplied `dir_handle`'s subtree. Blocks `..` escape and absolute symlinks. |
| **`AT_NOFOLLOW`** | Resolve-flag: the final path component must not be a symlink. |
| **`AT_NOSYMLINKS`** | Resolve-flag: no symlink may appear anywhere in the resolution. |
| **bump allocator** | The v1 userland `malloc`. Advances a cursor in a fixed BSS arena. `free` is a no-op. Leaks; intentional for v1. |
| **channel** | A bounded MPMC kernel-managed queue of typed messages. Handles of type `H_CHAN`. The primary async-event mechanism. |
| **control channel** | A specific channel held by every process. The kernel posts lifecycle messages (child exit, fault, kill request) to it. |
| **CPIO** | Old Unix archive format. v1's initramfs is a newc-format CPIO blob handed to the kernel by the bootloader. |
| **DEP** | Data Execution Prevention — Skalapos enforces by rejecting `W|X` page protections. |
| **dev_op** | Single syscall for structured device operations. Replaces `ioctl`. Op codes namespaced per device class. |
| **device class** | Closed-set enum (`DEV_CONSOLE`, `DEV_BLOCK`, etc.) that subtypes a `File` handle and gates valid `dev_op` codes. |
| **devfs** | In-kernel synthetic FS that publishes device handles under `/dev`. Each driver `devfs_publish`es its devices at init. |
| **DTB / FDT** | Device tree blob. On aarch64, the bootloader passes a DTB describing on-chip peripherals; the kernel walks it to find devices. |
| **freestanding** | Compiler mode (`-ffreestanding`) where no hosted libc or runtime is assumed. The kernel and Skalapos userland are both freestanding. |
| **handle** | A per-process integer referring to a kernel object. Carries a type tag (and subtype, for files) on the kernel side. Replaces POSIX file descriptors. |
| **handle table** | Per-process array indexed by `handle_t`. The kernel translates handles to objects via this table. |
| **HRNG** | Hardware random number generator. Used to seed the kernel's ASLR PRNG at boot. |
| **initramfs** | CPIO blob handed in by the bootloader; mounted as `/` in v1. Read-only in practice (in-memory FS). |
| **multiboot** | Bootloader protocol (versions 1 and 2). x86_64 Skalapos kernel embeds a multiboot header so GRUB / QEMU's `-kernel` can load it. |
| **PID 1** | The first userland process. The kernel constructs it directly because it has no parent. In v1: `/bin/sh`. In v2: a reaper supervisor. |
| **PL011** | The UART on the Raspberry Pi 4 (and most ARM SoCs). Used as the console on aarch64. |
| **POSIX wart** | Anything in POSIX that Skalapos is replacing rather than keeping. The pillars enumerate them. |
| **proc_chroot** | Skalapos's opt-in sandbox primitive. Replaces a process's ROOT_DIR with a given Directory handle. Irreversible. |
| **rights bits** | A `uint32_t` reserved field on each handle entry. Zero in v1; ignored. Pre-staged for a future capability-lite model. |
| **ROOT_DIR / CWD_DIR** | Specific handle slots in every process. libc uses them to translate POSIX-style absolute and relative path strings into `*_at` syscall calls. |
| **schema-driven** | The codegen pattern: syscall and status definitions live in `schemas/*.toml` and are translated into kernel and userland C by `tools/gen_syscalls.py`. |
| **ShmHandle** | The unified shared-memory primitive. Replaces System V shm, POSIX shm_open, and `MAP_SHARED|MAP_ANONYMOUS`-via-fork. |
| **spawn** | The single process-creation primitive. Takes an image handle, args, env, an explicit list of handles to pass to the child, and an options struct. |
| **status** | The typed error code returned by every syscall. `STATUS_OK == 0`; non-zero values are failure. High 8 bits identify the subsystem. |
| **TOCTOU** | Time-of-check / time-of-use race. Skalapos mitigates by preferring handle-based access (`fstat` over `stat`) and by `AT_BENEATH`. |
| **typed handle** | A handle whose kernel-side entry carries a type (and optionally subtype) tag. Lets the syscall edge reject wrong-typed access. |
| **virtio-blk** | A QEMU-friendly paravirtualized block device. Planned as v2's first real block driver. |
| **W^X** | "Write XOR Execute". DEP policy. Rejected at `vm_alloc` and `vm_protect`. |
| **wait list** | The per-object kernel data structure tracking which threads are blocked waiting for an event on that object. Embedded directly in channels, processes, timers, etc. See pillar 12. |
| **runqueue** | The list of `THREAD_RUNNABLE` threads the scheduler picks from. v1: one global list, round-robin. v2: per-CPU lists with work stealing. |
| **time slice** | The maximum time a thread runs before the scheduler considers preempting it. v1: 10 ms, fixed. |
| **MLFQ** | Multi-level feedback queue. The planned v2.2+ replacement for round-robin scheduling. |
| **BSP** | Bootstrap processor. The CPU the kernel boots on. v1 ignores all other cores. |
| **skfs** | Skalapos's v2 on-disk filesystem. Roughly "ext2 with sane modern choices, hex-readable by design." See pillar 13. |
| **inode** | The fixed-size per-file metadata record in skfs. 256 bytes; lives in the inode table region. |
| **mkfs** | The host tool (or eventual guest tool) that formats a block device into a skfs filesystem. Tool sets the journal size, inode count, etc. |
| **journal (skfs)** | A v2.1 ring-buffer region of the disk where metadata writes are recorded before being applied to their actual locations. Enables crash-safe metadata. |
| **xattr** | Extended attribute. Skfs reserves a block per inode to hold name/value attribute pairs. v2 stores them; consumers come later. |
