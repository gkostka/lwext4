#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# macOS job. The one CI job that does not run in a container (see
# ci/README.md): macOS cannot run in docker, so this script runs directly on
# a macOS machine, e.g. the macos-latest GitHub Actions runner
# (.github/workflows/macos.yml). It needs the Xcode command line tools
# (clang, Apple ld), cmake and e2fsprogs from Homebrew, whose tools are
# keg-only:
#
#   brew install e2fsprogs
#   PATH="$(brew --prefix e2fsprogs)/sbin:$PATH" ci/host/macos.sh [clang|asan-ubsan]
#
# Builds the library, the fs_test tools and the regression tests with the
# generic toolchain (Apple clang), runs the CTest suite and the e2fsprogs
# round trip (ci/scripts/fs-roundtrip.sh: images made by mke2fs are
# modified by lwext4-generic and checked by e2fsck -fn; images made by
# lwext4-mkfs are checked by e2fsck -fn), then formats a real disk device
# (an image attached with hdiutil) with lwext4-mkfs.
set -eu

variant=${1:-clang}
case "$variant" in
clang)      sanitize= ;;
asan-ubsan) sanitize=address,undefined ;;
*) echo "unknown variant '$variant'" >&2; exit 2 ;;
esac

[ "$(uname -s)" = Darwin ] || { echo "this job runs on macOS" >&2; exit 2; }
for tool in cc cmake ctest mke2fs e2fsck debugfs; do
	command -v "$tool" >/dev/null 2>&1 ||
		{ echo "$tool not found (see the header of $0)" >&2; exit 2; }
done
sw_vers
uname -m
cc --version | head -n 1
cmake --version | head -n 1
mke2fs -V 2>&1 | head -n 1

. ci/scripts/common.sh
build=$(ci_build_dir "macos-$variant")

# LeakSanitizer is not supported on macOS; ASan and UBSan are.
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake -S . -B "$build/b" \
	-DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DCMAKE_BUILD_TYPE=Release \
	-DLWEXT4_SANITIZE="$sanitize"
cmake --build "$build/b" -j"$(ci_jobs)"
file "$build/b/fs_test/lwext4-mkfs"
ctest --test-dir "$build/b"
ci/scripts/fs-roundtrip.sh "$build/b"

# lwext4-mkfs as a mkfs.ext4 for macOS (gkostka/lwext4#59): format a disk
# device, not just an image file. hdiutil attaches a 32 MiB raw image as
# /dev/diskN without mounting it; lwext4-mkfs formats the raw device
# /dev/rdiskN, lwext4-generic writes to the block device /dev/diskN, and
# e2fsck -fn checks the device and, after detaching, the image.
echo "== disk device"
disk_img=$PWD/$build/disk.img
dd if=/dev/null of="$disk_img" bs=1048576 seek=32 2>/dev/null
dev=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount \
	"$disk_img" | awk 'NR == 1 { print $1 }')
trap 'hdiutil detach "$dev" >/dev/null 2>&1 || true' EXIT
rdev=/dev/r${dev#/dev/}
# The device nodes belong to root unless the attaching user owns them.
as_root=
[ -w "$rdev" ] && [ -w "$dev" ] || as_root=sudo
ls -l "$dev" "$rdev"
$as_root "$build/b/fs_test/lwext4-mkfs" -i "$rdev" -b 1024 -e 4
$as_root e2fsck -fn "$rdev"
$as_root "$build/b/fs_test/lwext4-generic" -i "$dev" -d 50 -c 8 -s 65536 \
	>"$build/disk-generic.log" 2>&1 ||
	{ tail -40 "$build/disk-generic.log"; exit 1; }
$as_root e2fsck -fn "$dev"
hdiutil detach "$dev"
trap - EXIT
e2fsck -fn "$disk_img"
echo "disk device: lwext4-mkfs and lwext4-generic on $dev, e2fsck clean"
