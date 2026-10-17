# SPDX-License-Identifier: BSD-3-Clause
# ext2 with 4 KiB blocks: the symlink's block number is a direct block
# pointer in the inode.
lwext4_mke2fs -t ext2 -b 4096 "$1" 8M
