# Pillar 9 — Boot and init

## Goals

- The bootstrap path is unambiguous: bootloader → kernel → PID 1 → userland.
- v1 keeps PID 1 minimal: it is a shell. No service supervision in v1.
- v2 separates "the process the kernel starts" from "the process that supervises services."
- Privilege checks are deferred.

## Contract 
### v1 boot path

1. **Bootloader.**
2. **Kernel early init.**
   - Set up the BSS, the early `printf` (writing to console MMIO directly).
   - Configure paging / MMU. Identity-map the first few MiB, set up higher-half kernel mapping, switch to the high-half VA.
   - Install the IDT (x86) or exception vector table (aarch64).
   - Bring up the IRQ controller and timer driver.
   - Initialize the physical-frame allocator.
3. **Kernel late init.**
   - Discover devices (ACPI/PCI on x86, FDT walk on aarch64). Call each registered driver's probe.
   - Initialize the VFS. Register the initramfs filesystem type and mount the CPIO blob at `/`. Register tmpfs, mount at `/tmp`. Initialize devfs and mount at `/dev`; drivers' init code populates entries.
   - Parse the kernel command line. Recognize at least `init=<path>` (default `/bin/sh`).
4. **PID 1 spawn.**
   - The kernel opens `init` from the initramfs as a `File` handle.
   - It constructs the first process. PID 1's handle table is populated using a **hardcoded init layout** (see below).
   - The kernel schedules PID 1's main thread and enters the idle loop.
5. **PID 1 runs.**
   - In v1, PID 1 is `/bin/sh`. It prints a prompt to `/dev/console` and reads input from it.
   - If PID 1 exits, the kernel panics: `"PID 1 (init) exited, halting"`. This is intentional and aligned with Linux; if init dies, the only honest move is to stop.

#### PID 1 handle layout (hardcoded by the kernel)

PID 1 is the one process whose handles the kernel populates directly, because it has no parent:

| Handle slot | Object | Notes |
|---|---|---|
| 1 (stdin)  | `H_FILE` to `/dev/console` (RW) | |
| 2 (stdout) | dup of slot 1 | |
| 3 (stderr) | dup of slot 1 | |
| `SLOT_ROOT_DIR`     | `H_DIR` to `/` | libc resolves absolute paths via this |
| `SLOT_CWD_DIR`      | `H_DIR` to `/` | libc resolves relative paths via this |
| `SLOT_CONTROL_CHNL` | `H_CHNL` (no peer) | Lifecycle messages go here; nobody can send to PID 1 from outside in v1 |

These slot numbers are part of the ABI; libc reads them on startup. All other processes inherit handles via the explicit `inherit_handles` list in `proc_spawn` (pillar 2).

### v2 boot path (planned)

Replaces step 5 above. PID 1 is no longer `sh`; it is a **tiny reaper supervisor**:

```c
// v2 PID 1 sketch (not real code, not in v1):
int main (void) {
    handle_t svc_mgr;
    spawn_svc_manager(&svc_mgr);

    for (;;) {
        evt_t msg;
        chnl_recv(control_chnl, &msg, sizeof msg, ...);

        switch (msg.kind) {
        case EVT_TERMINATED:
            if (msg.proc_h == svc_mgr) {
                log_warning("svc-manager died, restarting");
                spawn_svc_manager(&svc_mgr);
            }
            // else: an orphaned process's death; log and continue.
            break;
        case EVT_KILL_REQUEST:
            graceful_shutdown(svc_mgr, msg.deadline_ms);
            system_shutdown(SHUTDOWN_HALT);
            // (system_shutdown does not return)
        default:
            log_info("unhandled control-chnl msg: kind=%u", msg.kind);
        }
    }
}
```

PID 1 in v2 does *only* this. The service manager (`/sbin/svc-manager`) is a separate process responsible for:

- Reading service config from `/etc/svc.d/*` (format **deferred** — TOML or s-expressions to be chosen at v2; YAML noted but not preferred).
- Building a dependency DAG and spawning services in order.
- Exposing a control channel for tools like `skctl` (`skctl start foo`, `skctl status`).
- Implementing restart policies (e.g., "restart on crash with backoff").

If the service manager crashes, PID 1 restarts it. If PID 1 itself crashes (it shouldn't), the kernel panics.

### Privilege

In v1, no privilege; only user is root. Any process can mount, unmount, halt the system, access raw devices. In v2, single boolean. Each process has a `privileged` bit set at spawn time by PID 1. `fs_mount`, `system_shutdown`, raw device opens require `privileged`. The bit is hereditary unless `proc_spawn` explicitly drops it.

### Shutdown

 Mechanism:

```c
status_t system_shutdown(uint32_t mode);    // SHUTDOWN_HALT | SHUTDOWN_REBOOT | SHUTDOWN_POWEROFF
```

In v2, the service manager (on operator request via `skctl shutdown`) sends graceful kill messages with deadlines to each service, waits, then calls `system_shutdown`. The kernel flushes any caches (v2: block-device dirty pages), tears down driver state, and either halts the CPU, triggers a reboot, or signals ACPI/PSCI for poweroff.

v1 has no shutdown syscall. To stop, kill QEMU.

## Why this over alternatives

- **PID 1 is the service manager directly (systemd-style monolith)** — works, but makes PID 1 big and its bugs require a reboot. Skalapos's v2 separation lets the service manager be replaced and debugged at runtime.
- **System V `/etc/inittab` parsed by PID 1** — same problem as above at lower complexity. Plus inittab's runlevel concept doesn't map onto anything Skalapos cares about.

## v2+ direction

- Reaper PID 1 + separate `svc-manager`.
- Service config format (TOML/sexpr/YAML decision deferred).
- `skctl` control tool.
- `system_shutdown`.
- Privilege.

## Reference: kernel command line

Parsed by the kernel in early init. Format: space-separated `key=value` or bare flags.

| Key | Default | Effect |
|---|---|---|
| `init=PATH` | `/bin/sh` | Path to PID 1's executable, in the initramfs |
| `loglevel=N` | `info` | Kernel log verbosity: `error`, `warn`, `info`, `debug`, `trace` |
| `console=DEV` | `ttyS0` on x86, `ttyAMA0` on Pi 4 | Which device is `/dev/console` |
| `noaslr` | (off) | Reserved; v1 ignores. Will be honored for debugging in v2. |
