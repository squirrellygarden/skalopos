// userland/libc/include/skalops/dev/console.h
//
// Console device-class ops. See docs/pillars/08-drivers.md.
// Op codes have the device-class byte in their high 8 bits (0x01 = CONSOLE).

#ifndef SKALOPS_DEV_CONSOLE_H
#define SKALOPS_DEV_CONSOLE_H

#include <stdint.h>
#include <skalops/handle.h>
#include <sys/status.h>

#define DEV_CLASS_CONSOLE        0x01u
#define CONSOLE_SET_MODE         0x01000001u
#define CONSOLE_GET_SIZE         0x01000002u

typedef enum {
    CONSOLE_MODE_COOKED = 0,    // line-buffered, echo input
    CONSOLE_MODE_RAW    = 1,    // single character at a time, no echo
} console_mode_t;

typedef struct { uint32_t mode; } console_set_mode_args_t;

typedef struct {
    uint32_t cols;
    uint32_t rows;
} console_size_t;

// Inline wrappers below are convenience; they call dev_op underneath.
// (dev_op declared in <skalops/handle.h>-adjacent header — TODO add <skalops/dev.h>.)

status_t dev_op(handle_t h, uint32_t op_code, void* args, size_t args_len, int64_t* out_ret);

static inline status_t console_set_mode(handle_t h, console_mode_t m) {
    console_set_mode_args_t a = { .mode = (uint32_t)m };
    return dev_op(h, CONSOLE_SET_MODE, &a, sizeof a, NULL);
}

static inline status_t console_get_size(handle_t h, console_size_t* out) {
    return dev_op(h, CONSOLE_GET_SIZE, out, sizeof *out, NULL);
}

#endif
