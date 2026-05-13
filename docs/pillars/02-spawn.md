# Pillar 2 — Process creation

## Goals

- One primitive for creating a new process; no `fork`/`exec` two-step.
- No copy-on-write address-space duplication; no surprises about which file descriptors a child inherits.
- The set of handles the child receives is explicit at the call site.
- Threads within a process are a separate, simpler primitive.

## Contract

### Processes

```c
status_t proc_spawn(
    /* image */    handle_t file_h,                 // ELF executable as a File handle
    /* args  */    const char* const* argv,         // null-terminated
    /* env   */    const char* const* envp,         // null-terminated
    /* pass  */    const handle_t* handles_to_pass, // array of handles in this process
                   size_t handles_count,
    /* opts  */    const spawn_opts_t* opts,        // may be NULL
    /* out   */    handle_t* out_proc_h);           // receives a Process handle
```

Semantics:

- The image is identified by a `File` handle, not a path. To spawn from a path, caller does `open_at(...)` first. This is intentional: there is no implicit `PATH` resolution at the kernel boundary.
- The new process starts execution at the ELF entry point. Its handle table is populated, in order, from `handles_to_pass`: the first handle in the array becomes handle `1` in the child, the second becomes handle `2`, etc. (Recall handle `0` is reserved as "invalid.")
- The caller's handles in `handles_to_pass` are **not** closed in the parent. The two processes now share the underlying objects (by refcount).
- `argv` and `envp` are copied into the child's address space at startup. Convention: `argv[0]` is the program name (not a path, just a name for `/proc/$pid/comm`-style introspection later).
- The kernel zeroes the child's BSS, sets up its stack with `argv`/`envp` per the ELF ABI for the target architecture, and randomizes addresses (ASLR, [pillar 7](07-memory.md)).
- The returned `Process` handle in the parent can be `proc_wait`-ed on, `process_kill`-ed (pillar 4), or passed to other processes for supervision.

`spawn_opts_t` carries optional fields (all zero/null = defaults):

```c
typedef struct {
    handle_t   root_dir;        // Directory handle; child's ROOT_DIR. NULL → inherit parent's.
    handle_t   cwd_dir;         // Directory handle; child's CWD_DIR. NULL → inherit parent's.
    handle_t   control_chan;    // Channel handle to use as the child's control channel.
                                // NULL → kernel creates a new one and passes it back via the
                                // child's handle table at a known slot (see init pillar).
    uint32_t   flags;           // reserved
} spawn_opts_t;
```

### Threads

```c
status_t thread_create(
    void (*entry)(void* arg),
    void* arg,
    size_t stack_size,
    const thread_opts_t* opts,
    handle_t* out_thread_h);

status_t thread_exit(int32_t status);                   // current thread
status_t thread_join(handle_t thread_h, int32_t* out_status);
```

Threads share the address space, file descriptors, and process control channel. They differ from processes in nothing except sharing — `thread_create` does not create a new process, does not get a new PID, does not have its own handle table.

The thread that started `main()` is conventionally "thread 0"; it is in no other way special. If thread 0 calls `thread_exit`, the rest of the threads continue running (unlike POSIX where the main thread is treated specially in some cases).

Process exit happens via `proc_exit(int32_t status)`. This terminates **all** threads of the calling process. The kernel posts a `Terminated{proc_h, status}` message on the process's parent's control channel.

## Pseudocode — what happens at spawn

```c
status_t proc_spawn_impl(...) {
    // 1. Validate the image handle.
    if (handle_type_of(file_h) != H_FILE) return STATUS_BAD_HANDLE_TYPE;

    // 2. Allocate a fresh process struct, address space, control channel.
    struct process* p = process_alloc();
    if (!p) return STATUS_OUT_OF_MEMORY;

    // 3. Load the ELF: parse headers, allocate VM regions, copy/map segments.
    //    Respects DEP (no W|X regions) and ASLR (randomizes base).
    status_t s = elf_load(file_h, p->vm);
    if (s != STATUS_OK) { process_free(p); return s; }

    // 4. Populate the child's handle table from handles_to_pass.
    for (size_t i = 0; i < handles_count; i++) {
        handle_install(p->handles, i + 1, handles_to_pass[i]);  // refcount-bumps the object
    }
    handle_install(p->handles, SLOT_ROOT_DIR,    opts->root_dir    ?: parent_root_dir);
    handle_install(p->handles, SLOT_CWD_DIR,     opts->cwd_dir     ?: parent_cwd_dir);
    handle_install(p->handles, SLOT_CONTROL_CHAN, opts->control_chan ?: new_chan);

    // 5. Build argv/envp on the new stack per ELF ABI for the target arch.
    elf_setup_stack(p, argv, envp);

    // 6. Hand back a Process handle to the parent.
    *out_proc_h = handle_install_new(current_process()->handles, H_PROC, p);

    // 7. Schedule.
    sched_add_thread(p->main_thread);
    return STATUS_OK;
}
```

## Why this over alternatives

- **`fork()`** — was POSIX's worst inheritance. Implicit COW duplication of every page, every FD, every signal handler, every lock. Useful in 1973; in 2026 it is a foot-cannon. `posix_spawn` already exists in POSIX as the modern alternative; Skalapos goes further and makes it the *only* primitive.
- **`posix_spawn` directly** — close in spirit to Skalapos's `proc_spawn`, but it carries POSIX baggage: file actions, sigmask, scheduler attributes, etc. Skalapos `proc_spawn` is leaner: handles list, args, env, options struct, done.
- **`vfork` / `clone(CLONE_VM|CLONE_FS|...)`** — Linux-isms. Address-space sharing for "fork then exec immediately" is unnecessary when `spawn` exists.
- **Coroutines or fibers as the default concurrency unit** — considered, rejected. Kernel-level concurrency is threads. Userland can build fibers on top with no kernel involvement.

## v2+ direction

- **Posthumous handle delivery.** If a process is spawned with `control_chan = NULL` and the kernel auto-creates the channel, the parent currently has to look it up via the Process handle (`proc_get_control_chan`). v2 could return a tuple `(out_proc_h, out_control_chan_h)` instead, removing a syscall.
- **Spawn-time resource limits.** `spawn_opts_t.flags` is reserved for things like "this process cannot create new threads," "max VM size," etc. Not in v1; trivial to add.
- **`proc_exec_self`-style hot reload.** Replace the current process's image with a new one, keeping the handle table. Useful for service managers. Definitely not v1.
