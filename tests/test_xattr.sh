# SPDX-License-Identifier: BSD-3-Clause
# ext4 with 256 byte inodes (room for attributes in the inode body) and
# ext2 with 128 byte inodes (xattr block only), each with a directory made
# by mke2fs.
dir="$1.d"
mkdir -p "$dir/dir"
lwext4_mke2fs -t ext4 -b 4096 -I 256 -d "$dir" "$1" 16M
lwext4_mke2fs -t ext2 -b 1024 -I 128 -d "$dir" "$1.ext2" 8M
