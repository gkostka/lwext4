# SPDX-License-Identifier: BSD-3-Clause
# ext4 with 256 byte inodes (attributes in the inode body) and ext2 with
# 128 byte inodes (attributes in the xattr block).
lwext4_mke2fs -t ext4 -b 1024 -I 256 "$1" 8M
lwext4_mke2fs -t ext2 -b 1024 -I 128 "$1.ext2" 8M
