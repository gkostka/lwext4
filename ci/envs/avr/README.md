# avr

Builds on [base](../base). Adds the 8-bit AVR toolchain and a simulator:

| Package | Why |
|---|---|
| gcc-avr, binutils-avr, avr-libc | compiler, linker and C library for AVR microcontrollers |
| simavr | cycle based AVR simulator; prints the UART output and stops when the CPU sleeps with interrupts disabled |

Used by `ci/jobs/avr.sh`:

1. `toolchain/avrxmega7.cmake` builds the library for XMEGA parts. simavr
   has no XMEGA core, so this build is compiled and linked only.
2. `toolchain/atmega1284.cmake` builds `tests/baremetal` for the largest
   AVR simavr supports (ATmega1284: 16 KiB RAM, 128 KiB flash) and runs it.
   `int` is 16 bits here, which is where portability bugs in checksum,
   hash and bitmap code show up. The firmware checks the byte order
   helpers, crc32c/crc16, bitmaps and all six htree hash variants against
   e2fsprogs, then mounts an ext4 image (metadata_csum, extents, 64bit,
   htree) made by `mke2fs` read-only from flash. The flash only has room
   for one image next to lwext4, and there is no RAM for a RAM disk: the
   ext2 image and `ext4_mkfs`/write tests run on the larger targets.

simavr cannot pass an exit status on; CTest decides on the `PASS`/`FAIL`
line.

```sh
ci/run.sh avr
ci/run.sh --shell avr
```

What it teaches: what a filesystem needs at minimum (about 128 KiB of code
and 16 KiB of RAM), and how to test on an 8-bit CPU without hardware.
