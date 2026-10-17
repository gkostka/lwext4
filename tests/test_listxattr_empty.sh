# SPDX-License-Identifier: BSD-3-Clause
lwext4_mke2fs -t ext4 -b 1024 -I 256 "$1" 8M
