# Pillar 7 — Memory

## Goals

- Userland syscalls express intent, not flag combinations.
- One unified shared-memory primitive: `ShmHandle`. Not System V shm, not POSIX `shm_open`, not `MAP_SHARED|MAP_ANONYMOUS|MAP_FILE` overloads.
- DEP and ASLR are always on.
- The kernel's internal physical-frame allocator and paging design are *implementation details*, free to change; this pillar is the userland-visible ABI.

## Contract 

### Anonymous and file-backed memory

```c
status_t vm_alloc(size_t len, uint32_t prot, void** out_base);
status_t vm_unmap(void* base, size_t len);
status_t vm_protect(void* base, size_t len, uint32_t new_prot);
status_t vm_advise(void* base, size_t len, uint32_t hint);   // DONTNEED, WILLNEED, RANDOM, SEQUENTIAL

status_t vm_map_file(handle_t file_h, int64_t file_off, size_t len,
                     uint32_t prot, uint32_t flags, void** out_base);
//   flags: SHARED | PRIVATE
```

`prot` is a bitmask: `PROT_READ | PROT_WRITE | PROT_EXEC`. The kernel rejects `prot & PROT_WRITE && prot & PROT_EXEC` outright with `STATUS_WX_DENIED`. To produce executable memory at runtime (a JIT), the standard pattern is:

```c
void* code;
vm_alloc(SIZE, PROT_READ | PROT_WRITE, &code);
// ... write machine code into `code` ...
vm_protect(code, SIZE, PROT_READ | PROT_EXEC);
```

`vm_unmap` works on any mapping created by `vm_alloc`, `vm_map_file`, or `vm_map_shared`. For anonymous memory, unmapping is also deallocation.

The kernel chooses addresses for all of `vm_alloc`/`vm_map_*`. There is no `MAP_FIXED` in v1. The kernel applies **ASLR** by randomizing placement of stack, code, heap, and each subsequent mapping. ASLR is not configurable.

### Shared memory

```c
status_t shm_create(size_t len, uint32_t flags, handle_t* out_shm_h);
status_t shm_stat(handle_t shm_h, shm_info_t* out);
status_t vm_map_shared(handle_t shm_h, uint32_t prot, uint32_t flags, void** out_base);
```

A `ShmHandle` (`H_SHM`) refers to a kernel-managed pool of pages with no associated filesystem path. To share with another process:

- Pass the handle in the `inherit_handles` array of [`proc_spawn`](02-spawn.md), or
- Send the handle in a message via [`chan_send`](04-events.md).

Either process can then `vm_map_shared` it. Maps in different processes refer to the same physical pages — writes by one are visible to the other.

`shm_create` always returns RW-capable memory. `vm_map_shared`'s `prot` parameter controls *this mapping*'s permissions, not the underlying object's. Different processes can hold mappings of the same `ShmHandle` with different prot.

#### Fixed semantics

- Pages of an `ShmHandle` are demand-paged: not committed until first written.
- Closing the last handle to a `ShmHandle` frees the pages.
- `ShmHandle` is the only way two processes share writable memory. There is no `MAP_SHARED|MAP_ANONYMOUS`, no System V shm, no POSIX `shm_open`.

### DEP, ASLR, guard pages

Three hardening defaults:

- **DEP** — always on. Hardware NX/XN bit set on all writable pages. `vm_alloc` and `vm_protect` reject `WRITE | EXEC`. The kernel's `.text` and the userland's `.text` are also never writable.
- **ASLR** — always on. Stack base, executable base, heap base, and each `vm_alloc`/`vm_map_*` address are randomized within the architecturally legal user range. Needs a small kernel PRNG seeded at boot.
  - v1 PRNG: a 64-bit xoshiro-like state seeded from `RDRAND` (x86) or BCM2711 HRNG / ARM generic-timer entropy (aarch64). "Weak PRNG OK for v1" — not cryptographic-strength; addresses unpredictable across boots is the bar.
- **Guard pages** — deferred to libc/v2. Kernel does not insert guard pages automatically in v1; caller can leave unmapped padding by their own bookkeeping.

### Pseudocode — process address-space layout (v1, x86_64)

```
0x0000000000000000  +-----------------+
                    |    unmapped     |  (catches NULL derefs)
0x0000000000400000  +-----------------+   ← randomized base for executable
                    |   text  (RX)    |
                    |   rodata (R)    |
                    |   data  (RW)    |
                    |   bss   (RW)    |
                    +-----------------+
                    |       gap       |
                    +-----------------+
                    |     heap (RW)   |  ← grows via vm_alloc
                    +-----------------+
                    |  mmaps  (vary)  |  ← vm_map_file / vm_map_shared
                    +-----------------+
0x00007fffffffffff  |   stack (RW)    |  ← grows down; randomized top
                    +-----------------+
```

aarch64 is similar but the canonical user range is `0x0000_0000_0000_0000` to `0x0000_ffff_ffff_ffff`. Specific addresses chosen by the kernel; userland never assumes them.

## Why this over alternatives

- **POSIX `mmap` omnibus** — one syscall, ~12 documented flag combinations, plus `MAP_FIXED`, `MAP_HUGETLB`, `MAP_STACK`, `MAP_NORESERVE`, etc. A reader of `mmap(NULL, 4096, PROT_RW, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)` has to mentally simulate the flag combo to know what's happening. Skalapos splits into three named primitives — anonymous, file-backed, shared — and each name reads correctly at the call site.
- **System V shm** — global numeric keys, `ipcrm`, `ipcs`. Genuinely awful. Skalapos's `ShmHandle` is the modern unified replacement.
- **POSIX `shm_open` + path namespace** — works (shared memory as files in a special tmpfs), but adds a namespace solely for shared memory and requires `mmap` to use. Replaced by `ShmHandle` flowing through normal handle-passing.
- **DEP opt-in** — Unsafe. If a future JIT needs RWX, explore `vm_alloc_unsafe` then.
- **Variable ASLR** — Unsafe.

## v2+ direction

- **Dynamic allocator.** v1 ships a bump allocator; push allocator to later release.
- **`vm_remap` / coalescing.** Linux's `mremap` is occasionally useful for realloc. v1's `realloc` can be naive (alloc-new, copy, free-old via no-op); revisit when it hurts.
- **`MAP_FIXED`-equivalent.** Add `vm_alloc_at(addr, len, prot)` as a separate syscall. Useful for loaders / debuggers; not the common path.
