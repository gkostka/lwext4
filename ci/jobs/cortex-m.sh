# SPDX-License-Identifier: BSD-3-Clause
# env: arm-none-eabi
# Build lwext4 and tests/baremetal with one of the Cortex-M toolchain files
# and run the firmware on an emulated MPS2 board (qemu-system-arm, ARM
# semihosting for the console and the exit status).
#
#   ci/run.sh cortex-m <cortex-m0|cortex-m0+|cortex-m3|cortex-m4|cortex-m4f|cortex-m7>
#
# ARMv6-M builds (M0/M0+) run on the Cortex-M3 AN385; QEMU has no Cortex-M0
# board with enough RAM for the 2 MiB RAM disk.
toolchain=${1:?usage: cortex-m.sh <toolchain>}
case "$toolchain" in
cortex-m0|cortex-m0+|cortex-m3) machine=mps2-an385 ;;
cortex-m4|cortex-m4f)           machine=mps2-an386 ;;
cortex-m7)                      machine=mps2-an500 ;;
*) echo "unknown toolchain '$toolchain'" >&2; exit 2 ;;
esac

. ci/scripts/common.sh
build=$(ci_build_dir "$toolchain")
arm-none-eabi-gcc --version | head -n 1
qemu-system-arm --version | head -n 1

cmake -S . -B "$build" \
	-DCMAKE_TOOLCHAIN_FILE="toolchain/$toolchain.cmake" \
	-DCMAKE_BUILD_TYPE=Release \
	-DLWEXT4_QEMU_MACHINE="$machine"
cmake --build "$build" -j"$(ci_jobs)"
arm-none-eabi-size "$build/tests/baremetal/baremetal_test.elf"
ctest --test-dir "$build" -V
