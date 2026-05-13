// userland/libc/include/skl/spawn.h
//
// Process and thread creation. See docs/pillars/02-spawn.md.

#ifndef SKL_SPAWN_H
#define SKL_SPAWN_H

#include <stdint.h>
#include <stddef.h>
#include <skl/handle.h>
#include <sys/status.h>

typedef struct {
    handle_t root_dir;       // child's ROOT_DIR. HANDLE_INVALID → inherit.
    handle_t cwd_dir;        // child's CWD_DIR.  HANDLE_INVALID → inherit.
    handle_t control_chan;   // child's control channel. HANDLE_INVALID → kernel creates.
    uint32_t flags;          // reserved
} spawn_opts_t;

/// Create a new process from an ELF image given as a File handle.
/// `handles_to_pass[i]` becomes handle (i+1) in the child.
status_t proc_spawn(handle_t image,
                    const char* const* argv,
                    const char* const* envp,
                    const handle_t* handles_to_pass, size_t handles_count,
                    const spawn_opts_t* opts,
                    handle_t* out_proc_h);

/// Terminate the calling process. Never returns.
_Noreturn void proc_exit(int32_t status_code);

/// Block until `proc_h` exits.
status_t proc_wait(handle_t proc_h, int32_t* out_exit_code);

/// Replace the calling process's ROOT_DIR with `dir_h`. Irreversible.
status_t proc_chroot(handle_t dir_h);

/// Set the calling process's CWD_DIR from a directory handle.
status_t proc_chdir_h(handle_t dir_h);

#endif
