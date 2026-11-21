# SPDX-License-Identifier: BSD-3-Clause
# ext4 with 64 bit block numbers (64 byte group descriptors), 1 KiB blocks.
dir="$1.d"
mkdir -p "$dir"
echo hello >"$dir/file"
lwext4_mke2fs -t ext4 -b 1024 -O 64bit,^metadata_csum -d "$dir" "$1" 16M
