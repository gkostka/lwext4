# arm-none-eabi

Builds on [base](../base). Adds bare-metal ARM tooling:

| Package | Why |
|---|---|
| gcc-arm-none-eabi, libnewlib-arm-none-eabi | compiler and C library for microcontrollers without an OS |
| qemu-system-arm | emulates the Arm MPS2 boards (Cortex-M3/M4/M7 FPGA images) the firmware boots on |
| qemu-user | `qemu-arm` runs the arm-sim build as a program, with ARM semihosting |

Used by

- `ci/jobs/cortex-m.sh <cortex-m0|cortex-m0+|cortex-m3|cortex-m4|cortex-m4f|cortex-m7>`:
  builds `tests/baremetal` with `toolchain/<cpu>.cmake` and boots it on an
  MPS2 board. `tests/baremetal/startup.c` and `mps2.ld` are the entire
  "board support": vector table, RAM initialisation and semihosting for the
  console and the exit status.
- `ci/jobs/arm-sim.sh`: the same firmware for the classic ARM7TDMI with
  newlib's `rdimon` semihosting library, run by `qemu-arm`.

The firmware runs unit tests (byte order, crc32c/crc16, bitmaps, htree
hashes checked against e2fsprogs), mounts ext2/ext4 images made by `mke2fs`
at build time, and formats, fills and remounts a 2 MiB RAM disk with
`ext4_mkfs`.

```sh
ci/run.sh cortex-m cortex-m4f
ci/run.sh --shell arm-none-eabi
```

What it teaches: running and debugging embedded C without hardware, and how
little is needed to boot a Cortex-M.
