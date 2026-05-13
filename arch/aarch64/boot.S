/* arch/aarch64/boot.S — aarch64 boot entry (Raspberry Pi 4 model).
 *
 * Entered from QEMU's raspi4b boot or U-Boot. By the standard convention:
 *   - We may be at EL2 (typical for QEMU's machine type); drop to EL1.
 *   - x0 holds the physical address of the DTB.
 *   - MMU is off.
 *
 * Responsibilities:
 *   1. Establish a known stack.
 *   2. Drop from EL2 to EL1 if needed.
 *   3. Set up TTBR0/TTBR1 with a minimum page-table sufficient to reach the
 *      kernel's higher-half virtual base.
 *   4. Enable the MMU.
 *   5. Clear BSS.
 *   6. Build boot_info_t (initramfs region from DTB, dtb_phys = x0).
 *   7. Call kmain(boot_info).
 *
 * This file is a stub. Real implementation is ~200 lines of asm.
 */

.section .bss
    .align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
_start:
    /* TODO: real boot sequence. For now, wait-for-event in a loop. */
1:  wfe
    b 1b
