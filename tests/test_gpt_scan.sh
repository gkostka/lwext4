# GPT disk images for test_gpt_scan.c. sfdisk writes the partition tables,
# mke2fs the ext4 filesystems that are copied into some partitions, and
# common/parttool.py modifies or damages the tables (recomputing CRCs with
# Python's zlib where the result should stay consistent).
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

# Main image: 12 MiB, six partitions, ext4 in partitions 1, 3 and 6.
fs root "gpt root" 2M
fs data "gpt data" 2M
fs six "gpt six" 2M
rm -f "$1"
truncate -s 12M "$1"
sfdisk -q "$1" <<EOF
label: gpt
first-lba: 2048
start=2048, size=4096, type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, uuid=11111111-2222-3333-4444-555555555555, name="root"
start=6144, size=2048, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B
start=8192, size=4096, type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name="data"
start=12288, size=2048, type=0657FD6D-A4AB-43C4-84E5-0933C84B4F4F
start=14336, size=2048, type=8DA63339-0007-60C0-C436-083AC8230908, name="123456789012345678901234567890123456"
start=16384, size=4096, type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name="six", attrs="RequiredPartition LegacyBIOSBootable"
EOF
put "$1" 2048 "$dir/root.img"
put "$1" 8192 "$dir/data.img"
put "$1" 16384 "$dir/six.img"
# Non-ASCII names as raw UTF-16LE: "Grüße €"; U+10348 (a surrogate pair),
# an unpaired high surrogate, "x", an unpaired low surrogate.
pt "$1" gpt-entry both 1 name_hex=47007200fc00df0065002000ac20
pt "$1" gpt-entry both 3 name_hex=00d848df00d8780000dc

# 24576 sectors: backup header in LBA 24575, backup entries from LBA 24543.
variant "$1" bad-hdr flip 568
variant "$1" bad-arr flip 1084
variant "$1" bad-arrs flip 1084 $((24543 * 512 + 60))
variant "$1" bad-hdrs flip 568 $((24575 * 512 + 56))
variant "$1" no-pmbr mbr 0 type=0 start=0 size=0
variant "$1" hybrid mbr 1 type=0x83 start=2048 size=4096
rm -f "$1.blank"
truncate -s 1M "$1.blank"

# Small image (2048 sectors, no filesystems) for the header checks.
s=$1.s
rm -f "$s"
truncate -s 1M "$s"
sfdisk -q "$s" <<EOF
label: gpt
first-lba: 34
start=40, size=8, type=L, name="one"
start=48, size=8, type=L, name="two"
EOF
variant "$s" rev gpt-header primary revision=0x00020000
variant "$s" rev-minor gpt-header primary revision=0x00010001
variant "$s" hsize-small gpt-header primary header_size=91
variant "$s" hsize-big gpt-header primary header_size=513
variant "$s" hsize-sector gpt-header primary header_size=512
variant "$s" mylba gpt-header primary my_lba=2
variant "$s" esize-small gpt-header primary entry_size=64
variant "$s" esize-odd gpt-header primary entry_size=192
variant "$s" nentries gpt-header primary num_entries=0x10000000
variant "$s" elba-mbr gpt-header primary entries_lba=0
variant "$s" elba-hdr gpt-header primary entries_lba=1
variant "$s" elba-usable gpt-header primary entries_lba=34
variant "$s" elba-disk gpt-header primary entries_lba=2048
variant "$s" elba-end gpt-header primary entries_lba=2020
variant "$s" usable-start gpt-header primary first_usable_lba=1
variant "$s" usable-order gpt-header primary first_usable_lba=100 \
	last_usable_lba=99
variant "$s" usable-end gpt-header primary last_usable_lba=2047
variant "$s" arrcrc gpt-header primary entries_crc32=0x12345678
variant "$s" arrcrc-both gpt-header primary entries_crc32=0x12345678
pt "$s.arrcrc-both" gpt-header backup entries_crc32=0x12345678
variant "$s" backup-bad gpt-header backup revision=0
variant "$s" entry-end gpt-entry both 1 last_lba=2015
variant "$s" entry-start gpt-entry both 1 first_lba=33
variant "$s" entry-order gpt-entry both 1 first_lba=50 last_lba=49
variant "$s" esize256 gpt-entry-size 256
variant "$s" esize1024 gpt-entry-size 1024

# More partitions than CONFIG_EXT4_PARTITIONS_COUNT (16).
rm -f "$1.many"
truncate -s 1M "$1.many"
{
	echo "label: gpt"
	echo "first-lba: 34"
	i=0
	while [ $i -lt 20 ]; do
		echo "start=$((40 + 8 * i)), size=8, type=L"
		i=$((i + 1))
	done
} | sfdisk -q "$1.many"

# 4096 byte sectors: ext4 (4 KiB blocks) in the only partition.
fs big "gpt 4k" 4M "-b 4096"
rm -f "$1.4k"
truncate -s 16M "$1.4k"
sfdisk -q --sector-size 4096 "$1.4k" <<EOF
label: gpt
start=256, size=1024, type=L, name="big"
EOF
put "$1.4k" 256 "$dir/big.img" 4096
