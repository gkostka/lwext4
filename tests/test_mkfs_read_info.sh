# SPDX-License-Identifier: BSD-3-Clause
# ext4 image with a volume label that fills all 16 bytes of s_volume_name,
# so it has no terminating NUL on disk.
lwext4_mke2fs -t ext4 -b 1024 -L sixteen-chars-ok "$1" 4M
