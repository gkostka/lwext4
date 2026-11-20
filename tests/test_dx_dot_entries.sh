# SPDX-License-Identifier: BSD-3-Clause
# dir_index (the mke2fs default for ext4): new directories get an index.
lwext4_mke2fs -t ext4 -b 1024 "$1" 8M
