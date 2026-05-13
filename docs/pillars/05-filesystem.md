# Pillar 5 — Filesystem and namespace

## Goals

- Familiar POSIX feel: a single global tree rooted at `/`, ambient per-process `cwd`, paths in the surface API.
- TOCTOU on path-based access is mitigated at the ABI level via the `*_at` family and resolution flags.
- Filesystems live in the kernel (monolithic) and are statically linked in v1.
- Sandboxing exists as an opt-in primitive but is not the default model.

## Contract — paths and resolution

Skalapos keeps POSIX-style path semantics but makes the `*_at(dir_h, relpath, …)` form the canonical syscall ABI:

- All path-taking syscalls accept a `dir_handle` and a relative `path`. The path must NOT begin with `/`; absolute paths at the kernel boundary are a typed error (`STATUS_PATH_NOT_RELATIVE`).
- libc provides bare `open(path, …)`-style wrappers that internally translate: if `path` starts with `/`, libc strips the leading slash and calls `_at(ROOT_DIR, path[1..], …)`; otherwise it calls `_at(CWD_DIR, path, …)`. ROOT_DIR and CWD_DIR are Directory handles held in fixed libc-internal slots, set up by the kernel at spawn time.
- Path resolution walks component-by-component inside the kernel. Per-resolution flags control how strict the walk is.

### Resolution flags (all `*_at` syscalls)

| Flag | Meaning |
|---|---|
| `AT_NOFOLLOW` | The final component must not be a symlink. |
| `AT_NOSYMLINKS` | No symlinks anywhere in the resolution. |
| `AT_BENEATH` | Resolution must stay within `dir_handle`'s subtree. `..` cannot escape; absolute symlinks fail. |
| `AT_EMPTY` | Permit `relpath == ""` to refer to `dir_handle` itself (for e.g. `fstat`-style ops via `stat_at`). |

`AT_BENEATH` is the killer feature: it makes safe traversal of untrusted trees a one-flag operation. Modeled on Linux's `RESOLVE_BENEATH`. Internally implemented by tracking the originating directory's filesystem and inode and refusing any walk step that crosses out of its subtree.

### The fstat-on-handle pattern

`stat(path)` is racey-by-construction (the path is re-resolved each call). Skalapos's documented safe pattern:

```c
handle_t fd;
open_at(dir_h, "etc/hosts", O_RDONLY, AT_BENEATH, &fd);
stat_info_t st;
fstat(fd, &st);   // refers to the same inode the open returned, period.
```

For check-then-use patterns, always check via `fstat`, never via a second `stat_at`. This is documented loudly because it is the single biggest TOCTOU-mitigation lesson.

## Contract — syscalls

```c
// Open / create / introspect:
status_t open_at(handle_t dir_h, const char* relpath,
                 uint32_t open_flags, uint32_t resolve_flags,
                 handle_t* out_file_h);
status_t mkdir_at(handle_t dir_h, const char* relpath, uint32_t mode);
status_t stat_at(handle_t dir_h, const char* relpath, uint32_t resolve_flags, stat_info_t* out);
status_t fstat(handle_t file_or_dir_h, stat_info_t* out);

// Directory iteration:
status_t dir_open_at(handle_t dir_h, const char* relpath, uint32_t resolve_flags, handle_t* out_dir_h);
status_t dir_read(handle_t dir_h, dirent_t* buf, size_t buf_cap, size_t* out_count);

// Mutate:
status_t link_at(handle_t old_dir, const char* old_rel,
                 handle_t new_dir, const char* new_rel, uint32_t flags);
status_t symlink_at(handle_t dir_h, const char* relpath, const char* target);
status_t unlink_at(handle_t dir_h, const char* relpath, uint32_t flags);   // flags: AT_REMOVEDIR
status_t rename_at(handle_t old_dir, const char* old_rel,
                   handle_t new_dir, const char* new_rel, uint32_t flags);

// Streaming I/O (works uniformly on files, ttys, block devices):
status_t file_read(handle_t file_h, void* buf, size_t n, int64_t offset, size_t* out_n);
status_t file_write(handle_t file_h, const void* buf, size_t n, int64_t offset, size_t* out_n);
status_t file_seek(handle_t file_h, int64_t offset, uint32_t whence, int64_t* out_pos);

// Sandbox:
status_t proc_chroot(handle_t dir_h);   // replaces calling process's ROOT_DIR; irreversible
status_t proc_chdir_h(handle_t dir_h);  // sets calling process's CWD_DIR from a handle
```

`offset == -1` for file_read/file_write means "use the file's current position." Otherwise the access is positional and the position is not updated.

## Mounting

```c
status_t fs_mount(handle_t source_h, handle_t target_dir_h,
                  const char* fs_type, const void* opts, size_t opts_len);
status_t fs_unmount(handle_t target_dir_h);
```

- `source_h` is a backing store: a Block-device File handle for an on-disk FS (v2+), or `HANDLE_INVALID` for in-memory FSes (tmpfs, devfs).
- `target_dir_h` is a Directory handle. After mount, accessing anything beneath that directory dispatches to the new FS.
- `fs_type` is a string matching a registered filesystem (`"initramfs"`, `"tmpfs"`, eventually `"skfs"` for a real on-disk FS in v2).
- One global mount table. Mount is privileged. **v1 has no privilege checks**; v2 will gate this on the deferred privilege bit.

### v1 mount layout

```
/                  initramfs   (CPIO blob handed in by bootloader)
/dev               devfs       (synthetic; populated by driver-init code)
/tmp               tmpfs       (in-memory, populated empty)
```

That's the entire v1 mount table. No `/proc`, no `/sys` (deferred). No persistent storage.

## Pseudocode — path walk

```c
status_t open_at_impl(handle_t dir_h, const char* relpath, uint32_t open_flags,
                      uint32_t resolve_flags, handle_t* out) {
    if (handle_type_of(dir_h) != H_DIR) return STATUS_BAD_HANDLE_TYPE;
    if (relpath[0] == '/')              return STATUS_PATH_NOT_RELATIVE;

    struct dir* base = handle_object(dir_h);
    struct dir* origin = base;          // for AT_BENEATH
    struct dir* cur = base;
    const char* p = relpath;

    while (*p) {
        const char* slash = strchr(p, '/');
        size_t component_len = slash ? (size_t)(slash - p) : strlen(p);

        if (component_len == 0) { p++; continue; }
        if (component_len == 2 && p[0] == '.' && p[1] == '.') {
            if (resolve_flags & AT_BENEATH && cur == origin) return STATUS_NOT_BENEATH;
            cur = cur->parent ?: cur;   // root's parent is itself
            p += component_len;
            continue;
        }
        if (component_len == 1 && p[0] == '.') { p += component_len; continue; }

        struct inode* next = dir_lookup(cur, p, component_len);
        if (!next) return STATUS_NO_ENTRY;

        if (next->type == INODE_SYMLINK) {
            if (resolve_flags & AT_NOSYMLINKS) return STATUS_LOOP;
            if (!slash && (resolve_flags & AT_NOFOLLOW)) {
                // last component; AT_NOFOLLOW means we return the symlink itself, see open_flags
                ...
            }
            // resolve the symlink:
            const char* target = symlink_target(next);
            if (target[0] == '/' && (resolve_flags & AT_BENEATH))
                return STATUS_NOT_BENEATH;
            // recurse / loop with bounded depth (40, traditional)
            ...
        }

        cur = next;
        p += component_len;
        if (slash) p++;
    }

    // open `cur` and install in handle table
    *out = handle_install(current_process()->handles, H_FILE, cur, ...);
    return STATUS_OK;
}
```

(Real implementation is more careful, but the shape is right.)

## Why this over alternatives

- **F3: single-component-only kernel ABI.** Strictly safer (every lookup is one component, atomic), but requires libc to walk paths one component per syscall — many syscalls per `open`. Rejected by user as too aggressive an ergonomic break.
- **F2: Plan 9-style per-process namespace.** Each process gets its own mutable mount tree. Slicker for sandboxing; "what is `/etc/hosts`?" depends on who's asking. Rejected — the user wants familiar POSIX paths with no namespace surprises.
- **No `cwd`** — would force libc to maintain it as a userland concept, identical to the kernel implementation. Net: ambient `cwd` in the kernel is cheaper and matches user expectations.

## v2+ direction

- **On-disk FS.** A simple Skalapos FS ("skfs") backed by a block device. Tree-of-extents inode layout, no journaling in v2, no fancy features.
- **`/proc`-equivalent.** A read-only synthetic FS exposing process and handle introspection. Built on top of the same FS interface drivers use.
- **`mount` privilege.** Either via the deferred boolean privilege bit, or — long-term — via a `MOUNT_AUTHORITY` typed handle that PID 1 starts holding and selectively grants.
- **Persistent namespace operations.** Bind-mount-like ("show this directory at this other location") composed via mount.
- **`O_PATH`-equivalent.** Open a path *just* to get a handle for `*_at` use, without permission to read/write the underlying object.
