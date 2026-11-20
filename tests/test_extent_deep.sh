# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks: extent tree blocks hold 84 entries.
lwext4_mke2fs -t ext4 -b 1024 -N 1024 "$1" 40M
