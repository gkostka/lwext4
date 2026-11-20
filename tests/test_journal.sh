# SPDX-License-Identifier: BSD-3-Clause
# ext4 with 1 KiB blocks and the smallest journal mke2fs makes: 1024
# blocks.
lwext4_mke2fs -t ext4 -b 1024 -J size=1 "$1" 32M
