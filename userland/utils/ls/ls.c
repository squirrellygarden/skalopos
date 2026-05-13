// userland/utils/ls/ls.c — /bin/ls
//
// List the entries of a directory. No flags in v1; just names, one per line.
// If no argument, list cwd.

#include <skl/spawn.h>
#include <skl/handle.h>
// #include <skl/fs.h>     // dir_open_at, dir_read

int main(int argc, char** argv, char** envp) {
    (void)envp;
    // TODO:
    //   handle_t dir = CWD_DIR;
    //   if (argc > 1) { open dir from argv[1] relative to CWD_DIR }
    //   loop dir_read printing names
    (void)argc; (void)argv;
    proc_exit(0);
}
