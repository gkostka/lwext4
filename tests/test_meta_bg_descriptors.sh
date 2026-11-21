# SPDX-License-Identifier: BSD-3-Clause
# ext4 with meta_bg: 1 KiB blocks, 256 blocks per group, 96 groups, so
# three meta groups of 32 groups (32 byte descriptors, one descriptor
# block each), whose descriptor blocks are in the first, second and last
# group of each meta group.
lwext4_mke2fs -t ext4 -b 1024 -g 256 -N 2048 \
	-O meta_bg,^resize_inode,^flex_bg,^64bit "$1" 24M
dumpe2fs -h "$1" 2>/dev/null | grep -q 'Filesystem features:.*meta_bg' ||
	{ echo "$1: no meta_bg" >&2; exit 1; }
