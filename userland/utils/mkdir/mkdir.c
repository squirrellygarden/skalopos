// userland/utils/mkdir/mkdir.c — /bin/mkdir
//
// Create a directory at the given relative path. No -p in v1.

#include <skl/spawn.h>
#include <skl/handle.h>
// #include <skl/fs.h>   // mkdir_at

int main(int argc, char** argv, char** envp) {
    (void)envp;
    if (argc < 2) {
        // TODO: fputs("usage: mkdir <dir>\n", STDERR);
        proc_exit(1);
    }
    // TODO: mkdir_at(CWD_DIR, argv[1], 0755);
    (void)argv;
    proc_exit(0);
}
