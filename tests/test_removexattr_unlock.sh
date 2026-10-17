# SPDX-License-Identifier: BSD-3-Clause
# 128 byte inodes: the attributes go to the xattr block.
lwext4_mke2fs -t ext2 -b 1024 -I 128 "$1" 8M
