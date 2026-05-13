# Pillar 11 — Userland scope

## Goals

- v1 ships enough userland to exercise every locked-in ABI end-to-end.
- The libc is *Skalapos's* libc, not glibc; no goal of POSIX source-compat.
- Header layout makes the boundary between Skalapos primitives and ISO-C-ish APIs visible at every `#include`.
- A real dynamic allocator is **not** in v1 — it's its own subsystem with its own design pass.

## What ships in v1

- **libc-core** (`userland/libc/`, ~500 LOC).
- **`/bin/sh`** as PID 1 (`userland/sh/`, ~300 LOC).
- **Utilities** in `userland/utils/`: `echo`, `cat`, `ls`, `pwd`, `mkdir`. Each is 30–100 LOC.

Total userland: ~2000–2500 LOC. Enough to demo every kernel ABI; nothing more.

## Header naming convention

Three buckets:

| Prefix | Meaning | Examples |
|---|---|---|
| `<skl/...>` | Skalapos-specific primitives | `<skl/spawn.h>`, `<skl/channel.h>`, `<skl/handle.h>`, `<skl/dev/console.h>`, `<skl/dev/block.h>`, `<skl/check.h>` |
| Unprefixed | ISO-C-ish surface | `<string.h>`, `<stdio.h>`, `<stdlib.h>`, `<stdint.h>`, `<stddef.h>` |
| `<sys/...>` | Syscall-adjacent low-level | `<sys/status.h>`, `<sys/syscall.h>` (mostly used by libc internals) |

The `#include` line tells you which world you're in. Anything portable-looking should *feel* portable; anything Skalapos-specific should *look* Skalapos-specific.

## libc-core contents

### Provided in v1

```c
// <string.h>
void*  memcpy(void* dst, const void* src, size_t n);
void*  memset(void* dst, int c, size_t n);
void*  memmove(void* dst, const void* src, size_t n);
int    memcmp(const void* a, const void* b, size_t n);
size_t strlen(const char* s);
int    strcmp(const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
char*  strchr(const char* s, int c);

// <stdio.h>
int  printf (const char* fmt, ...);     // writes to handle 2 (stdout)
int  fprintf(handle_t h, const char* fmt, ...);   // writes to any File handle
int  puts(const char* s);
int  fputs(const char* s, handle_t h);
int  putchar(int c);
int  putc(int c, handle_t h);

// <stdlib.h>
void* malloc(size_t n);
void  free(void* p);                    // no-op in v1
void  abort(void) __attribute__((noreturn));
int   atoi(const char* s);

// <skl/check.h>
#define STATUS_OR_DIE(expr) ...   // exits with status_describe() on failure
```

### Not provided in v1

- `fopen`/`fclose`/`fread`/`fwrite` (FILE*-based stdio). v1 uses raw handle-based `fputs`/`fputc`. Decide if/when to add stdio FILE* in v2.
- `scanf`. No.
- Long-double, locale, multibyte, wide chars. No.
- `getenv`/`setenv`. Deferred; the spawn ABI passes `envp` as `(char*[])` but v1 doesn't process it beyond making it available to `main`.
- `time`, `gettimeofday`. Add a `<skl/time.h>` in v2.

### CRT (crt0)

`crt0.S` (per arch) is the first code that runs in a freshly-spawned process:

1. The kernel passes `argc`, `argv`, `envp` on the stack per the ELF ABI.
2. `crt0` zeroes BSS (linker provides `__bss_start`/`__bss_end`).
3. `crt0` initializes the bump allocator (assigns the arena base).
4. `crt0` calls `main(argc, argv, envp)`.
5. On return, `crt0` calls `proc_exit(main's_return_value)`.

## The bump allocator (v1)

```c
// userland/libc/src/stdlib/alloc.c
static uint8_t arena[64 * 1024];   // .bss, ~64 KiB
static size_t  cursor = 0;

void* malloc(size_t n) {
    if (n == 0) return NULL;
    size_t aligned = (n + 15) & ~(size_t)15;
    if (cursor + aligned > sizeof arena) return NULL;
    void* p = &arena[cursor];
    cursor += aligned;
    return p;
}

void free(void* p) {
    (void)p;   // no-op in v1
}
```

Long-running processes leak. **The shell sidesteps this by not calling `malloc` at all** — it uses a fixed-size line buffer in its own BSS. Utilities run-to-exit and don't care.

If a utility's malloc usage ever approaches the arena size, increase the arena size in libc. When that becomes uncomfortable, that's the signal to do v2's allocator design pass.

This is documented loudly in [`userland/libc/README.md`](../../userland/libc/README.md): "v1 malloc is a leaky bump allocator. Do not write long-running processes against it without `arena_reset()`."

## The shell (`/bin/sh`)

Minimal interactive shell. ~300 LOC. Capabilities:

- Read a line from stdin (handle 1).
- Tokenize on whitespace. No quoting, no escapes.
- First token is the command. If it contains a `/`, treat it as a path. Otherwise search a fixed path: `/bin`, `/sbin`.
- `open_at` the binary, `proc_spawn` it with the shell's `stdin`/`stdout`/`stderr`/`root_dir`/`cwd_dir` handles passed through, `proc_wait` for completion, print exit status if non-zero.
- Built-ins: `cd <path>` (calls `proc_chdir_h` after opening the new directory), `exit [n]`, `pwd` (prints `cwd_dir`'s path — needs a libc helper to materialize a path from a handle, or store it as a string the shell tracks).

Not in v1: pipes, redirections, globbing, environment substitution, command substitution, conditionals, loops, functions, history, line editing.

## Utilities — v1 set

| Program | LOC est. | What it does |
|---|---|---|
| `echo` | ~30 | Print args separated by spaces, newline at end. No `-n`, no `-e`. |
| `cat` | ~50 | For each arg, `open_at` and copy bytes to stdout. With no args, copy stdin. |
| `ls` | ~100 | `dir_open_at` + `dir_read` for argv[1] (or `.`). Print names. No flags. |
| `pwd` | ~30 | Print the current directory path. Needs a libc helper to render `CWD_DIR` to a string. |
| `mkdir` | ~30 | `mkdir_at(CWD_DIR, argv[1], 0755)`. No `-p`. |

After v1: `cp`, `mv`, `rm`, `cat -n`, etc. — when the ABI is stable and there's a reason.

## Schema-driven generation

Two files in [`schemas/`](../../schemas/) are the source of truth:

- `syscalls.toml` — every syscall: number, name, args (typed), return shape.
- `status.toml` — every status code: number, name, brief description, subsystem.

`tools/gen_syscalls.py` reads both and emits:

- Kernel: `kernel/syscall/dispatch_generated.c` (number → handler table), `kernel/syscall/numbers_generated.h`.
- Userland: `userland/libc/src/sys/syscall_generated.c` (one wrapper function per syscall, with the typed signature), `userland/libc/include/sys/syscall.h`, `userland/libc/include/sys/status.h`.
- Per-class device headers: not generated from schemas (their opcodes are in `<skl/dev/<class>.h>` and are part of the public API).

Run `just codegen` after editing the schemas. The generated files are checked into git so the tree builds without Python at hand — but they MUST be regenerated whenever the schemas change.

## Why this over alternatives

- **Building a real malloc in v1** — multi-week subsystem with its own subtleties (fragmentation, alignment, allocation patterns, locking for threads). Mixing its bugs with the kernel's bugs makes both undebuggable. The bump allocator buys us the libc surface with ~15 LOC and zero subtlety.
- **Skipping malloc entirely in v1** — would force every utility to use `vm_alloc` directly, which is ugly and wastes whole pages for tiny allocations. The bump allocator is the right middle ground.
- **POSIX-compat libc** — would force decisions about every POSIX corner (locales, sigaction, pthread, ...) before the kernel ABI is even stable. Hard no.
- **Using musl as the libc** — musl is glorious but assumes a Linux-ish syscall surface and POSIX-ish API. Adapting it would dwarf writing the small libc Skalapos actually needs.
- **Skipping a shell, using the kernel's `init=` as the entry point for tests** — fine for `make test`, but you want a shell for interactive debugging from day one.

## v2+ direction

- **Real `malloc`.** A simple slab/freelist hybrid backed by `vm_alloc` for large requests. Same libc surface; swap the implementation file.
- **stdio FILE\*.** Buffered I/O on top of raw handles. Useful but not required.
- **A second shell.** Maybe a Lispy one for scripting; maybe a more POSIX-ish one. Both are big enough projects to not pick now.
- **Coreutils-equivalents.** As needed; one at a time.
- **A test-runner binary.** For `just test`. Boots, runs a list of test programs from `/tests`, captures pass/fail, exits with summary.
- **A static-link userland for kernel-only debugging.** Not really useful; mentioned only because someone always asks.
