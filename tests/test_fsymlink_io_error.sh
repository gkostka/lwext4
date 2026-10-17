# SPDX-License-Identifier: BSD-3-Clause
# Empty ext4 image, 1 KiB blocks, one group.
lwext4_mke2fs -t ext4 -b 1024 "$1" 4M
