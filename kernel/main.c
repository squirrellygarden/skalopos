// kernel/main.c — kernel entry point.
//
// Called by arch/<arch>/boot.S after early CPU setup (long mode on x86,
// EL1 on aarch64), with a stack but with paging and MMU set up only to the
// minimum needed to reach this function.
//
// Sequence (planned):
//   1. Initialize early console (so kprintf works).
//   2. Bring paging fully online; switch to the kernel's final page tables.
//   3. Initialize the physical-frame allocator and the kernel heap (kmalloc).
//   4. Install IDT / exception vectors.
//   5. Initialize the IRQ controller and timer driver.
//   6. Discover devices (ACPI/PCI on x86, FDT walk on aarch64).
//   7. Iterate registered drivers (via .drivers ELF section); call each init.
//   8. Initialize VFS; register initramfs/tmpfs/devfs; mount initramfs at /,
//      tmpfs at /tmp, devfs at /dev (the latter is populated by driver inits
//      already).
//   9. Parse kernel command line for init=<path>.
//  10. Spawn PID 1 with the hardcoded init handle layout (see
//      docs/pillars/09-init.md).
//  11. Enter the idle loop; the scheduler takes it from there.

#include "boot/boot.h"   // boot_info_t (TBD)

void kmain(const boot_info_t* boot_info) {
    (void)boot_info;
    // TODO: implement the sequence above.
    for (;;) { /* halt */ }
}
