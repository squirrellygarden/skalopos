# CLAUDE.md — arch/

Architecture-specific kernel code lives here. There are exactly two arches in Skalapos: `x86_64` and `aarch64`. Adding a third requires a design discussion.

## Layout per arch

```
arch/<arch>/
  boot.S            very-early assembly entry (called by bootloader; sets stack, jumps to C)
  kernel.ld         linker script defining VA layout, section ordering, BSS markers
  arch.toml         per-arch compile flags consumed by tools/gen_build.py
  cpu.c             CPU init, IDT/exception vectors, MSR/MMU setup
  context.S         thread context-save / context-restore primitives
  paging.c          page-table construction and walking for this arch
  syscall_entry.S   trap handler that lands at syscall_dispatch()
  irq.c             IRQ controller bring-up (IO-APIC / GIC)
  timer.c           timer driver (HPET/PIT for x86; ARM generic timer for aarch64)
  uart.c            console driver (16550 COM1 / VGA text for x86; PL011 for aarch64)
  README.md         arch-specific quirks
```

The boundary: anything that touches a specific MSR, page-table format, exception number, or instruction goes in `arch/`. Anything portable lives in `kernel/`.

## arch.toml shape

```toml
[arch]
triple = "x86_64-elf"

[compile]
cflags    = ["-mno-red-zone", "-mcmodel=kernel", "-mno-sse", "-mno-mmx"]
asflags   = []
ldscript  = "arch/x86_64/kernel.ld"

[link]
kernel_base = "0xffffffff80100000"
```

`gen_build.py` reads this and folds it into the emitted ninja rules.

## Boot handoff conventions

### x86_64

- Bootloader: QEMU `-kernel` for dev. Real hardware uses GRUB with a multiboot1 or multiboot2 header in the kernel image.
- On entry: protected mode (multiboot1) or long mode (multiboot2 + EFI); paging may or may not be on. `boot.S` is responsible for getting into 64-bit long mode with a known stack and known paging setup, then `call kmain`.
- Multiboot info struct pointer is passed per ABI; `boot.S` saves it for later VFS init (initramfs lives in modules).

### aarch64 (Pi 4 model)

- Bootloader: QEMU `-machine raspi4b` direct boot, or U-Boot on real hardware.
- On entry: EL2 (typically). The kernel drops to EL1 in `boot.S`, sets up the initial stack, MMU off, jumps to `kmain`.
- `x0` carries the DTB physical address per the Linux/ARM convention.

## Endianness

Both arches are little-endian. No big-endian support is planned.

## ABI / ELF

Both arches use System V ELF64. Kernel image is statically linked, never relocatable at load. Userland is PIC (for ASLR) and the kernel loader applies relocations at spawn time.

## Don't

- Don't add a third arch without discussing scope. Even RISC-V is a significant lift; it isn't free just because the kernel is portable.
- Don't sprinkle `#ifdef __x86_64__` through portable kernel code. If a piece of logic differs per arch, give it an `arch_*` function declared in `kernel/<subsystem>/arch.h` and implement it in each `arch/<arch>/<subsystem>.c`.
- Don't assume identity mapping. The kernel runs in the higher half on both arches; the bottom half is for userland.
