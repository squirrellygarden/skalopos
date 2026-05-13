// kernel/driver/driver.h — driver registration and the driver-ops table.
//
// See docs/pillars/08-drivers.md.

#ifndef SKL_KERNEL_DRIVER_DRIVER_H
#define SKL_KERNEL_DRIVER_DRIVER_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    DEV_NONE    = 0,    // regular files; synthetic devices like /dev/null
    DEV_CONSOLE = 1,
    DEV_BLOCK   = 2,
    DEV_NET     = 3,
    DEV_TIMER   = 4,
    DEV_FB      = 5,
    DEV_INPUT   = 6,
} device_class_t;

struct device;

typedef struct {
    device_class_t class;
    const char*    name;        // e.g., "pl011_uart"

    int32_t (*open) (struct device*, void** out_internal);
    int32_t (*close)(struct device*);

    int32_t (*read) (struct device*, void* buf, size_t n, int64_t off, size_t* out_n);
    int32_t (*write)(struct device*, const void* buf, size_t n, int64_t off, size_t* out_n);

    int32_t (*dev_op)(struct device*, uint32_t op_code, void* args, size_t args_len,
                      int64_t* out_ret);

    void    (*irq_handler)(struct device*);
} driver_ops_t;

// TODO: implement:
//   struct device* device_alloc(device_class_t, const char* name, const driver_ops_t* ops);
//   void          devfs_publish(struct device*, const char* name);   // appears as /dev/<name>

// DRIVER_INIT(fn) puts a function pointer in the .drivers ELF section so kmain
// can iterate registered drivers without a hand-maintained list. Linker script
// emits __drivers_start / __drivers_end markers.
#define DRIVER_INIT(fn) \
    static void (*const fn##_init_ptr)(void) \
        __attribute__((used, section(".drivers"))) = (fn)

#endif
