// userland/libc/include/skalops/handle.h
//
// Public handle-related declarations for userland. Mirrors the kernel
// definitions in kernel/handle/handle.h.

#ifndef SKALOPS_HANDLE_H
#define SKALOPS_HANDLE_H

#include <stdint.h>
#include <sys/status.h>

typedef int32_t handle_t;
#define HANDLE_INVALID ((handle_t)0)

typedef enum {
    H_NONE   = 0,
    H_FILE   = 1,
    H_DIR    = 2,
    H_CHNL   = 3,
    H_PROC   = 4,
    H_THREAD = 5,
    H_TIMER  = 6,
    H_SHM    = 7,
} handle_type_t;

/// Release a handle in the calling process.
status_t handle_close(handle_t h);

/// Duplicate a handle within the calling process.
status_t handle_dup(handle_t h, handle_t* out_h);

/// Introspect a handle's type and subtype.
status_t handle_type(handle_t h, uint32_t* out_type, uint32_t* out_subtype);

#endif
