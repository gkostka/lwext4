# Sparse file with whole-block holes between data blocks, a partially
# written block after a hole run, and a hole covering the tail of the file.
# Block size is 4096; "D" is a data block, "." a hole:
#
#   block: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14
#          D . . D . D D . . . d  .  .  .  t
#
# Block 10 has data only in its middle and the file ends 100 bytes into
# block 14, which is a hole. mke2fs -d preserves the holes.
dir="$1.d"
mkdir -p "$dir"
python3 - "$dir/sparse.bin" <<'PY'
import sys

bs = 4096

def pattern(blk, n):
	return bytes(((blk * 31 + i * 7) % 255) + 1 for i in range(n))

with open(sys.argv[1], "wb") as f:
	f.truncate(14 * bs + 100)
	for blk in (0, 3, 5, 6):
		f.seek(blk * bs)
		f.write(pattern(blk, bs))
	f.seek(10 * bs + 1000)
	f.write(pattern(10, 2000))
PY
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1" 8M
