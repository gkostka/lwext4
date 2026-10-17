# SPDX-License-Identifier: BSD-3-Clause
# ext2, 1 KiB blocks: block maps with 256 entries per indirect block.
lwext4_mke2fs -t ext2 -b 1024 "$1" 8M
