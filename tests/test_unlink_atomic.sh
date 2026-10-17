# SPDX-License-Identifier: BSD-3-Clause
# ext4 with a journal
lwext4_mke2fs -t ext4 -b 1024 "$1" 8M
