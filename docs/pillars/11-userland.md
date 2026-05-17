# Pillar 11 — Userland scope

## Goals

- v1 to ship minimal userland to exercise kernel. All else is stubbed.
- Skalapos-specific libc; not necessarily ISO-C conformant, or glibc compatable.

## v1 Components 

- **libc-core** (`userland/libc/`).
- **`/bin/sh`** as PID 1 (`userland/sh/`).
- **Utilities** in `userland/utils/`: `echo`, `cat`, `ls`, `pwd`, `mkdir`.


## Header naming convention

| Prefix | Meaning | Examples |
|---|---|---|
| `<skalops/...>` | Skalapos primitives | `<skalops/spawn.h>`, `<skalops/channel.h>`, `<skalops/handle.h>` |
| Unprefixed | ISO C surface | `<string.h>`, `<stdio.h>`, `<stdlib.h>`, `<stdint.h>`, `<stddef.h>` |
| `<sys/...>` | Low-level libc surface | `<sys/status.h>`, `<sys/syscall.h>` |

## libc-core contents

### Provided in v1

```c
// <string.h>
void* memcpy(void * dst, const void * src, uint64_t n);
void* memset(void * dst, uint64_t c, uint64_t n);
void* memmove(void * dst, const void * src, uint64_t n);
uint64_t memcmp(const void * a, const void * b, uint64_t n);
uint64_t strlen(const char* s);
uint64_t strcmp(const char * a, const char* b);
uint64_t strncmp(const char * a, const char* b, uint64_t n);
char* strchr(const char * s, uint64_t c);

// <stdio.h>
uint64_t printf (const char* fmt, ...);     // writes to handle 2 (stdout)
uint64_t fprintf(handle_t h, const char* fmt, ...);   // writes to any File handle
uint64_t puts(const char* s);
uint64_t fputs(const char* s, handle_t h);
uint64_t putchar(uint64_t c);
uint64_t putc(uint64_t c, handle_t h);

// <stdlib.h>
void * malloc(uint64_t n);
void free(void* p);                   
void abort(void) __attribute__((noreturn));
uint64_t atoi(const char* s);

// <skalops/check.h>
#define STATUS_OR_DIE(expr) ...   // exits with status_describe() on failure
```

### CRT (crt0)

`crt0.S` (per arch) is the first code that runs in a freshly-spawned process:

1. The kernel passes `argc`, `argv`, `envp` on the stack per the ELF ABI.
2. `crt0` zeroes BSS (linker provides `__bss_start`/`__bss_end`).
3. `crt0` initializes the bump allocator (assigns the arena base).
4. `crt0` calls `main(argc, argv, envp)`.
5. On return, `crt0` calls `proc_exit(main's_return_value)`.

## Bump allocator

```c
// userland/libc/src/stdlib/alloc.c
static uint8_t arena[64 * 1024];   // .bss, ~64 KiB
static uint64_t  cursor = 0;

void * malloc(uint64_t n) {
    if (n == 0) return NULL;
    uint64_t aligned = (n + 15) & ~(uint64_t)15;
    if (cursor + aligned > sizeof arena) return NULL;
    void* p = &arena[cursor];
    cursor += aligned;
    return p;
}
```

Placeholder for a standard dynamic allocator. Note that long-running utilities will leak. Prefer to avoid malloc, but a stub is a useful exercise.

## The shell

Minimal interactive shell at `/bin/sh`. Reads command and args from stdin, search current directory or `$PATH` (`/bin`, `/sbin`) for command if not absolute. Then, spawn process from file at the command's path.

Some of the following may be included in >=v2: pipes, redirections, globbing, environment substitution, command substitution, conditionals, loops, functions, history, and line editing.

## Utilities

### v1

- `echo` 
- `cat` 
- `ls` 
- `pwd` 
`mkdir`

### v2
- `cp`
- `mv`
- `rm`

Others not listed.

## Schema-driven generation

Two files in [`schemas/`](../../schemas/) are the source of truth:

- `syscalls.toml` — every syscall: number, name, args (typed), return shape.
- `status.toml` — every status code: number, name, brief description, subsystem.

`tools/gen_syscalls.py` reads both and emits:

- Kernel: `kernel/syscall/dispatch_generated.c` (number → handler table), `kernel/syscall/numbers_generated.h`.
- Userland: `userland/libc/src/sys/syscall_generated.c` (one wrapper function per syscall, with the typed signature), `userland/libc/include/sys/syscall.h`, `userland/libc/include/sys/status.h`.
- Per-class device headers: not generated from schemas (their opcodes are in `<skalops/dev/<class>.h>` and are part of the public API).

Run `just codegen` after editing the schemas. The generated files are checked into git so the tree builds without Python at hand — but they MUST be regenerated whenever the schemas change.

## v2+ direction

- A full dynamic allocator
- Buffered I/O on top of raw handles.
- Implement coreutils as needed
- Userland test infra.
