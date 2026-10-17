# Extent headers whose eh_max/eh_entries exceed the capacity of the buffer
# holding them (the 60 byte i_block for the root, one block for tree blocks).
dir="$1.d"
mkdir -p "$dir"
printf 'hello extents\n' > "$dir/root_huge"
printf 'hello extents\n' > "$dir/root_five"
# Sparse file with 6 extents: needs a depth 1 tree with a separate leaf block.
python3 - "$dir/leaf_huge" <<'PY'
import sys
with open(sys.argv[1], 'wb') as f:
	for i in range(0, 12, 2):
		f.seek(i * 4096)
		f.write(b'chunk%d' % i)
PY
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1" 8M

# i_block[0] = magic | entries << 16, i_block[1] = max | depth << 16
# root_huge: eh_entries = eh_max = 0xffff, depth 0.
debugfs -w -R 'sif root_huge block[0] 0xfffff30a' "$1"
debugfs -w -R 'sif root_huge block[1] 0x0000ffff' "$1"
# root_five: eh_entries = eh_max = 5, one more than i_block can hold.
debugfs -w -R 'sif root_five block[0] 0x0005f30a' "$1"
debugfs -w -R 'sif root_five block[1] 0x00000005' "$1"
# leaf_huge: eh_entries = eh_max = 0xffff in the leaf block.
leaf=$(debugfs -R 'ex leaf_huge' "$1" 2>/dev/null |
	awk '$1 == "0/" { print $8 }')
[ -n "$leaf" ]
debugfs -w -R "zap_block -o 2 -l 4 -p 0xff $leaf" "$1"
