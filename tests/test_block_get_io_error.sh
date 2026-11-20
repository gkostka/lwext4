# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks: "sparse" has a data block in every other block of its
# first 64 KiB, 32 extents, so its extent tree has a leaf block below the
# inode (depth 1).
dir="$1.d"
mkdir -p "$dir"
i=0
while [ $i -lt 32 ]; do
	printf 'block %d\n' $((i * 2)) |
		dd of="$dir/sparse" bs=1024 seek=$((i * 2)) conv=notrunc 2>/dev/null
	i=$((i + 1))
done
lwext4_mke2fs -t ext4 -b 1024 -d "$dir" "$1" 4M
debugfs -R 'stat /sparse' "$1" 2>/dev/null | grep -q '(ETB0)' ||
	{ echo "sparse: no extent tree below the inode" >&2; exit 1; }
cp "$1" "$1.orig"
