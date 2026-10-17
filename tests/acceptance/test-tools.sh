#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# The fs_test tools README.md documents (lwext4-generic, lwext4-mkfs) and the
# ones it ships and installs (lwext4-mbr, lwext4-server, lwext4-client):
# every option they accept is exercised, and what they write to an image is
# checked with e2fsck and debugfs.
set -eu
. "$(dirname "$0")/lib.sh"

# expect_fail <what> <command...>: the command must exit with non-zero
expect_fail()
{
	what=$1
	shift
	if "$@" >"$WORK/out.txt" 2>&1; then
		cat "$WORK/out.txt"
		die "$what: expected a failure"
	fi
	pass "$what"
}

# usage_check <tool> <short/long option...>: --help and -h exit 0 and
# document every option the tool accepts.
usage_check()
{
	tool=$1
	shift
	name=$(basename "$tool")
	for h in --help -h; do
		lw "$tool" $h >"$WORK/help.txt" 2>&1 ||
			{ cat "$WORK/help.txt"; die "$name $h exits with an error"; }
		grep -q 'Usage' "$WORK/help.txt" || die "$name $h prints no usage"
	done
	pass "$name --help / -h print the usage and succeed"
	for opt in "$@"; do
		grep -Eq -- "(^|[^a-z_-])$opt([^a-z_]|$)" "$WORK/help.txt" ||
			{ cat "$WORK/help.txt"; die "$name --help does not document $opt"; }
	done
	pass "$name --help documents $*"
	lw "$tool" --version >"$WORK/version.txt"
	lw "$tool" -x | cmp -s - "$WORK/version.txt" ||
		die "$name -x and --version differ"
	[ -s "$WORK/version.txt" ] || die "$name --version prints nothing"
	pass "$name --version / -x print the version ($(cat "$WORK/version.txt"))"
	expect_fail "$name rejects an unknown option" lw "$tool" --no-such-option
}

# ---------------------------------------------------------------------------
step "lwext4-generic"
# ---------------------------------------------------------------------------
usage_check "$GENERIC" -i --input -s --rw_size -c --rw_count -d --dirs \
	-l --clean -b --bstat -t --sbstat -w --wpart -v --verbose -x --version \
	-h --help

img="$WORK/generic.img"
lwext4_mke2fs -t ext4 -b 1024 "$img" 64M

# Default rw size/count (1 MiB x 10), no directory test
lw "$GENERIC" -i "$img" >"$WORK/generic1.txt"
grep -q '^test finished' "$WORK/generic1.txt" || die "no 'test finished'"
pass "lwext4-generic -i <image> runs"
fsck_clean "$img" "after lwext4-generic"
# test1 holds rw_count chunks of rw_size bytes, chunk i filled with '0'+i%10
: >"$WORK/expect.bin"
for i in 0 1 2 3 4 5 6 7 8 9; do
	head -c 1048576 /dev/zero | tr '\0' "$i" >>"$WORK/expect.bin"
done
dbg "$img" "dump /test1 $WORK/test1.bin" >/dev/null
cmp "$WORK/test1.bin" "$WORK/expect.bin" || die "/test1 content differs"
pass "debugfs reads back /test1 (10 x 1 MiB written by lwext4-generic)"
[ "$(dbg "$img" "cat /hello.txt")" = "Hello World !" ] ||
	die "/hello.txt differs"
pass "debugfs reads back /hello.txt"

# -s/--rw_size, -c/--rw_count, -d/--dirs (long and short forms)
lw "$GENERIC" --input "$img" --rw_size 4096 --rw_count 3 --dirs 50 >/dev/null
[ "$(file_size "$img" /test1)" = 12288 ] ||
	die "--rw_size 4096 --rw_count 3 did not write 12288 bytes"
pass "--rw_size/--rw_count control the file test (12288 bytes)"
n=$(dir_names "$img" /dir1 | grep -c '^f[0-9]*$')
[ "$n" -eq 50 ] || die "--dirs 50 created $n files"
pass "--dirs 50 creates 50 entries in /dir1"
lw "$GENERIC" -i "$img" -s 1000 -c 7 -d 20 >/dev/null
[ "$(file_size "$img" /test1)" = 7000 ] ||
	die "-s 1000 -c 7 did not write 7000 bytes"
n=$(dir_names "$img" /dir1 | grep -c '^f[0-9]*$')
[ "$n" -eq 20 ] || die "-d 20 left $n files (previous run not cleaned up?)"
pass "-s/-c/-d short options work, each run starts from a clean slate"
fsck_clean "$img" "after lwext4-generic with options"

# -t/--sbstat: statistics match the superblock
lw "$GENERIC" -i "$img" -c 1 --sbstat >"$WORK/sbstat.txt"
sb_inodes=$(dumpe2fs -h "$img" 2>/dev/null | sed -n 's/^Inode count: *//p')
sb_bsize=$(dumpe2fs -h "$img" 2>/dev/null | sed -n 's/^Block size: *//p')
grep -q "^inodes_count = $sb_inodes\$" "$WORK/sbstat.txt" ||
	die "--sbstat inodes_count does not match dumpe2fs ($sb_inodes)"
grep -q "^block_size = $sb_bsize\$" "$WORK/sbstat.txt" ||
	die "--sbstat block_size does not match dumpe2fs ($sb_bsize)"
pass "--sbstat prints the superblock statistics"

# -b/--bstat: block cache statistics; README: "block cache should not
# allocate more than CONFIG_BLOCK_DEV_CACHE_SIZE" (16 for the generic build)
lw "$GENERIC" -i "$img" -c 2 -d 100 --bstat >"$WORK/bstat.txt"
max=$(sed -n 's/^bcache->max_ref_blocks = //p' "$WORK/bstat.txt")
[ -n "$max" ] || die "--bstat prints no block cache statistics"
[ "$max" -le 16 ] || die "block cache held $max blocks (limit 16)"
pass "--bstat: block cache peaked at $max of 16 blocks"

# -l/--clean: everything the test created is removed afterwards
lw "$GENERIC" -i "$img" -c 2 -d 10 --clean >/dev/null
for f in /test1 /hello.txt /dir1; do
	if debugfs -R "stat $f" "$img" 2>&1 | grep -q '^Inode:'; then
		die "--clean left $f behind"
	fi
done
pass "--clean removes /test1, /hello.txt and /dir1"
fsck_clean "$img" "after lwext4-generic --clean"
lw "$GENERIC" -i "$img" -c 1 -l >/dev/null
pass "-l is accepted"

# -v/--verbose: debug output
lw "$GENERIC" -i "$img" -c 1 >"$WORK/quiet.txt"
lw "$GENERIC" -i "$img" -c 1 --verbose >"$WORK/verbose.txt"
[ "$(wc -l <"$WORK/verbose.txt")" -gt "$(wc -l <"$WORK/quiet.txt")" ] ||
	die "--verbose prints no additional output"
pass "--verbose adds debug output"

# -w/--wpart is Windows only
expect_fail "--wpart is refused outside Windows" lw "$GENERIC" -i "$img" --wpart
grep -q 'only under windows' "$WORK/out.txt" || die "--wpart: no explanation"

expect_fail "lwext4-generic fails on a missing image" \
	lw "$GENERIC" -i "$WORK/does-not-exist.img"

# ---------------------------------------------------------------------------
step "lwext4-mkfs"
# ---------------------------------------------------------------------------
usage_check "$MKFS" -i --input -b --block -e --ext -w --wpart -v --verbose \
	-x --version -h --help

# -e selects ext2/3/4, -b the block size; e2fsck and dumpe2fs check the
# result, lwext4-generic then uses it and e2fsck checks again.
for bs in 1024 2048 4096; do
	for e in 2 3 4; do
		img="$WORK/mkfs-ext$e-$bs.img"
		# 40 MiB: not a multiple of the group size, the last group is
		# partial for every block size
		truncate -s 40M "$img"
		lw "$MKFS" -i "$img" -e $e -b $bs >"$WORK/mkfs.txt"
		fsck_clean "$img" "lwext4-mkfs -e $e -b $bs"
		f=$(features "$img")
		[ "$(dumpe2fs -h "$img" 2>/dev/null | sed -n 's/^Block size: *//p')" = $bs ] ||
			die "-b $bs: wrong block size"
		case $e in
		2) ! has_feature "$img" has_journal && ! has_feature "$img" extent ;;
		3) has_feature "$img" has_journal && ! has_feature "$img" extent ;;
		4) has_feature "$img" has_journal && has_feature "$img" extent ;;
		esac || die "-e $e: unexpected features: $f"
		pass "lwext4-mkfs -e $e -b $bs: ext$e ($f)"
		lw "$GENERIC" -i "$img" -s 65536 -c 8 -d 30 >/dev/null
		fsck_clean "$img" "lwext4-generic on lwext4-mkfs -e $e -b $bs"
	done
done

# Long options, default block size (1024) and fs type (ext4)
img="$WORK/mkfs-long.img"
truncate -s 32M "$img"
lw "$MKFS" --input "$img" >/dev/null
[ "$(dumpe2fs -h "$img" 2>/dev/null | sed -n 's/^Block size: *//p')" = 1024 ] ||
	die "default block size is not 1024"
has_feature "$img" extent || die "default fs type is not ext4"
pass "lwext4-mkfs defaults: ext4, 1024 byte blocks"
lw "$MKFS" --input "$img" --ext 3 --block 2048 >/dev/null
has_feature "$img" has_journal && ! has_feature "$img" extent ||
	die "--ext 3 did not create ext3"
fsck_clean "$img" "lwext4-mkfs --ext 3 --block 2048"
lw "$MKFS" -i "$img" -e 2 --verbose >"$WORK/mkfs-verbose.txt"
lw "$MKFS" -i "$img" -e 2 >"$WORK/mkfs-quiet.txt"
[ "$(wc -l <"$WORK/mkfs-verbose.txt")" -gt "$(wc -l <"$WORK/mkfs-quiet.txt")" ] ||
	die "--verbose prints no additional output"
pass "lwext4-mkfs --verbose adds debug output"

expect_fail "lwext4-mkfs rejects -b 512" lw "$MKFS" -i "$img" -b 512
expect_fail "lwext4-mkfs rejects -b 8192" lw "$MKFS" -i "$img" -b 8192
expect_fail "lwext4-mkfs rejects -e 5" lw "$MKFS" -i "$img" -e 5
expect_fail "lwext4-mkfs --wpart is refused outside Windows" \
	lw "$MKFS" -i "$img" --wpart
expect_fail "lwext4-mkfs fails without an image" lw "$MKFS" -e 4

# ---------------------------------------------------------------------------
step "lwext4-mbr"
# ---------------------------------------------------------------------------
usage_check "$MBR" -i --input -w --wpart -v --verbose -x --version -h --help

img="$WORK/mbr.img"
truncate -s 64M "$img"
# 1 MiB aligned partitions of 8, 16 and 20 MiB, fourth entry empty
sfdisk -q "$img" <<EOF
label: dos
start=2048, size=16384, type=83
start=18432, size=32768, type=83
start=51200, size=40960, type=83
EOF
lw "$MBR" -i "$img" >"$WORK/mbr.txt"
cat >"$WORK/mbr-expect.txt" <<EOF
mbr_entry 0:
	offeset: 0x100000, 1MB
	size:    0x800000, 8MB
mbr_entry 1:
	offeset: 0x900000, 9MB
	size:    0x1000000, 16MB
mbr_entry 2:
	offeset: 0x1900000, 25MB
	size:    0x1400000, 20MB
mbr_entry 3:
	empty/unknown
EOF
sed -n '/^mbr_entry/,$p' "$WORK/mbr.txt" | diff -u "$WORK/mbr-expect.txt" - ||
	die "lwext4-mbr output differs from the sfdisk partition table"
pass "lwext4-mbr reports the partitions sfdisk created"
lw "$MBR" --input "$img" --verbose >/dev/null
pass "lwext4-mbr --input/--verbose"
truncate -s 1M "$WORK/nombr.img"
expect_fail "lwext4-mbr fails on an image without a partition table" \
	lw "$MBR" -i "$WORK/nombr.img"

# ---------------------------------------------------------------------------
step "lwext4-server / lwext4-client"
# ---------------------------------------------------------------------------
usage_check "$SERVER" -i --image -p --port -v --verbose -w --winpart \
	-c --cache_wb -x --version -h --help
usage_check "$CLIENT" -c --call -p --port -a --addr -x --version -h --help

SERVER_PID=
stop_server()
{
	if [ -n "$SERVER_PID" ]; then
		kill "$SERVER_PID" 2>/dev/null || :
		wait "$SERVER_PID" 2>/dev/null || :
		SERVER_PID=
	fi
}
trap stop_server EXIT

# server_session <port> <server options...>
server_session()
{
	port=$1
	shift
	img="$WORK/server-$port.img"
	lwext4_mke2fs -t ext4 -b 1024 "$img" 32M
	lw "$SERVER" -i "$img" -p "$port" "$@" >"$WORK/server-$port.log" 2>&1 &
	SERVER_PID=$!
	# Wait until the server answers (a call it rejects: nothing changes)
	i=0
	until lw "$CLIENT" -p "$port" -c "dir_close 63" 2>&1 | grep -q '^rc:'; do
		i=$((i + 1))
		[ $i -lt 50 ] || die "lwext4-server did not start"
		sleep 0.1
	done
	call()
	{
		lw "$CLIENT" -a 127.0.0.1 --port "$port" -c "$*" >"$WORK/client.txt" ||
			{ cat "$WORK/client.txt"; die "lwext4-client -c '$*' failed"; }
	}
	call "device_register 0 0 bdev"
	call "mount bdev /"
	call "mount_point_stats / 0"
	call "cache_write_back / 1"
	call "dir_mk /test"
	call "fopen 0 /test/file wb+"
	call "fwrite 0 0 3000000 0"
	call "ftell 0 3000000"
	call "fsize 0 3000000"
	call "fseek 0 0 0"
	call "fread 0 0 3000000 0"
	call "fclose 0"
	call "multi_fcreate /test /f 20"
	call "multi_fwrite /test /f 20 5000"
	call "multi_fread /test /f 20 5000"
	call "multi_dcreate /test /d 20"
	call "dir_open 0 /test"
	call "dir_entry_get 0 41"
	call "dir_close 0"
	call "multi_dremove /test /d 20"
	call "multi_fremove /test /f 20"
	call "cache_write_back / 0"
	# Failures are reported through the client's exit status
	if lw "$CLIENT" -p "$port" -c "fopen 1 /test/missing rb" >/dev/null; then
		die "lwext4-client reports success for a failed call"
	fi
	if lw "$CLIENT" -p "$port" -c "dir_entry_get 0 1" >/dev/null; then
		die "lwext4-client reports success for a failed check"
	fi
	pass "lwext4-client exit status reports failed calls"
	call "umount /"
	stop_server
	pass "lwext4-server $* / lwext4-client session on port $port"
	fsck_clean "$img" "after the lwext4-server session"
	# The server writes 'x' (RW_BUFFER_PATERN)
	head -c 3000000 /dev/zero | tr '\0' x >"$WORK/expect.bin"
	dbg "$img" "dump /test/file $WORK/file.bin" >/dev/null
	cmp "$WORK/file.bin" "$WORK/expect.bin" || die "/test/file differs"
	pass "debugfs reads back the file written through lwext4-server"
	[ "$(dir_names "$img" /test | wc -l)" -eq 1 ] ||
		die "multi_fremove/multi_dremove left entries behind"
	pass "multi_fremove/multi_dremove removed everything"
}

server_session 12341
server_session 12342 --cache_wb
server_session 12343 -c -v

finish
