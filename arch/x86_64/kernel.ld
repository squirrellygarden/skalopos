/* arch/x86_64/kernel.ld — kernel linker script for x86_64.
 *
 * The kernel runs in the higher half. Physical base 0x100000 (1 MiB); virtual
 * base 0xffffffff80100000. Early boot.S identity-maps low memory and walks
 * paging up so kmain executes at the virtual base.
 *
 * This file is a sketch. Fill in once boot.S is fleshed out.
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

KERNEL_VIRT_BASE = 0xffffffff80100000;
KERNEL_PHYS_BASE = 0x100000;

SECTIONS {
    . = KERNEL_VIRT_BASE;

    .multiboot_header : AT(KERNEL_PHYS_BASE) {
        KEEP(*(.multiboot_header))
    }

    .text : AT(ADDR(.text) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE) {
        *(.text*)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE) {
        *(.rodata*)
    }

    .drivers : AT(ADDR(.drivers) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE) {
        __drivers_start = .;
        KEEP(*(.drivers))
        __drivers_end = .;
    }

    .data : AT(ADDR(.data) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE) {
        *(.data*)
    }

    .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE) {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    /DISCARD/ : { *(.eh_frame) *(.note.*) *(.comment) }
}
