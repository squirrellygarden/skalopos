// kernel/boot/boot.h — types passed from arch boot code to kmain().
//
// Each arch's boot.S assembles a `boot_info_t` from whatever the bootloader
// hands it (multiboot info on x86; DTB pointer + initramfs region on aarch64),
// then calls kmain(boot_info_t*).

#ifndef SKALOPS_KERNEL_BOOT_BOOT_H
#define SKALOPS_KERNEL_BOOT_BOOT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uintptr_t initramfs_phys_base;
    size_t    initramfs_size;

    uintptr_t dtb_phys;          // 0 on x86
    uintptr_t multiboot_phys;    // 0 on aarch64

    const char* cmdline;         // kernel command line (already copied/sanitized)
} boot_info_t;

#endif
