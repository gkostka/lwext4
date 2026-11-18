# Index nodes (eh_depth > 0) with a bad entry count or depth in the inode root.
dir="$1.d"
mkdir -p "$dir"
# Sparse files with 6 extents: each needs a depth 1 tree.
python3 - "$dir" <<'PY'
import sys
for name in ('idx_ok', 'idx_empty', 'idx_huge', 'idx_deep'):
	with open(sys.argv[1] + '/' + name, 'wb') as f:
		for i in range(0, 12, 2):
			f.seek(i * 4096)
			f.write(b'chunk%d' % i)
PY
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1" 8M

# idx_ok is left intact.
# i_block[0] = magic | entries << 16, i_block[1] = max | depth << 16
# idx_empty: depth 1 root with eh_entries = 0 (index entry left in place).
debugfs -w -R 'sif idx_empty block[0] 0x0000f30a' "$1"
# idx_huge: depth 1 root with eh_entries = eh_max = 0xffff.
debugfs -w -R 'sif idx_huge block[0] 0xfffff30a' "$1"
debugfs -w -R 'sif idx_huge block[1] 0x0001ffff' "$1"
# idx_deep: root claims a depth beyond the ext4 maximum of 5.
debugfs -w -R 'sif idx_deep block[1] 0xffff0004' "$1"
