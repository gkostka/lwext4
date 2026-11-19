# cross-linux

Builds on [base](../base). Adds a GCC cross compiler and C library for every
Linux architecture in the CI matrix, and qemu-user to run the results:

| Architecture | Triple | Why it is in the matrix |
|---|---|---|
| armhf | arm-linux-gnueabihf | 32-bit ARM, alignment sensitive |
| aarch64 | aarch64-linux-gnu | 64-bit ARM |
| x86_64 | x86_64-linux-gnu | release packages (and a cross build from arm64 hosts) |
| i686 | i686-linux-gnu | 32-bit `long`/`size_t`, 4-byte aligned `uint64_t` |
| riscv64 | riscv64-linux-gnu | another 64-bit RISC ABI |
| ppc64le | powerpc64le-linux-gnu | 64-bit POWER, little endian |
| s390x | s390x-linux-gnu | 64-bit big endian |
| powerpc | powerpc-linux-gnu | 32-bit big endian |
| mips | mips-linux-gnu | 32-bit big endian, different errno values |

The compiler for the build machine's own architecture is Debian's native
`gcc` (it also answers to `<triple>-gcc`), so the image builds natively on
both amd64 and arm64.

Used by `ci/jobs/qemu-user.sh <arch>` with `toolchain/linux-cross.cmake`:
the test programs run under `qemu-<arch>` (CTest uses it as
`CMAKE_CROSSCOMPILING_EMULATOR`), while `mke2fs`/`e2fsck` run natively. So
filesystem images cross the byte order and word size boundary in both
directions, which catches byte swapping and structure layout bugs.

```sh
ci/run.sh qemu-user s390x
ci/run.sh --shell cross-linux
```

Note: on hosts with 16 KiB pages (Raspberry Pi 5 kernels) qemu-arm cannot
map 32-bit ARM shared libraries, so the job links armhf statically there.

What it teaches: how to test endianness and ABI portability of C code
without owning the hardware.
