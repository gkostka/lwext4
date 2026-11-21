# SPDX-License-Identifier: BSD-3-Clause
# ext2 (block map), 1 KiB blocks: a 64 KiB file, blocks 12 and up are
# mapped through an indirect block.
dir="$1.d"
mkdir -p "$dir"
awk 'BEGIN { for (i = 0; i < 65536; i++) printf "%c", 65 + i % 26 }' >"$dir/file"
lwext4_mke2fs -t ext2 -b 1024 -d "$dir" "$1" 4M
