# SPDX-License-Identifier: BSD-3-Clause
# ext2: neither extents nor extended attributes needed.
lwext4_mke2fs -t ext2 "$1" 8M
