# Skalapos development container.
#
# Builds on Debian 12 (bookworm). Installs cross GCC toolchains for x86_64-elf
# and aarch64-elf via bootlin prebuilts, plus QEMU, ninja, just, gdb-multiarch,
# Python 3.11 with tomllib.
#
# Build:  just docker-build
# Use:    just docker-shell      (interactive)
#         just docker-qemu x86_64  (one-shot build+run)
#
# This Dockerfile is a stub. The exact bootlin URLs are versioned; pin the
# exact toolchain versions when this is filled in.

FROM debian:bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        xz-utils \
        ninja-build \
        gdb-multiarch \
        qemu-system-x86 \
        qemu-system-arm \
        python3 \
        python3-pip \
        make \
        bsdmainutils \
        libarchive-tools \
        git \
    && rm -rf /var/lib/apt/lists/*

# `just` from cargo or distro:
RUN curl --proto '=https' --tlsv1.2 -sSf https://just.systems/install.sh \
    | bash -s -- --to /usr/local/bin

# TODO: download and extract bootlin x86_64-elf and aarch64-elf toolchains
# to /opt/cross. Add both bin/ dirs to PATH.
ENV PATH="/opt/cross/x86_64-elf/bin:/opt/cross/aarch64-elf/bin:${PATH}"

WORKDIR /work
CMD ["/bin/bash"]
