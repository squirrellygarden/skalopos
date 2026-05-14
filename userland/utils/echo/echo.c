// userland/utils/echo/echo.c — /bin/echo
//
// Print arguments separated by spaces, newline at end. No flags.

#include <skalops/spawn.h>
// #include <stdio.h>      // fputs, putchar
// #include <skalops/handle.h> // STDOUT

int main(int argc, char** argv, char** envp) {
    (void)envp;
    // TODO:
    // for (int i = 1; i < argc; i++) {
    //     fputs(argv[i], STDOUT);
    //     if (i + 1 < argc) putc(' ', STDOUT);
    // }
    // putc('\n', STDOUT);
    (void)argc; (void)argv;
    proc_exit(0);
}
