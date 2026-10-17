# SPDX-License-Identifier: BSD-3-Clause
# Small ext4 images, one without and one with a journal.
lwext4_mke2fs -t ext4 -O ^has_journal -b 1024 -m 0 "$1" 2M
lwext4_mke2fs -t ext4 -b 1024 -m 0 "$1.journal" 4M
