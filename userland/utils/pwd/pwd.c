// userland/utils/pwd/pwd.c — /bin/pwd
//
// Print the calling process's current directory path. Needs a libc helper to
// materialize a path from a directory handle (walk parents recursively); not
// trivial but small.

#include <skalops/spawn.h>

int main(int argc, char** argv, char** envp) {
    (void)argc; (void)argv; (void)envp;
    // TODO: render CWD_DIR to a string and write to stdout.
    proc_exit(0);
}
