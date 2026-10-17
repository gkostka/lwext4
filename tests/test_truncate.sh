# SPDX-License-Identifier: BSD-3-Clause
# 1 KiB blocks: small indirect blocks and extent tree nodes.
lwext4_mke2fs -t ext4 -b 1024 "$1" 32M
lwext4_mke2fs -t ext2 -b 1024 "$1.ext2" 8M
