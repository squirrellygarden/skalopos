# Dev setup

This document covers what you need installed on a Linux host to build and run Skalapos. macOS works with Homebrew analogs; Windows users want WSL2 or the [Docker container](#docker-route).

## Quick check

If you can run all of these without errors, you're ready:

```
x86_64-elf-gcc   --version
aarch64-elf-gcc  --version
qemu-system-x86_64 --version          # need 8.0+ for aarch64 raspi4b
qemu-system-aarch64 --version
gdb-multiarch    --version
ninja            --version
just             --version
python3          --version            # 3.11+ for tomllib stdlib; 3.9+ with `pip install tomli`
```

Then:

```
just build x86_64
just qemu x86_64
```

You should see PID 1's shell prompt on the serial console. Ctrl-A X to quit QEMU.

## Installing prerequisites

### Cross GCC toolchains (the hardest piece)

Skalapos targets bare-metal ELF, not Linux. The distro packages `gcc-x86-64-linux-gnu` / `gcc-aarch64-linux-gnu` target Linux ABI and **will fight you** — do not use them.

Three options, in increasing order of effort:

**(a) Use the Docker container.** Skip this section; see [Docker route](#docker-route) below.

**(b) Prebuilt toolchains.** Download `x86_64-elf` and `aarch64-elf` toolchains from a prebuilt source such as [bootlin's toolchain page](https://toolchains.bootlin.com/). Extract under `.toolchains/`, prepend their `bin/` directories to `$PATH`.

**(c) Build from source via `crosstool-ng`.**

```
git clone https://github.com/crosstool-ng/crosstool-ng.git
cd crosstool-ng
./bootstrap && ./configure --prefix=$HOME/.local && make && make install

# In a scratch directory:
ct-ng x86_64-elf
ct-ng build       # ~20-30 min

# Repeat for aarch64-elf in a separate scratch dir:
ct-ng aarch64-elf
ct-ng build
```

The resulting toolchains live under `~/x-tools/{x86_64-elf,aarch64-elf}/bin/`. Add both to your `PATH`.

### QEMU

Need 8.0 or later for the `raspi4b` machine. Most distros ship recent QEMU; if yours doesn't, build from source.

- Debian / Ubuntu: `sudo apt install qemu-system-x86 qemu-system-arm`
- Fedora: `sudo dnf install qemu-system-x86 qemu-system-aarch64`
- Arch: `sudo pacman -S qemu-full`

### Everything else

- **`ninja`** — `apt install ninja-build` / `dnf install ninja-build` / `pacman -S ninja`
- **`just`** — install via `cargo install just` or your distro's package manager (recent Debian/Ubuntu have it as `just`)
- **`gdb-multiarch`** — `apt install gdb-multiarch` (Debian/Ubuntu); on Fedora/Arch the normal `gdb` package handles multiple targets if built with `--enable-targets=all`
- **`python3`** — 3.11+ ideal (stdlib `tomllib`); 3.9+ workable with `pip install tomli`

## Docker route

If you'd rather not install a cross toolchain locally:

```
just docker-build           # one-time; builds the dev image
just docker-shell           # interactive shell with everything installed, repo mounted at /work
# inside the container:
just build x86_64
just qemu x86_64
```

The container image is defined in [`ci/docker/Dockerfile`](../ci/docker/Dockerfile).

## Verifying ASLR and DEP at boot

After `just qemu x86_64` boots into a shell, you can sanity-check that the hardening defaults are working. v1 doesn't ship any introspection utilities for this yet; add a `vmcheck` utility in `userland/utils/` when it becomes useful.

## Common problems

| Symptom | Likely cause | Fix |
|---|---|---|
| `x86_64-elf-gcc: command not found` | Toolchain not in `PATH` | Add the toolchain's `bin/` to `PATH` or use the Docker route. |
| QEMU complains about `raspi4b` | QEMU < 8.0 | Upgrade QEMU. Workaround: edit the aarch64 justfile recipe to use `-M raspi3b` and adjust the kernel build accordingly. |
| Linker error about `_start` | Missing crt0 in linker invocation | Should not happen in the canonical build; if it does, file is `userland/libc/src/sys/crt0.S` and must be the first object on the userland link line. |
| `ninja: error: build.ninja:N: unknown target 'all'` | Stub `gen_build.py` not yet implemented | Expected pre-v1. The build system is scaffolded; rules will be populated as code is written. |
| Kernel triple-faults at boot on x86 | Lots of possible causes; common ones are GDT/IDT setup or stack overflow during early init | Boot with `just debug x86_64` + `just gdb x86_64`, break at `kmain`, single-step. |

## Environment variables the build honors

| Variable | Default | Meaning |
|---|---|---|
| `CC_X86_64`   | `x86_64-elf-gcc`  | x86_64 cross C compiler |
| `CC_AARCH64`  | `aarch64-elf-gcc` | aarch64 cross C compiler |
| `BUILD_TYPE`  | `release`         | `release` or `debug` (debug = -O0 -g) |
| `V`           | unset             | If set, ninja prints full commands (`ninja -v`). |
