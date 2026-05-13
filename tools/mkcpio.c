// tools/mkcpio.c — host tool that builds an initramfs CPIO from a manifest.
//
// Reads initramfs/manifest.txt (target_path  source_path lines) and emits a
// newc-format CPIO archive that the kernel's initramfs FS will mount.
//
// Compiled with the host's regular cc, NOT the cross toolchain.
//
// Stub. Real implementation is ~200 lines of straightforward C.
// Alternative: use libarchive's `bsdtar --format=newc` from the build
// system and skip writing this entirely. Document the choice when the
// build system is fleshed out.

#include <stdio.h>

int main(int argc, char** argv) {
    fprintf(stderr, "mkcpio: not yet implemented (stub)\n");
    return 2;
}
