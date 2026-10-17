# MBR disk images with extended partitions for test_mbr_logical.c. sfdisk
# writes the partition tables (one extended boot record 2048 sectors before
# each logical partition), mke2fs the ext4 filesystems that are copied into
# some partitions, and common/parttool.py modifies or damages the tables.
export LC_ALL=C
dir="$1.d"
mkdir -p "$dir"

pt()
{
	python3 "$(dirname "$0")/common/parttool.py" "$@"
}

# fs <name> <text> <size> [mke2fs options]: ext4 image with hello.txt
fs()
{
	mkdir -p "$dir/$1"
	printf '%s\n' "$2" > "$dir/$1/hello.txt"
	lwext4_mke2fs -t ext4 $4 -d "$dir/$1" "$dir/$1.img" "$3"
}

# put <image> <lba> <file> [sector size]
put()
{
	dd if="$3" of="$1" bs="${4:-512}" seek="$2" conv=notrunc 2>/dev/null
}

# variant <image> <suffix> <parttool arguments>: patched copy of <image>
variant()
{
	cp "$1" "$1.$2"
	img=$1.$2
	shift 2
	pt "$img" "$@"
}

# Main image: 20 MiB (40960 sectors). Primary 1 (ext4, active), extended
# primary 2 (sectors 6144-34815) with logical partitions 5 (ext4, EBR at
# 6144), 6 (swap, EBR at 12288) and 7 (ext4, EBR at 16384), primary 3
# (FAT32), primary 4 unused.
fs p1 "mbr p1" 2M
fs p5 "mbr p5" 2M
fs p7 "mbr p7" 2M
rm -f "$1"
truncate -s 20M "$1"
sfdisk -q "$1" <<EOF
label: dos
label-id: 0x12345678
start=2048, size=4096, type=83, bootable
start=6144, size=28672, type=5
start=34816, size=4096, type=c
start=8192, size=4096, type=83
start=14336, size=2048, type=82
start=18432, size=4096, type=83
EOF
put "$1" 2048 "$dir/p1.img"
put "$1" 8192 "$dir/p5.img"
put "$1" 18432 "$dir/p7.img"

# First EBR without a data partition (its logical partition deleted)
variant "$1" empty ebr 0 0 type=0 start=0 size=0
# Link and data records swapped within the first EBR
variant "$1" swapped ebr 0 0 type=5 start=6144 size=4096
pt "$1.swapped" ebr 0 1 type=0x83 start=2048 size=4096
# Last EBR links back to the first one / second EBR links to itself
variant "$1" loop ebr 2 1 type=5 start=0 size=4096
variant "$1" self-loop ebr 1 1 start=6144
# Next EBR at the end of / beyond the extended partition
variant "$1" oob ebr 1 1 start=28672
variant "$1" oob-far ebr 1 1 start=0x7fffffff
# Extended partition beyond the end of the device
variant "$1" ext-oob mbr 1 size=34817
# Second EBR without boot signature
variant "$1" nosig ebr 1 0 signature=0
# Logical partition 6 beyond its extended partition / on its own EBR
variant "$1" logical-oob ebr 1 0 size=30000
variant "$1" logical-at-ebr ebr 1 0 start=0
# Primary partition 3 beyond the end of the device / starting at LBA 0
variant "$1" primary-oob mbr 2 size=8000
variant "$1" primary-zero mbr 2 start=0

# Plain MBR: four primary partitions, ext4 in 1 (active) and 3, 4 empty.
fs p3 "mbr p3" 2M
rm -f "$1.plain"
truncate -s 12M "$1.plain"
sfdisk -q "$1.plain" <<EOF
label: dos
start=2048, size=4096, type=83, bootable
start=6144, size=2048, type=c
start=8192, size=4096, type=83
EOF
put "$1.plain" 2048 "$dir/p1.img"
put "$1.plain" 8192 "$dir/p3.img"

# 4096 byte sectors: ext4 (4 KiB blocks) in primary 1 and logical 5.
fs k1 "mbr 4k p1" 4M "-b 4096"
fs k5 "mbr 4k p5" 2M "-b 4096"
rm -f "$1.4k"
truncate -s 16M "$1.4k"
sfdisk -q --sector-size 4096 "$1.4k" <<EOF
label: dos
start=256, size=1024, type=83
start=1280, size=2048, type=5
start=1536, size=512, type=83
EOF
put "$1.4k" 256 "$dir/k1.img" 4096
put "$1.4k" 1536 "$dir/k5.img" 4096
