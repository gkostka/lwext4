#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Cross-check lwext4 against e2fsprogs using the fs_test tools.
#
# Usage: fs-roundtrip.sh <build dir> [emulator...]
#
# The lwext4 tools (lwext4-mkfs, lwext4-generic) run through the optional
# emulator (e.g. "qemu-s390x -L /usr/s390x-linux-gnu" or "wine"); mke2fs and e2fsck
# always run on the host. For every case:
#
#   1. an image is formatted, either by mke2fs on the host or by lwext4-mkfs,
#   2. e2fsck -fn verifies the fresh image (lwext4-mkfs cases only),
#   3. lwext4-generic mounts it and runs its directory and file tests,
#   4. e2fsck -fn verifies the image lwext4 has written to.
#
# This catches byte order and structure layout bugs in both directions
# (host-written images read by the target and target-written images read
# by the host), which the CTest suite alone does not.
set -eu

build="$1"
shift
# Emulator command line; word split on use (paths must not contain spaces).
emu="$*"
PATH="$PATH:/sbin:/usr/sbin"

mkfs_tool="$build/fs_test/lwext4-mkfs"
generic_tool="$build/fs_test/lwext4-generic"
if [ -f "$mkfs_tool.exe" ]; then # Windows build, run by e.g. "wine"
	mkfs_tool="$mkfs_tool.exe"
	generic_tool="$generic_tool.exe"
fi
work="$build/roundtrip"
rm -rf "$work"
mkdir -p "$work"

failed=0
passed=0

fail()
{
	echo "FAIL: $case_name: $*"
	failed=$((failed + 1))
}

fsck_image()
{
	if ! e2fsck -fn "$img" >"$work/$case_name.fsck.log" 2>&1; then
		fail "e2fsck after $1"
		sed 's/^/    /' "$work/$case_name.fsck.log" | head -40
		return 1
	fi
}

run_generic()
{
	if ! $emu "$generic_tool" -i "$img" -d 50 -c 8 -s 65536 \
	    >"$work/$case_name.generic.log" 2>&1; then
		fail "lwext4-generic"
		tail -40 "$work/$case_name.generic.log" | sed 's/^/    /'
		return 1
	fi
}

# Image formatted on the host by mke2fs: <name> <mke2fs options...>
host_mkfs_case()
{
	case_name="mke2fs-$1"
	shift
	img="$work/$case_name.img"
	echo "== $case_name"
	# Newer e2fsprogs enable features lwext4 does not support.
	mke2fs -q -F -O ^orphan_file,^metadata_csum_seed "$@" "$img" 32M \
		>/dev/null 2>&1 ||
		mke2fs -q -F "$@" "$img" 32M >/dev/null
	run_generic && fsck_image "lwext4-generic" && passed=$((passed + 1))
	return 0
}

# Image formatted by lwext4-mkfs on the target: <ext level>
#
# Only 1 KiB blocks on 32 MiB, i.e. four full block groups: lwext4-mkfs
# currently computes the free block count of a partial last block group as
# if it were full (e2fsck: "Free blocks count wrong"), even on x86_64, and
# that would mask architecture specific bugs.
target_mkfs_case()
{
	case_name="lwext4-mkfs-ext$1-1024"
	img="$work/$case_name.img"
	echo "== $case_name"
	truncate -s 32M "$img"
	if ! $emu "$mkfs_tool" -i "$img" -b 1024 -e "$1" \
	    >"$work/$case_name.mkfs.log" 2>&1; then
		fail "lwext4-mkfs"
		tail -40 "$work/$case_name.mkfs.log" | sed 's/^/    /'
		return 0
	fi
	fsck_image "lwext4-mkfs" && run_generic &&
		fsck_image "lwext4-generic" && passed=$((passed + 1))
	return 0
}

host_mkfs_case ext2-1024 -t ext2 -b 1024
host_mkfs_case ext3-1024 -t ext3 -b 1024
host_mkfs_case ext4-1024 -t ext4 -b 1024
host_mkfs_case ext4-4096 -t ext4 -b 4096
# ext4 without metadata_csum: separates checksum handling from the rest of
# the ext4 feature set (extents, flex_bg, 64bit, htree, ...).
host_mkfs_case ext4-nocsum-4096 -t ext4 -b 4096 -O ^metadata_csum

target_mkfs_case 2
target_mkfs_case 3
target_mkfs_case 4

echo "fs-roundtrip: $passed passed, $failed failed"
[ "$failed" -eq 0 ]
