# SPDX-License-Identifier: BSD-3-Clause
# Block mapped directories (no extents) with dir_index: ext2 with 1 KiB
# blocks ($1) and ext3 with 4 KiB blocks ($1.ext3), each with a tree d/
# (files, a subdirectory, an empty directory) and /keep.
dir="$1.d"
mkdir -p "$dir/d/sub" "$dir/d/empty"
echo one >"$dir/d/f1"
echo two >"$dir/d/sub/f2"
echo keep >"$dir/keep"
lwext4_mke2fs -t ext2 -b 1024 -O dir_index -d "$dir" "$1" 8M
lwext4_mke2fs -t ext3 -b 4096 -O dir_index -d "$dir" "$1.ext3" 16M
