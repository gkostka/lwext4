# SPDX-License-Identifier: BSD-3-Clause
# 1 KiB blocks and metadata_csum: small index blocks with checksum tails.
lwext4_mke2fs -t ext4 -b 1024 -N 16384 "$1" 32M
