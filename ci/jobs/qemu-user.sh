# SPDX-License-Identifier: BSD-3-Clause
# env: cross-linux
# Cross compile the generic target for another Linux architecture and run
# the CTest suite and the e2fsprogs round trip under qemu-user. mke2fs and
# e2fsck run natively, so images cross the byte order/word size boundary in
# both directions.
#
#   ci/run.sh qemu-user <arch>
arch=${1:?usage: qemu-user.sh <arch>}
cflags=
case "$arch" in
# 32-bit ARM, strict(er) alignment
armhf)    triple=arm-linux-gnueabihf;   qemu=qemu-arm ;;
aarch64)  triple=aarch64-linux-gnu;     qemu=qemu-aarch64 ;;
# 32-bit x86: 32-bit size_t/long, 4-byte aligned uint64_t
i686)     triple=i686-linux-gnu;        qemu=qemu-i386 ;;
riscv64)  triple=riscv64-linux-gnu;     qemu=qemu-riscv64 ;;
ppc64le)  triple=powerpc64le-linux-gnu; qemu=qemu-ppc64le ;;
# Big endian; the byte order comes from the compiler (__BYTE_ORDER__) ...
s390x)    triple=s390x-linux-gnu;       qemu=qemu-s390x ;;
powerpc)  triple=powerpc-linux-gnu;     qemu=qemu-ppc ;;
mips)     triple=mips-linux-gnu;        qemu=qemu-mips ;;
# ... and an explicit CONFIG_BIG_ENDIAN must keep working.
s390x-explicit-big-endian)
	triple=s390x-linux-gnu; qemu=qemu-s390x
	cflags=-DCONFIG_BIG_ENDIAN=1 ;;
*) echo "unknown arch '$arch'" >&2; exit 2 ;;
esac

ldflags=
# qemu-arm cannot map the 4 KiB aligned segments of 32-bit ARM shared
# libraries on hosts with larger pages (e.g. Raspberry Pi 5 kernels use
# 16 KiB pages): link statically there.
if [ "$qemu" = qemu-arm ] && [ "$(getconf PAGESIZE)" -gt 4096 ]; then
	ldflags=-static
fi

. ci/scripts/common.sh
build=$(ci_build_dir "qemu-user-$arch")
sysroot=/usr/$triple
$triple-gcc --version | head -n 1
$qemu --version | head -n 1

cmake -S . -B "$build" \
	-DCMAKE_TOOLCHAIN_FILE=toolchain/linux-cross.cmake \
	-DCROSS_TRIPLE="$triple" \
	-DCROSS_EMULATOR="$qemu" \
	-DCROSS_SYSROOT="$sysroot" \
	-DCROSS_CFLAGS="$cflags" \
	-DCROSS_LDFLAGS="$ldflags" \
	-DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(ci_jobs)"
file "$build/fs_test/lwext4-generic"
$qemu -L "$sysroot" "$build/fs_test/lwext4-generic" -x
ctest --test-dir "$build"
ci/scripts/fs-roundtrip.sh "$build" $qemu -L "$sysroot"
