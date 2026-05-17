# Pillar 10 — Build, toolchain, and dev loop

## Goals

- Cross-compile + QEMU from day one; both ARM64 and x86_64 are first-class.
- The daily inner loop (edit → build → run in QEMU) is fast and unsurprising.
- The build is inspectable: when something goes wrong, you can read the actual commands being run.
- No fight with autoconf, no CMake DSL, no Makefile recursion.

## Toolchain

**GCC freestanding cross-toolchain.** Two targets:

- `x86_64-elf-gcc` (with matching `x86_64-elf-binutils`)
- `aarch64-elf-gcc` (with matching `aarch64-elf-binutils`)

Bare metal. Distro packages like `gcc-x86-64-linux-gnu` target Linux ABI and will fight you; do not use them.

Two paths to install:

1. **Prebuilt:** the [bootlin toolchains](https://toolchains.bootlin.com/) page (or similar) has prebuilt `*-elf` toolchains. Download, extract, add `bin/` to `$PATH`.
2. **From source via `crosstool-ng`:** `ct-ng x86_64-elf` and `ct-ng aarch64-elf`, build each. ~30 minutes per target on a fast machine, one-time.

The dev container ([`ci/docker/Dockerfile`](../../ci/docker/Dockerfile)) installs both via prebuilt downloads. Use that if you don't want to mess with the host toolchain.

The build system reads `$(CC_X86_64)` and `$(CC_AARCH64)` (defaults: `x86_64-elf-gcc`, `aarch64-elf-gcc`). Setting them at invocation time (`CC_X86_64=clang just build x86_64`) is supported and will not fight you — Skalapos source uses no GCC-only features that Clang lacks. The default is GCC for inline-asm diagnostic quality and OSDev folklore alignment.

## Build system

Skalapos uses **Python-generated Ninja files driven by a Justfile.**

- `tools/gen_build.py` walks the source tree and emits `build/<arch>/build.ninja`. Knows: per-arch compile flags, linker script paths, kernel-vs-userland split, initramfs CPIO pack step.
- `justfile` holds user-facing commands. Each recipe makes sure `build.ninja` is current, then invokes `ninja`, then runs QEMU/GDB as appropriate.
- `build.ninja` is generated, gitignored, and human-readable for debugging.

### Common commands

```
just build x86_64           # build kernel.elf and initramfs.cpio for x86_64
just build aarch64

just qemu x86_64            # build and run in QEMU
just qemu aarch64

just debug x86_64           # build with -O0 -g; run QEMU with -s -S (GDB stub waits for connection)
just gdb x86_64             # attach gdb-multiarch to the running stub

just clean
just clean-all              # also remove the cross-compile sysroot and downloaded toolchains
just codegen                # regenerate sources from schemas/
```

### Compile flags (canonical set)

Kernel:
```
-ffreestanding -nostdlib -nostartfiles
-fno-builtin -fno-stack-protector -fno-pic -fno-pie
-fno-omit-frame-pointer
-Wall -Wextra -Wstrict-prototypes -Wpedantic -Werror
-std=c11
-mno-red-zone                                       # x86_64 only
-mgeneral-regs-only                                 # aarch64 only (no FP/NEON in kernel)
-O2                                                 # -O0 -g for debug builds
```

Userland: same minus `-fno-pic -fno-pie` (userland is position-independent for ASLR). Plus `-fno-stack-protector` is kept because libc doesn't ship `__stack_chk_*` symbols in v1.

### Linker scripts

Per-arch, in `arch/<arch>/kernel.ld`. Define:

- Kernel base address (higher-half: `0xffffffff80100000` on x86_64; `0xffff000000080000` on aarch64).
- Section ordering: `.text`, `.rodata`, `.data`, `.bss`, with explicit alignment.
- `__bss_start`/`__bss_end` markers for the kernel's BSS clearer.
- A `.drivers` section accumulating `DRIVER_INIT` entries so the kernel can iterate registered drivers without a manual list.

Userland binaries use the toolchain's default linker layout; the kernel's loader honors standard ELF PT_LOAD segments.

## QEMU invocations

### x86_64

```
qemu-system-x86_64 \
    -m 512M \
    -kernel build/x86_64/kernel.elf \
    -initrd build/x86_64/initramfs.cpio \
    -append "init=/bin/sh" \
    -nographic \
    -serial mon:stdio \
    -no-reboot \
    -d guest_errors    # plus -d int for IDT debug
```

KVM acceleration optional via `-enable-kvm -cpu host`; not default.

### aarch64 (Pi 4)

```
qemu-system-aarch64 \
    -M raspi4b \
    -m 1G \
    -kernel build/aarch64/kernel.elf \
    -initrd build/aarch64/initramfs.cpio \
    -append "init=/bin/sh" \
    -nographic \
    -serial mon:stdio
```

Requires QEMU 8.0+ for the `raspi4b` machine. Older QEMU versions can fall back to `-M raspi3b` with the appropriate kernel build; document this only if a contributor needs it.

## GDB stub

Add `-s -S` to QEMU's args (`-s` enables the gdbserver on `localhost:1234`; `-S` halts CPU at start).

```
just debug x86_64       # in one terminal — boots QEMU paused
just gdb x86_64         # in another — launches gdb-multiarch with symbols loaded
```

Inside GDB:
```
(gdb) break kmain
(gdb) continue
```

For aarch64 use `gdb-multiarch` set `set architecture aarch64`.

## Optional Docker dev container

[`ci/docker/Dockerfile`](../../ci/docker/Dockerfile) installs:

- Both cross GCC toolchains
- `qemu-system-x86`, `qemu-system-arm`
- `gdb-multiarch`
- `ninja-build`, `just`, `python3`, `make`, `bsdtar` (for CPIO)

Usage:

```
just docker-build           # build the image
just docker-shell           # run an interactive shell inside, repo mounted at /work
# inside the container:
just build x86_64
just qemu x86_64
```

The docker path is optional; if your host has the toolchains, use them directly.

## Repository layout (build-relevant)

```
skalapos/
  justfile                   # user-facing commands
  tools/
    gen_build.py             # emits build/<arch>/build.ninja
    gen_syscalls.py          # emits syscall dispatch table + libc wrappers from schemas/
    mkcpio.c                 # host tool, packs initramfs
  arch/
    x86_64/
      boot.S
      kernel.ld
      arch.toml              # per-arch flags consumed by gen_build.py
    aarch64/
      boot.S
      kernel.ld
      arch.toml
  kernel/                    # arch-independent kernel sources
  userland/
    libc/
    sh/
    utils/echo/  cat/  ls/  pwd/  mkdir/
  initramfs/                 # staging directory; build pulls binaries here, then mkcpio
  schemas/                   # syscalls.toml, status.toml
  docs/
  build/                     # gitignored
    x86_64/   # build.ninja, *.o, kernel.elf, initramfs.cpio
    aarch64/
  ci/
    docker/Dockerfile
```

## Why this over alternatives

- **GNU Make** — widely understood but ugly.
- **CMake** — Learning curve.
- **Clang over GCC** — An option, but prefer GCC assembly assist.

## v2+ direction

- **CI via QEMU.** `just test` boots `init=/bin/test-runner` in QEMU; the test runner exits with a status code surfaced through `isa-debug-exit` (x86) or ARM semihosting; the just recipe maps that to a pass/fail. Not v1 but the hooks are cheap to add.
- **Build provenance.** `build/<arch>/buildinfo.txt` recording compiler version, flags, commit hash.
- **Coverage builds.** `-fprofile-arcs -ftest-coverage` flag set, instrumented userland binaries that report into a kernel-side coverage buffer.
