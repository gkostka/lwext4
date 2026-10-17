# SPDX-License-Identifier: BSD-3-Clause
# ext4 with metadata_csum (the mke2fs default) and 256 byte inodes.
lwext4_mke2fs -t ext4 -b 1024 -I 256 -O metadata_csum "$1" 8M
