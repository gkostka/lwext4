#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Exercise the legacy client/server suites using the native CI build.
set -eu
. "$(dirname "$0")/lib.sh"

cd "$TOP_DIR"

step "Legacy client/server tests (128 MiB images)"
make -j1 test LWEXT4_BUILD_DIR="$BUILD"
for fs in ext2 ext3 ext4; do
	fsck_clean "ext_images/$fs" "$fs after make test"
	[ "$(dir_names "ext_images/$fs" / | tr '\n' ' ')" = "lost+found " ] ||
		die "$fs: make test left files behind"
done

step "Legacy client/server tests (1 GiB images)"
make -j1 test_all LWEXT4_BUILD_DIR="$BUILD"
for fs in ext2 ext3 ext4; do
	[ "$(dir_names "ext_images/$fs" / | tr '\n' ' ')" = "lost+found " ] ||
		die "$fs: make test_all left files behind"
done
rm -rf ext_images

finish
