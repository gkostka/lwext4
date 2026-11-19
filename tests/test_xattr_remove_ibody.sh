# SPDX-License-Identifier: BSD-3-Clause
# ext4 with 256 byte inodes: small xattrs live in the inode body.
lwext4_mke2fs -t ext4 -b 1024 -I 256 "$1" 8M
