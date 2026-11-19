# Issue #89: directories with a corrupted size or corrupted entries.
#
#   /zero     i_size forced to 0 (what the fuzzer image boils down to: its
#             root inode is all zeroes)
#   /hole     i_size claims two blocks, only the first one is mapped
#   /reclen0  ".." entry has rec_len 0
#   /overrun  rec_len of the last entry ends 4 bytes before the end of the
#             block, so the following entry header straddles the block end
#   /ok       untouched control directory
#
# metadata_csum stays enabled: lwext4 only warns on a directory block
# checksum mismatch, so it must not be what catches the corruption.
img="$1"
dir="$img.d"
for d in zero hole reclen0 overrun ok; do
	mkdir -p "$dir/$d"
	printf 'x' > "$dir/$d/a"
done
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$img" 8M

debugfs -w -R "sif /zero size 0" "$img" >/dev/null 2>&1
debugfs -w -R "sif /hole size 8192" "$img" >/dev/null 2>&1

# Entries in a fresh 4K directory block: "." @0, ".." @12, "a" @24.
# rec_len is the little endian u16 at entry offset 4.
blk=$(debugfs -R "bmap /reclen0 0" "$img" 2>/dev/null)
printf '\000\000' |
	dd of="$img" bs=1 seek=$((blk * 4096 + 12 + 4)) conv=notrunc 2>/dev/null

# rec_len of "a" = 4068 (0x0fe4): next entry would start at offset 4092
blk=$(debugfs -R "bmap /overrun 0" "$img" 2>/dev/null)
printf '\344\017' |
	dd of="$img" bs=1 seek=$((blk * 4096 + 24 + 4)) conv=notrunc 2>/dev/null
