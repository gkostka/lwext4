# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks, one group: a 64 KiB file (one extent in the inode)
# and a 64 KiB file with a block in every other block of its first 128 KiB
# (32 extents, a leaf block below the inode).
dir="$1.d"
mkdir -p "$dir"
awk 'BEGIN { for (i = 0; i < 65536; i++) printf "%c", 65 + i % 26 }' >"$dir/file"
i=0
while [ $i -lt 32 ]; do
	printf 'block %d\n' $((i * 2)) |
		dd of="$dir/sparse" bs=1024 seek=$((i * 2)) conv=notrunc 2>/dev/null
	i=$((i + 1))
done
lwext4_mke2fs -t ext4 -b 1024 -d "$dir" "$1" 4M
debugfs -R 'stat /sparse' "$1" 2>/dev/null | grep -q '(ETB0)' ||
	{ echo "sparse: no extent tree below the inode" >&2; exit 1; }
