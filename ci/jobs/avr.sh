# SPDX-License-Identifier: BSD-3-Clause
# env: avr
# 8-bit AVR (16-bit int, 8-bit registers).
#
# 1. toolchain/avrxmega7.cmake: the library for XMEGA parts. simavr has no
#    XMEGA core, so this one is compiled and linked only.
# 2. toolchain/atmega1284.cmake: tests/baremetal on a simulated ATmega1284
#    (simavr): byte order, checksums, bitmaps and htree hashes against
#    e2fsprogs values, and a read-only mount of a mke2fs made ext4 image
#    kept in flash (flash has room for one image; ext2 with indirect blocks
#    runs on the other targets). 16 KiB of RAM leaves no room for a RAM
#    disk, so the ext4_mkfs/write tests only run on the larger targets.
. ci/scripts/common.sh
avr-gcc --version | head -n 1

build=$(ci_build_dir avrxmega7)
cmake -S . -B "$build" -DCMAKE_TOOLCHAIN_FILE=toolchain/avrxmega7.cmake \
	-DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(ci_jobs)"

build=$(ci_build_dir atmega1284)
cmake -S . -B "$build" -DCMAKE_TOOLCHAIN_FILE=toolchain/atmega1284.cmake \
	-DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(ci_jobs)"
avr-size "$build/tests/baremetal/baremetal_test.elf"
ctest --test-dir "$build" -V
