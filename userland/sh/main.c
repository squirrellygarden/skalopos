// userland/sh/main.c — /bin/sh, PID 1 in v1.
//
// Minimal interactive shell. ~300 LOC when complete. See docs/pillars/11-userland.md.
//
// What this will do:
//   - Print a prompt to stdout (handle 2).
//   - Read a line from stdin (handle 1) into a fixed-size BSS buffer.
//     (No malloc — bump allocator would exhaust over a long session.)
//   - Tokenize on whitespace. No quoting, no escapes, no globbing.
//   - First token is the command. If it contains '/', treat as path.
//     Otherwise search PATH = { "/bin", "/sbin" }.
//   - Built-ins: `cd <dir>`, `exit [n]`, `pwd`.
//   - For external commands: open_at the binary, proc_spawn with stdin/stdout/
//     stderr/root_dir/cwd_dir passed through, proc_wait, report exit status if
//     non-zero, loop.

#include <stdint.h>
#include <skalops/handle.h>
#include <skalops/spawn.h>

// Headers that don't yet exist but will:
//   #include <stdio.h>             // printf, fputs
//   #include <string.h>            // strlen, strcmp, memcpy
//   #include <skalops/fs.h>            // open_at, dir_open_at, mkdir_at, …

static char line_buf[1024];

int main(int argc, char** argv, char** envp) {
    (void)argc; (void)argv; (void)envp;

    // TODO: implement the loop.
    for (;;) {
        // print_prompt();
        // ssize_t n = read_line(line_buf, sizeof line_buf);
        // if (n <= 0) break;
        // dispatch(line_buf);
    }

    proc_exit(0);
}
