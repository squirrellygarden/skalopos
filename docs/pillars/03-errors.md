# Pillar 3 — Errors

## Goals

- Every syscall reports success or failure in a uniform, typed way.
- No thread-local error state.
- No magical sentinel return values (`-1`, `NULL`, etc.) overloaded with "and check errno."
- Failure information carries enough context for a libc wrapper to surface a useful error without a second syscall.

## Contract

Every syscall returns a `Status`. Syscalls that have a meaningful payload on success return it via an out-parameter pointer or via a second register, depending on the ABI; the calling convention is documented per architecture in [`arch/<arch>/syscall-abi.md`](../reference/syscall-abi.md).

```c
typedef int32_t status_t;

// In every syscall signature:
status_t some_syscall(arg, arg, ..., out_value_t* out);

// libc wraps these:
typedef struct {
    status_t status;
    some_type value;     // valid iff status == STATUS_OK
} some_result_t;
```

### Status codes

Status codes are 32-bit signed integers, with the high 8 bits identifying the subsystem and the low 24 bits identifying the specific condition. Layout:

```
 31         24 23                                       0
 +-----------+------------------------------------------+
 |  subsys   |             code within subsys           |
 +-----------+------------------------------------------+
```

Subsystem IDs:

| ID | Subsystem | Example codes |
|----|-----------|---------------|
| 0  | Global | `STATUS_OK = 0`, `STATUS_NOT_IMPLEMENTED`, `STATUS_INTERNAL_ERROR` |
| 1  | Handle | `STATUS_BAD_HANDLE`, `STATUS_BAD_HANDLE_TYPE`, `STATUS_HANDLE_TABLE_FULL` |
| 2  | FS | `STATUS_NO_ENTRY`, `STATUS_NOT_DIR`, `STATUS_IS_DIR`, `STATUS_NAME_TOO_LONG`, `STATUS_NOT_BENEATH`, `STATUS_LOOP` |
| 3  | Process | `STATUS_NO_PROC`, `STATUS_BAD_EXEC`, `STATUS_OUT_OF_PIDS` |
| 4  | Memory | `STATUS_OUT_OF_MEMORY`, `STATUS_BAD_PROT`, `STATUS_WX_DENIED`, `STATUS_NOT_MAPPED` |
| 5  | Channel | `STATUS_CHAN_CLOSED`, `STATUS_CHAN_EMPTY`, `STATUS_CHAN_FULL` |
| 6  | Device | `STATUS_BAD_OP`, `STATUS_DEVICE_BUSY`, `STATUS_DEVICE_GONE` |
| 7  | I/O | `STATUS_EOF`, `STATUS_INTERRUPTED`, `STATUS_BAD_SIZE`, `STATUS_OFFSET_BAD` |
| 8  | Reserved | reserved |

`STATUS_OK == 0` always. Any nonzero status is failure.

The full list lives in [`schemas/status.toml`](../../schemas/status.toml) and is codegen'd into `<sys/status.h>` for both kernel and userland.

### Errno-style helpers in libc

To keep ports of POSIX-style code livable, libc provides:

```c
const char* status_name(status_t s);     // "STATUS_NO_ENTRY"
const char* status_describe(status_t s); // "no such file or directory"
```

But there is no `errno` global, and libc functions return `status_t` directly or as part of a result struct.

### `<skalops/check.h>` — control-flow macros for `status_t`

Two macros, complementary, in the spirit of Rust's `?` vs `.unwrap()`:

```c
// Propagate. If `expr` returns a non-OK status_t, return that status from
// the enclosing function. The enclosing function must itself return status_t.
// Used as a statement.
STATUS_TRY(expr);

// Unwrap-or-die. Evaluates a result-struct-returning call; on success yields
// `.value`, on failure writes a diagnostic to stderr and calls proc_exit with
// a non-zero code. Used as an expression.
STATUS_OR_DIE(expr);
```

`STATUS_TRY` is for normal error propagation up the call stack; `STATUS_OR_DIE` is for "I have asserted this cannot fail in this program" sites (test harnesses, initialization paths where failure is unrecoverable, throwaway tools).

## Pseudocode — call site shapes

```c
// Single out value:
handle_t fd;
status_t s = open_at(root_dir_h, "etc/hosts", O_RDONLY, AT_BENEATH, &fd);
if (s != STATUS_OK) {
    fprintf(stderr, "open: %s\n", status_describe(s));
    proc_exit(1);
}

// libc convenience wrapper (also generated from the schema):
file_open_result_t r = file_open(root_dir_h, "etc/hosts", O_RDONLY, AT_BENEATH);
if (r.status != STATUS_OK) { ... }
use(r.handle);

// Propagate (enclosing function returns status_t):
status_t load_config(handle_t root_dir_h, handle_t* out_fd) {
    STATUS_TRY(open_at(root_dir_h, "etc/hosts", O_RDONLY, AT_BENEATH, out_fd));
    // ... more setup, any of which may STATUS_TRY ...
    return STATUS_OK;
}

// Unwrap-or-die (call site cannot meaningfully recover):
handle_t fd = STATUS_OR_DIE(file_open(root_dir_h, "etc/hosts", O_RDONLY, AT_BENEATH));
```

## Why this over alternatives

- **`errno`** — thread-local, has to be saved/restored across library calls, easy to forget to clear, and forces every error-returning function to overload its return value with a sentinel. Skalapos pays the cost of a small struct return (or a two-register convention) and gets back composability.
- **`-1` sentinel** — Conflicts with valid return values for some syscalls (consider `read()` returning a `ssize_t`), forces explicit checks everywhere, and trains programmers to ignore returns. Status-first ABI makes the check unavoidable.
- **Rich error objects passed through a caller-provided buffer** — gives more context (which path, which handle, where) but doubles the syscall ABI surface and increases complexity.

## v2+ direction

No planned direction.