# SPDX-License-Identifier: BSD-3-Clause
# ext2 (block map), 1 KiB blocks, one group: a 64 KiB file.
dir="$1.d"
mkdir -p "$dir"
awk 'BEGIN { for (i = 0; i < 65536; i++) printf "%c", 65 + i % 26 }' >"$dir/file"
lwext4_mke2fs -t ext2 -b 1024 -d "$dir" "$1" 4M
