// kernel/handle/handle.h — per-process handle table.
//
// See docs/pillars/01-handles.md for the design.

#ifndef SKALOPS_KERNEL_HANDLE_HANDLE_H
#define SKALOPS_KERNEL_HANDLE_HANDLE_H

#include <stdint.h>
#include <stddef.h>

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

#define HANDLE_MAX_PER_PROC 1024

struct handle_entry {
    handle_type_t type;
    uint32_t      subtype;     // device_class for H_FILE; zero otherwise
    void*         object;      // points at struct file, struct channel, ...
    uint32_t      rights;      // reserved; zero in v1
    uint32_t      refcount;    // intra-process refcount for handle_dup
};

struct handle_table {
    struct handle_entry entries[HANDLE_MAX_PER_PROC];
    handle_t            next_free_hint;
};

// TODO: implement in handle.c:
//   handle_t      handle_install   (struct handle_table*, handle_type_t, uint32_t subtype, void* object);
//   void*         handle_object    (const struct handle_table*, handle_t, handle_type_t expected);
//   handle_type_t handle_type_of   (const struct handle_table*, handle_t);
//   uint32_t      handle_subtype_of(const struct handle_table*, handle_t);
//   int32_t       handle_close     (struct handle_table*, handle_t);   // returns status

#endif
