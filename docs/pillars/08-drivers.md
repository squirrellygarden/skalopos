# Pillar 8 — Drivers

## Goals

- Drivers live in the kernel (monolithic). Statically linked. LKMs deferred.
- Userland sees devices as files under `/dev`, opened to typed handles.
- Streaming I/O (read/write bytes) uses the same `file_read`/`file_write` it uses for regular files.
- Structured device operations go through one syscall: `dev_op(handle, op_code, args, args_len)`. There is no `ioctl`, no `fcntl`, no `prctl`.

## Contract 
### Userland Surface

Devices appear in the filesystem under `/dev`, populated by **devfs** at boot. Opening a device file returns a handle of type `H_FILE` whose subtype is the device's `device_class`.

```c
// Open a device:
handle_t h;
open_at(root_dir, "dev/console", O_RDWR, 0, &h);

// Streaming (works for files, ttys, block devices, anything stream-shaped):
file_read (h, buf, n, /*off*/-1, &got);
file_write(h, buf, n, /*off*/-1, &put);

// Structured op:
dev_op(h, CONSOLE_SET_MODE, &mode, sizeof mode, /*ret*/NULL);
```

`dev_op` is one syscall:

```c
status_t dev_op(handle_t file_h, uint32_t op_code,
                void* args, size_t args_len,
                int64_t* out_ret);     // op-specific scalar return, or NULL
```

Op codes are namespaced per device class. Layout: high 8 bits = class, low 24 bits = op-within-class.

```
CONSOLE class = 0x01:   CONSOLE_SET_MODE = 0x01_000001, CONSOLE_GET_SIZE = 0x01_000002, ...
BLOCK   class = 0x02:   BLOCK_GET_GEOMETRY = 0x02_000001, BLOCK_FLUSH = 0x02_000002, ...
NET     class = 0x03:   NET_SET_MAC = 0x03_000001, NET_GET_STATS = 0x03_000002, ...
TIMER   class = 0x04:   ...
FB      class = 0x05:   ...
INPUT   class = 0x06:   ...
```

The kernel validates: handle's subtype must equal the op code's high byte, else `STATUS_BAD_OP`.

### Per-class headers

Every op code is declared in a per-class header in `<skalops/dev/>` along with its argument and return struct:

```c
// userland/libc/include/skalops/dev/console.h
typedef struct { uint32_t mode; } console_set_mode_args_t;
#define CONSOLE_SET_MODE  0x01000001

static inline status_t console_set_mode(handle_t h, uint32_t mode) {
    console_set_mode_args_t a = { .mode = mode };
    return dev_op(h, CONSOLE_SET_MODE, &a, sizeof a, NULL);
}
```

So calling code says `console_set_mode(h, RAW)` — same ergonomics as a per-class syscall family would give, but underneath, only `dev_op` crosses the syscall boundary.

### In-kernel driver interface

Each driver implements:

```c
typedef struct {
    enum device_class class;
    const char * name;             // e.g., "pl011_uart"

    status_t (*open) (struct device*, handle_t* out_internal);
    status_t (*close)(struct device*);

    status_t (*read) (struct device*, void* buf, size_t n, int64_t off, size_t* out_n);
    status_t (*write)(struct device*, const void* buf, size_t n, int64_t off, size_t* out_n);

    status_t (*dev_op)(struct device*, uint32_t op_code, void* args, size_t args_len, int64_t * out_ret);

    // Hardware lifecycle:
    void (*irq_handler)(struct device*);   // called by interrupt subsystem
} driver_ops_t;
```

A driver registers itself with the kernel at init time:

```c
// kernel/driver/uart_pl011.c
static const driver_ops_t pl011_ops = { /* ... */ };

void pl011_init(void) {
    struct device* d = device_alloc(DEV_CONSOLE, "ttyS0", &pl011_ops);
    /* probe hardware, configure baud, hook IRQ */
    devfs_publish(d, "console");   // appears as /dev/console
}
DRIVER_INIT(pl011_init);   // section-attribute trick to enumerate drivers at boot
```

A driver may leave any op `NULL`. Calling a NULL op returns `STATUS_NOT_SUPPORTED`. Streaming-only devices populate `read`/`write` and leave `dev_op` NULL; structure-only devices do the inverse.

###  Device discovery

Skalapos uses a uniform "device probe" subsystem above two arch-specific enumerators:

- **x86_64:** ACPI table walk for platform devices, PCI bus enumeration for everything else. v1 may hardcode standard PC-AT (PIC/IO-APIC, PIT/HPET, COM1, VGA text mode).
- **aarch64:** flattened device tree (FDT/DTB) passed by the bootloader. Walk it, match `compatible` strings against registered drivers.

The shared interface:

```c
// kernel/driver/probe.h
typedef struct {
    const char* compatible[8];   // device-tree-style match strings; first match wins
    status_t (*probe)(const probe_info_t*);
} driver_match_t;

void driver_register_match(const driver_match_t* m);
```

The arch enumerator walks its respective table, calls `probe` on the first matching driver, and the probe function allocates a `struct device` and publishes it to devfs.

## Drivers

### v1

| Driver | x86_64 | aarch64 | Notes |
|---|---|---|---|
| Console | COM1 16550-compatible UART; VGA text mode | PL011 UART | `/dev/console`. `file_write` for output; `file_read` for input. |
| Timer | HPET (preferred) or legacy PIT | ARM generic timer | Scheduler tick; backs `timer_create` |
| IRQ controller | IO-APIC (preferred) or 8259 PIC | GIC-400 (Pi 4 has GICv2) | Internal; not exposed via `/dev` |

Optional v1 nice-to-haves:

| Driver | Notes |
|---|---|
| `null`/`zero`/`full` | Synthetic devices in devfs; ~40 LOC each. Useful for testing. |

### v2

| Driver | Notes |
|---|---|
| virtio-blk | First real block device, to enable persistent FS. |

### v3+

Networking, USB, sound, framebuffer beyond text mode, real-hardware block (SD/MMC, NVMe).

## Pseudocode — adding a driver

```c
// kernel/driver/null.c — synthetic device example
#include <skalops/dev/null.h>
#include "../driver/driver.h"
#include "../driver/devfs.h"

static status_t null_read(struct device* d, void* buf, size_t n, int64_t off, size_t* out_n) {
    (void)d; (void)off;
    *out_n = 0;
    return STATUS_OK;
}

static status_t null_write(struct device* d, const void* buf, size_t n, int64_t off, size_t* out_n) {
    (void)d; (void)buf; (void)off;
    *out_n = n;
    return STATUS_OK;
}

static const driver_ops_t null_ops = {
    .class = DEV_NONE,           // it's not really a class; could add DEV_SYNTHETIC
    .name  = "null",
    .read  = null_read,
    .write = null_write,
};

void null_init(void) {
    struct device* d = device_alloc(DEV_NONE, "null", &null_ops);
    devfs_publish(d, "null");
}
DRIVER_INIT(null_init);
```

## Why this over alternatives

- **`ioctl`** — Awful.
- **Per-class top-level handle types** — would give `H_CONSOLE`, `H_BLOCK`, etc., each with its own syscall family. ABI-level type safety; bigger trap table; abandons `file_read`/`file_write` uniformity; ABI commitment per device class. Skalapos chose D2 for the flattening tradeoff.
- **Drivers as channel endpoints** — every op a message. Maximally aligns with the event model but breaks the "everything is a file" property hardest. Considered, set aside.

## v2+ direction

- Loadable kernel modules, if the project grows such that this is helpful.
- Userland device drivers, if desired.
