# Pillar 1 — Handles

## Goals

- Every reference a process holds to a kernel object goes through a single, uniform mechanism.
- Type confusion (reading a `Channel` as if it were a `File`) is caught at the syscall edge with a typed error, not later in driver code.
- The `ioctl`/`fcntl`/`prctl` family of grab-bag syscalls does not need to exist.

## Contract

A **handle** is a `uint32_t` identifying an entry in the calling process's handle table. The same numeric value in two different processes refers to different objects. Handles are not transferable as bare integers; to share an object with another process, the holder passes the handle via [`spawn`](02-spawn.md) or [`chnl_send`](04-events.md), which the kernel translates into an entry in the recipient's table.

Each handle-table entry carries:

- **A type tag** — one of `H_FILE`, `H_DIR`, `H_CHNL`, `H_PROC`, `H_THREAD`, `H_TIMER`, `H_SHM`. (Future types are added by extending the enum; old code does not break.)
- **A subtype tag, valid only for `H_FILE`** — a `device_class` enum value (`DEV_NONE` for regular files, `DEV_CONSOLE`, `DEV_BLOCK`, `DEV_NET`, `DEV_FB`, `DEV_INPUT`, …). The kernel uses this to validate the op code passed to [`dev_op`](08-drivers.md).
- **A pointer to the kernel-side object** (e.g., a `struct file`, `struct channel`, `struct process`).
- **A reserved rights field** (zero in v1; not validated, but laid out so it can be enforced later).

The handle table is per-process. Handle `0` is never valid (this is intentional — `0` looks like `NULL` and is a useful sentinel for "no handle"). Standard input/output/error in libc are conventionally handles `1`, `2`, `3` for the process; the kernel does not hardcode these except in [PID 1's startup layout](09-init.md).

### Syscalls operating on any handle

- `handle_close(h) → Status` — release the entry. After this call, `h` is invalid in the calling process. If the object's last reference is dropped, the kernel reclaims it.
- `handle_dup(h, out_h*) → Status` — return a new handle in the calling process referring to the same object.
- `handle_type(h, out_type*, out_subtype*) → Status` — introspect a handle. Used by libc to write `read(fd, …)` as "is `fd` an `H_FILE`? then call `file_read`; otherwise return `STATUS_BAD_HANDLE`."

### Type-checked dispatch

Every syscall that takes a handle declares the expected type (and subtype, where relevant) in its [schema entry](../../schemas/syscalls.toml). The kernel-side dispatcher validates this before calling the handler:

```c
// Generated dispatcher fragment for file_read:
if (handle_type_of(args.h) != H_FILE) return STATUS_BAD_HANDLE_TYPE;
return file_read_impl(...);

// And for a device-specific op like CONSOLE_SET_MODE going through dev_op:
if (handle_type_of(args.h) != H_FILE) return STATUS_BAD_HANDLE_TYPE;
if (device_class_of(args.h) != class_of_op(args.op_code)) return STATUS_BAD_OP;
return dev_op_impl(...);
```

## Pseudocode — handle table layout

```c
// kernel/handle/handle.h
typedef int32_t handle_t;
#define HANDLE_INVALID 0

typedef enum {
    H_NONE = 0,
    H_FILE,
    H_DIR,
    H_CHNL,
    H_PROC,
    H_THREAD,
    H_TIMER,
    H_SHM,
} handle_type_t;

typedef struct {
    handle_type_t type;
    uint32_t      subtype;   // device_class for H_FILE, zero otherwise
    void*         object;    // points at struct file, struct channel, ...
    uint32_t      rights;    // reserved; zero in v1
    uint32_t      refcount;  // for shared objects across handles in this process
} handle_entry_t;

typedef struct {
    handle_entry_t entries[HANDLE_MAX_PER_PROC];   // 1024 to start
    handle_t       next_free_hint;
} handle_table_t;
```

## Why this over alternatives

- **Bare `int` FDs** (POSIX) — give no type information at the ABI; cannot reject wrong-typed access at the syscall edge; force every subsystem to defensively dispatch on internal type after the call. Skalapos type-checks at the boundary.
- **Capability handles with rights bits** (Fuchsia, seL4) — were considered and rejected. The user finds them "nightmarish" in practice: every subsystem must define and enforce its rights bits, the bootstrap question ("how does any process get a root capability?") is hard, and the userland mental model gets heavy. Skalapos leaves a rights field reserved so this can be revisited in v3+, but no v1 syscall consults it.
- **Per-class top-level handle types** (D4 from pillar 8 discussion) — pushes the type tag up into the handle-type enum (`H_CONSOLE`, `H_BLOCK`, etc.) and gives every device class its own syscall family. Rejected because it kills "everything is a file" for byte-stream operations and grows the trap table proportionally with device classes. Skalapos's choice keeps `file_read`/`file_write` uniform across files, ttys, and block devices and uses the subtype field to gate structured ops.

## v2+ direction

- **Rights bits become real.** The reserved `rights` field starts being consulted at syscall dispatch. Handles can be duplicated with strictly fewer rights (`handle_derive(h, new_rights, out_h*)`). This is the minimum viable capability story without committing to a full capability OS.
- **Handle inheritance auditing.** A debug syscall (`handle_dump(proc_h, …)`) lists every handle a process holds, its type, and where it came from (parent at spawn, received via channel, dup'd). Trivial to implement once the table is populated; invaluable for debugging.
- **Cross-process handle revocation.** A holder can hand out a handle whose backing object can be revoked at will. Useful for service supervisors. Not v1.
