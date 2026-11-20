# SPDX-License-Identifier: BSD-3-Clause
# 1 KiB blocks, one 64 KiB file with a known pattern.
dir="$1.d"
mkdir -p "$dir"
awk 'BEGIN { for (i = 0; i < 65536; i++) printf "%c", 65 + i % 26 }' >"$dir/data"
lwext4_mke2fs -t ext4 -b 1024 -d "$dir" "$1" 4M
