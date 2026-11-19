# SPDX-License-Identifier: BSD-3-Clause
# env: msp430
# platform: linux/amd64
# 16-bit MSP430 (16-bit int and pointers, 20-bit in the large model).
#
# 1. toolchain/msp430.cmake: the library for the MSP430G2210 (256 bytes of
#    RAM, 2 KiB of flash: far too small to run lwext4), compiled only.
# 2. toolchain/msp430-sim.cmake: MSP430X large memory model, tests/baremetal
#    on the GDB simulator (msp430-elf-run): unit tests, read-only mounts of
#    mke2fs made images and ext4_mkfs + read/write tests on a 256 KiB RAM
#    disk (without a journal, which needs at least 1024 blocks).
. ci/scripts/common.sh
msp430-elf-gcc --version | head -n 1

build=$(ci_build_dir msp430g2210)
cmake -S . -B "$build" -DCMAKE_TOOLCHAIN_FILE=toolchain/msp430.cmake \
	-DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(ci_jobs)"

build=$(ci_build_dir msp430-sim)
cmake -S . -B "$build" -DCMAKE_TOOLCHAIN_FILE=toolchain/msp430-sim.cmake \
	-DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(ci_jobs)"
msp430-elf-size "$build/tests/baremetal/baremetal_test.elf"
ctest --test-dir "$build" -V
