# msp430

Builds on [base](../base). Adds TI's MSP430 GCC (the Mitto Systems build,
pinned by version and SHA-256) with the device support files. It ships
`msp430-elf-run`, the GDB MSP430 simulator.

| Piece | Why |
|---|---|
| msp430-gcc 9.3.1.11 | compiler, newlib, simulator (`msp430-elf-run`) and its `-msim` runtime |
| msp430-gcc-support-files 1.212 | device headers and linker scripts (`MSP430_SUPPORT`) for `-mmcu=` builds |
| curl, bzip2, unzip | fetch and unpack the above |

TI only publishes x86_64 Linux binaries, so the job that uses this
environment has a `# platform: linux/amd64` header. On an arm64 machine it
needs Docker's amd64 emulation (binfmt_misc + qemu-user-static).

Used by `ci/jobs/msp430.sh`:

1. `toolchain/msp430.cmake` builds the library for the MSP430G2210 (256
   bytes of RAM, far too small to run lwext4): compile only.
2. `toolchain/msp430-sim.cmake` (MSP430X, large memory model) builds
   `tests/baremetal` and runs it in the simulator: unit tests, read-only
   mounts of `mke2fs` images and `ext4_mkfs` + read/write tests on a
   256 KiB RAM disk (journal-less: a journal needs at least 1024 blocks).

```sh
ci/run.sh msp430
ci/run.sh --shell msp430
```

What it teaches: a 16-bit target with 20-bit pointers, and using a
toolchain vendor's simulator as a test runner.
