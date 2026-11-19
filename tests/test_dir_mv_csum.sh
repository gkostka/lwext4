# SPDX-License-Identifier: BSD-3-Clause
# metadata_csum without dir_index: directories are linear.
lwext4_mke2fs -t ext4 -b 1024 -O ^dir_index "$1" 8M
