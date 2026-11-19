# SPDX-License-Identifier: BSD-3-Clause
# env: arm-none-eabi
# toolchain/arm-sim.cmake: arm-none-eabi with the GCC default CPU (ARM7TDMI,
# ARMv4T) and newlib's rdimon semihosting library. qemu-arm (user mode)
# implements ARM semihosting, so the tests/baremetal firmware runs as a
# plain program with a console, a heap and an exit status.
. ci/scripts/common.sh
build=$(ci_build_dir arm-sim)
arm-none-eabi-gcc --version | head -n 1
qemu-arm --version | head -n 1

cmake -S . -B "$build" -DCMAKE_TOOLCHAIN_FILE=toolchain/arm-sim.cmake \
	-DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(ci_jobs)"
arm-none-eabi-size "$build/tests/baremetal/baremetal_test.elf"
ctest --test-dir "$build" -V
