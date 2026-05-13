// userland/utils/cat/cat.c — /bin/cat
//
// For each argument, open and copy bytes to stdout. With no arguments,
// copy stdin to stdout.

#include <skl/spawn.h>
#include <skl/handle.h>
// #include <skl/fs.h>     // open_at, file_read, file_write
// #include <sys/status.h>

int main(int argc, char** argv, char** envp) {
    (void)envp;
    // TODO:
    //   handle_t cwd = CWD_DIR;
    //   if (argc <= 1) {
    //       copy_until_eof(STDIN, STDOUT);
    //   } else {
    //       for (int i = 1; i < argc; i++) {
    //           handle_t f;
    //           status_t s = open_at(cwd, argv[i], O_RDONLY, 0, &f);
    //           if (s != STATUS_OK) { /* print error, continue */ continue; }
    //           copy_until_eof(f, STDOUT);
    //           handle_close(f);
    //       }
    //   }
    (void)argc; (void)argv;
    proc_exit(0);
}
