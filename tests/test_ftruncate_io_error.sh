# SPDX-License-Identifier: BSD-3-Clause
# ext4 (extents) and ext2 (block map) images, 1 KiB blocks, one group each:
# a 64 KiB file and 64 small files in another directory.
dir="$1.d"
mkdir -p "$dir/other"
awk 'BEGIN { for (i = 0; i < 65536; i++) printf "%c", 65 + i % 26 }' >"$dir/file"
i=0
while [ $i -lt 64 ]; do
	echo $i >"$dir/other/$i"
	i=$((i + 1))
done
lwext4_mke2fs -t ext4 -b 1024 -I 256 -d "$dir" "$1" 4M
lwext4_mke2fs -t ext2 -b 1024 -I 256 -d "$dir" "$1.ext2" 4M
