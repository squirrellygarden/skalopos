/* arch/aarch64/kernel.ld — kernel linker script for aarch64.
 *
 * Higher-half kernel at virtual 0xffff000000080000; physical entry at 0x80000
 * (standard Pi 4 kernel load address).
 */

OUTPUT_FORMAT(elf64-littleaarch64)
ENTRY(_start)

KERNEL_VIRT_BASE = 0xffff000000080000;
KERNEL_PHYS_BASE = 0x00080000;

SECTIONS {
    . = KERNEL_VIRT_BASE;

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

    /DISCARD/ : { *(.note.*) *(.comment) *(.eh_frame) }
}
