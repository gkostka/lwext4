# SPDX-License-Identifier: BSD-3-Clause
# ext4 without metadata_csum (extents, dir_index), with a small directory
# tree made by mke2fs, whose directories are not indexed.
dir="$1.d"
mkdir -p "$dir/pre/child"
printf 'made by mke2fs\n' >"$dir/pre/child/f"
lwext4_mke2fs -t ext4 -O ^metadata_csum -b 1024 -d "$dir" "$1" 16M
