/* arch/x86_64/boot.S — x86_64 boot entry.
 *
 * Entered from the bootloader (QEMU -kernel direct boot or GRUB multiboot2).
 * Responsibilities, in order:
 *   1. Establish a known stack.
 *   2. Get into 64-bit long mode (if not already).
 *   3. Set up minimum page tables to reach the higher-half kernel base.
 *   4. Clear BSS.
 *   5. Build a boot_info_t on the stack/heap from the multiboot info pointer.
 *   6. Call kmain(boot_info).
 *
 * This file is a stub. The full implementation is roughly ~150 lines of asm
 * plus a small multiboot2 header at the top.
 */

.section .multiboot_header, "a"
    /* TODO: emit multiboot2 magic, architecture, length, checksum, end tag. */

.section .bss
    .align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
_start:
    /* TODO: real boot sequence. For now, halt. */
    cli
    hlt
1:  jmp 1b
