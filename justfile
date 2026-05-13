# Skalapos top-level commands.
# Run `just` with no args to see this list.
#
# Conventions:
#   - `arch` is x86_64 or aarch64.
#   - Build outputs land in build/<arch>/.
#   - Every recipe runs from the repo root.

default:
    @just --list

# ----- build / clean ----------------------------------------------------------

# Regenerate sources from schemas/ (syscall dispatch, status enums, libc wrappers).
codegen:
    python3 tools/gen_syscalls.py

# Regenerate build.ninja for an arch (called automatically by `build`).
ninja-gen arch:
    python3 tools/gen_build.py {{arch}}

# Build kernel + initramfs for an arch.
build arch: codegen (ninja-gen arch)
    ninja -C build/{{arch}}

# Same, but with -O0 -g (slower kernel, full GDB symbols).
build-debug arch: codegen (ninja-gen arch)
    BUILD_TYPE=debug ninja -C build/{{arch}}

# Remove all build artifacts.
clean:
    rm -rf build/

# Remove build artifacts AND cached cross toolchains (Docker image is unaffected).
clean-all: clean
    rm -rf .toolchains/

# ----- run --------------------------------------------------------------------

# Build and run in QEMU.
qemu arch: (build arch)
    @just _qemu-{{arch}}

# Build a debug kernel and run QEMU paused, waiting for GDB attach.
debug arch: (build-debug arch)
    @just _qemu-{{arch}}-debug

# Attach gdb-multiarch to a running `just debug <arch>` session.
gdb arch:
    gdb-multiarch \
        -ex "target remote :1234" \
        -ex "symbol-file build/{{arch}}/kernel.elf" \
        -ex "break kmain"

_qemu-x86_64:
    qemu-system-x86_64 \
        -m 512M \
        -kernel build/x86_64/kernel.elf \
        -initrd build/x86_64/initramfs.cpio \
        -append "init=/bin/sh" \
        -nographic \
        -serial mon:stdio \
        -no-reboot

_qemu-x86_64-debug:
    qemu-system-x86_64 \
        -m 512M \
        -kernel build/x86_64/kernel.elf \
        -initrd build/x86_64/initramfs.cpio \
        -append "init=/bin/sh" \
        -nographic \
        -serial mon:stdio \
        -no-reboot \
        -s -S

_qemu-aarch64:
    qemu-system-aarch64 \
        -M raspi4b \
        -m 1G \
        -kernel build/aarch64/kernel.elf \
        -initrd build/aarch64/initramfs.cpio \
        -append "init=/bin/sh" \
        -nographic \
        -serial mon:stdio

_qemu-aarch64-debug:
    qemu-system-aarch64 \
        -M raspi4b \
        -m 1G \
        -kernel build/aarch64/kernel.elf \
        -initrd build/aarch64/initramfs.cpio \
        -append "init=/bin/sh" \
        -nographic \
        -serial mon:stdio \
        -s -S

# ----- docker dev container ---------------------------------------------------

docker-build:
    docker build -t skalapos-dev -f ci/docker/Dockerfile .

docker-shell:
    docker run --rm -it -v "$(pwd)":/work -w /work skalapos-dev bash

docker-qemu arch="x86_64":
    docker run --rm -it -v "$(pwd)":/work -w /work skalapos-dev just qemu {{arch}}

# ----- introspection ----------------------------------------------------------

# Print the generated syscall numbers and status codes without writing files.
schema-print:
    python3 tools/gen_syscalls.py --print

# Show what `just build <arch>` will produce.
ninja-targets arch="x86_64":
    cat build/{{arch}}/build.ninja

# ----- placeholders (filled in as the system grows) ---------------------------

# Run the in-QEMU test runner (v2+).
test arch="x86_64":
    @echo "test runner not implemented in v1; see docs/pillars/10-build.md v2+"
