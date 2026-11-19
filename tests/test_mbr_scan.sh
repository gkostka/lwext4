# MBR disk images for test_mbr_scan.c, partition tables written by sfdisk.
export LC_ALL=C
dir="$1.d"
mkdir -p "$dir"
for p in 1 3; do
	mkdir -p "$dir/p$p"
	printf 'mbr p%s\n' $p > "$dir/p$p/hello.txt"
	lwext4_mke2fs -t ext4 -d "$dir/p$p" "$dir/p$p.img" 2M
done

# Four primary partitions: ext4 in 1 and 3, FAT32 in 2, 4 empty.
rm -f "$1"
truncate -s 12M "$1"
sfdisk -q "$1" <<EOF
label: dos
start=2048, size=4096, type=83
start=6144, size=2048, type=c
start=8192, size=4096, type=83
EOF
dd if="$dir/p1.img" of="$1" bs=512 seek=2048 conv=notrunc 2>/dev/null
dd if="$dir/p3.img" of="$1" bs=512 seek=8192 conv=notrunc 2>/dev/null

# Primary 2 is an extended partition with two logical Linux partitions.
rm -f "$1.ext"
truncate -s 12M "$1.ext"
sfdisk -q "$1.ext" <<EOF
label: dos
start=2048, size=4096, type=83
start=6144, size=16384, type=5
start=8192, size=4096, type=83
start=14336, size=4096, type=83
EOF

rm -f "$1.blank"
truncate -s 1M "$1.blank"
