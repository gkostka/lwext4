# SPDX-License-Identifier: BSD-3-Clause
# Small images with 1 KiB blocks and 1024 blocks per group, so that the
# allocators walk through several groups: ext4 (extents, flex_bg), ext2
# (block maps), and ext4 with few inodes.
lwext4_mke2fs -t ext4 -b 1024 -g 1024 -m 0 "$1" 4M
lwext4_mke2fs -t ext2 -b 1024 -g 1024 -m 0 "$1.ext2" 4M
lwext4_mke2fs -t ext4 -b 1024 -g 1024 -m 0 -N 64 "$1.inodes" 4M
