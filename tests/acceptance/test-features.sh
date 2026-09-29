#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# README "About", "Features" and "Supported ext2/3/4 features" claims.
#
# Every filesystem is created by mke2fs, written by lwext4 through the public
# API (lwext4-acceptance), and then checked by e2fsprogs: e2fsck -fn must
# find nothing to fix and debugfs must read back exactly what lwext4 wrote.
# The other direction (content written by e2fsprogs, read by lwext4) and the
# behaviour on the features the README lists as unsupported are covered too.
set -eu
. "$(dirname "$0")/lib.sh"

LONG_TARGET=/this/symlink/target/is/longer/than/sixty/characters/so/it/does/not/fit/into/the/inode
NMANY=400

# ---------------------------------------------------------------------------
# lwext4 writes, e2fsprogs checks.
# ---------------------------------------------------------------------------

write_script()
{
	# ext4_recover() reports ENOTSUP without a journal, as documented by
	# the fs_test tools which accept exactly that.
	if has_feature "$img" has_journal; then
		recover=recover
	else
		recover="fail ENOTSUP recover"
	fi
	cat <<EOF
mount
$recover
journal_start
mkdir /d
mkdir /d/sub
write /d/small 13 1
write /d/big 3000000 2 7777
append /d/big 5000 2
write /d/trunc 1000000 3
truncate /d/trunc 1000
write /d/gone 5000 4
remove /d/gone
write /d/old 100 5
rename /d/old /d/new
mkdir /mvdir
write /mvdir/f 10 6
dir_mv /mvdir /d/sub/moved
mkdir /rmdir
mkdir /rmdir/a
write /rmdir/a/f 5000 7
dir_rm /rmdir
symlink /d/small /fastlink
symlink $LONG_TARGET /slowlink
link /d/small /hardlink
mknod /chr chr 0x0501
mknod /blk blk 0x0802
mknod /fifo fifo 0
mknod /sock sock 0
chmod /d/small 640
chown /d/small 100000 70000
atime /d/small 1111111111
mtime /d/small 1234567890
ctime /d/small 1300000000
setxattr /d/small user.test hello
setxattr /d/small user.other world
removexattr /d/small user.other
mkdir /many
EOF
	# Enough entries for several directory blocks even with 64 KiB blocks
	# (dir_index: the directory becomes an htree).
	i=0
	while [ $i -lt $NMANY ]; do
		echo "write /many/a_rather_long_file_name_to_fill_blocks_$i 1 $i"
		i=$((i + 1))
	done
	cat <<EOF
cache_check
journal_stop
umount
EOF
}

verify_script()
{
	cat <<EOF
mount ro
verify /d/small 13 1
verify /d/big 3005000 2
verify /d/trunc 1000 3
verify /d/new 100 5
verify /d/sub/moved/f 10 6
missing /d/gone
missing /d/old
missing /mvdir
missing /rmdir
ls / blk,chr,d,fastlink,fifo,hardlink,lost+found,many,slowlink,sock
ls /d big,new,small,sub,trunc
exists /d dir
exists /d/small file
exists /fastlink symlink
exists /slowlink symlink
exists /chr chr
exists /blk blk
exists /fifo fifo
exists /sock sock
readlink /fastlink /d/small
readlink /slowlink $LONG_TARGET
verify /hardlink 13 1
mode /d/small 640
owner /d/small 100000 70000
get_atime /d/small 1111111111
get_mtime /d/small 1234567890
get_ctime /d/small 1300000000
getxattr /d/small user.test hello
listxattr /d/small user.test
count /many $NMANY
verify /many/a_rather_long_file_name_to_fill_blocks_$((NMANY - 1)) 1 $((NMANY - 1))
fail EROFS mkdir /x
fail EROFS write /d/x 1 1
cache_check
umount
EOF
}

# debugfs <request> output of an inode's stat
stat_of()
{
	dbg "$img" "stat $1"
}

# rw_case <name> <size> <mke2fs options...>
rw_case()
{
	name=$1
	size=$2
	shift 2
	img="$WORK/$name.img"
	step "lwext4 writes, e2fsprogs checks: $name (mke2fs $*)"
	lwext4_mke2fs "$@" "$img" "$size"
	log "features: $(features "$img")"

	write_script >"$WORK/write.txt"
	acc "$img" <"$WORK/write.txt"
	pass "$name: lwext4 wrote files, dirs, links, nodes, xattrs"
	fsck_clean "$img" "$name after lwext4 writes"

	# Independent read back with debugfs.
	dump_cmp "$img" /d/big 3005000 2
	dump_cmp "$img" /d/small 13 1
	dump_cmp "$img" /d/trunc 1000 3
	dump_cmp "$img" /d/sub/moved/f 10 6
	dump_cmp "$img" /many/a_rather_long_file_name_to_fill_blocks_7 1 7

	ls_root=$(dir_names "$img" / | sort | tr '\n' ' ')
	[ "$ls_root" = "blk chr d fastlink fifo hardlink lost+found many slowlink sock " ] ||
		die "debugfs ls /: '$ls_root'"
	pass "debugfs lists the lwext4 created root entries"
	n=$(dir_names "$img" /many | wc -l)
	[ "$n" -eq "$NMANY" ] || die "debugfs sees $n entries in /many"
	pass "debugfs sees all $NMANY entries in /many"

	s=$(stat_of /d/small)
	expect_match "$s" 'Type: regular +Mode: +0640' "chmod visible to debugfs"
	expect_match "$s" 'User: +100000 +Group: +70000 ' "chown visible to debugfs"
	expect_match "$s" 'Links: 2 ' "hardlink count visible to debugfs"
	expect_match "$s" 'atime: 0x423a35c7' "atime visible to debugfs"
	expect_match "$s" 'mtime: 0x499602d2' "mtime visible to debugfs"
	expect_match "$s" 'ctime: 0x4d7c6d00' "ctime visible to debugfs"
	ino_small=$(printf '%s\n' "$s" | sed -n 's/^Inode: \([0-9]*\).*/\1/p')
	ino_hard=$(stat_of /hardlink | sed -n 's/^Inode: \([0-9]*\).*/\1/p')
	[ -n "$ino_small" ] && [ "$ino_small" = "$ino_hard" ] ||
		die "hardlink inode $ino_hard != $ino_small"
	pass "hardlink shares the inode ($ino_small)"
	expect_match "$(dbg "$img" "ea_get /d/small user.test")" \
		'^(user\.test \(5\) = )?"?hello"?$' "xattr visible to debugfs"
	expect_match "$(dbg "$img" "ea_list /d/small")" 'user\.test' \
		"xattr list visible to debugfs"
	if dbg "$img" "ea_list /d/small" | grep -q user.other; then
		die "removed xattr still visible"
	fi
	pass "removed xattr is gone"
	expect_match "$(stat_of /fastlink)" 'Fast link dest: "/d/small"' \
		"fast symlink visible to debugfs"
	[ "$(dbg "$img" "cat /slowlink")" = "$LONG_TARGET" ] ||
		die "slow symlink target differs"
	pass "slow symlink visible to debugfs"
	expect_match "$(stat_of /chr)" 'Type: character special' "chr node type"
	expect_match "$(stat_of /chr)" 'Device major/minor number: 05:01' \
		"chr node device number"
	expect_match "$(stat_of /blk)" 'Type: block special' "blk node type"
	expect_match "$(stat_of /blk)" 'Device major/minor number: 08:02' \
		"blk node device number"
	expect_match "$(stat_of /fifo)" 'Type: FIFO' "fifo node type"
	expect_match "$(stat_of /sock)" 'Type: socket' "socket node type"

	if has_feature "$img" extent; then
		expect_match "$(stat_of /d/big)" '^EXTENTS:' \
			"new files use extents (extents feature)"
	else
		expect_match "$(stat_of /d/big)" '^BLOCKS:' \
			"new files use block maps (no extents feature)"
	fi
	if has_feature "$img" dir_index; then
		flags=$(stat_of /many | sed -n 's/.*Flags: \(0x[0-9a-f]*\).*/\1/p')
		[ $((flags & 0x1000)) -ne 0 ] ||
			die "/many has no htree index flag (flags $flags)"
		pass "large directory has the htree index flag (dir_index)"
		# debugfs 1.47 prints a bogus checksum complaint for htrees
		# without metadata_csum (also for ones e2fsck -D built), so
		# only its exit status and the dump itself are checked here;
		# e2fsck -fn above validated the tree.
		debugfs -R "htree_dump /many" "$img" >"$WORK/htree.txt" 2>/dev/null
		expect_match "$(cat "$WORK/htree.txt")" 'Number of entries' \
			"debugfs walks the lwext4 built htree"
	fi

	# lwext4 reads everything back from a fresh mount, read-only.
	verify_script >"$WORK/verify.txt"
	cksum_before=$(cksum <"$img")
	acc "$img" <"$WORK/verify.txt"
	[ "$(cksum <"$img")" = "$cksum_before" ] ||
		die "read-only mount modified the image"
	pass "$name: lwext4 reads everything back, read-only mount writes nothing"
}

rw_case ext2-1k 64M -t ext2 -b 1024
rw_case ext2-4k-128inode 64M -t ext2 -b 4096 -I 128
rw_case ext3-1k 64M -t ext3 -b 1024
rw_case ext3-2k 64M -t ext3 -b 2048
rw_case ext4-1k 64M -t ext4 -b 1024
rw_case ext4-2k 64M -t ext4 -b 2048
rw_case ext4-4k 64M -t ext4 -b 4096
# "multiple blocksize supported: 1KB, 2KB, 4KB ... 64KB"
rw_case ext4-8k 256M -t ext4 -b 8192
rw_case ext4-16k 256M -t ext4 -b 16384
rw_case ext4-32k 512M -t ext4 -b 32768
rw_case ext4-64k 1G -t ext4 -b 65536
# Supported feature variations
rw_case ext4-no-metadata_csum-gdt_csum 64M -t ext4 -O ^metadata_csum,uninit_bg
rw_case ext4-no-64bit 64M -t ext4 -O ^64bit
rw_case ext4-meta_bg 64M -t ext4 -O ^resize_inode,meta_bg
rw_case ext4-no-flex_bg 64M -t ext4 -O ^flex_bg
rw_case ext4-no-journal 64M -t ext4 -O ^has_journal
rw_case ext4-no-dir_index 64M -t ext4 -O ^dir_index
# mmp is listed as unsupported; lwext4 ignores it and must not break it.
rw_case ext4-mmp 64M -t ext4 -O mmp

# ---------------------------------------------------------------------------
# e2fsprogs writes, lwext4 reads (and then modifies).
# ---------------------------------------------------------------------------

NBIG=12000
BIGOFF=5368709120	# 5 GiB: file sizes and offsets beyond 32 bits

# host_case <name> <mke2fs options...>
host_case()
{
	name=$1
	shift
	img="$WORK/host-$name.img"
	src="$WORK/src"
	step "e2fsprogs writes, lwext4 reads: $name (mke2fs $*)"
	rm -rf "$src"
	mkdir -p "$src/dir/sub" "$src/big"
	pattern 0 1 >"$src/empty"
	pattern 1 2 >"$src/one"
	pattern 4095 3 >"$src/dir/f4095"
	pattern 4096 4 >"$src/dir/f4096"
	pattern 1048593 5 >"$src/dir/sub/f1m"
	i=0
	while [ $i -lt $NBIG ]; do
		: >"$src/big/entry_$i"
		i=$((i + 1))
	done
	ln -s dir/f4096 "$src/fastlink"
	ln -s "$LONG_TARGET" "$src/slowlink"
	ln "$src/dir/f4095" "$src/hardlink"
	lwext4_mke2fs "$@" -d "$src" "$img" 128M
	# e2fsck -D turns the big directory into an htree (dir_index)
	rc=0
	e2fsck -fyD "$img" >"$WORK/fsckD.log" 2>&1 || rc=$?
	[ $rc -le 1 ] || { cat "$WORK/fsckD.log"; die "e2fsck -fyD failed"; }
	# A sparse 5 GiB + 1 MiB file with data at the start and at 5 GiB
	pattern 65536 6 >"$WORK/sparse"
	pattern 1048576 7 "$BIGOFF" |
		dd of="$WORK/sparse" bs=65536 seek=$((BIGOFF / 65536)) conv=notrunc iflag=fullblock 2>/dev/null
	cat >"$WORK/host.cmds" <<EOF
write $WORK/sparse large
mknod fifo p
mknod chr c 5 1
mknod blk b 8 2
ea_set /dir/f4096 user.color blue
ea_set /dir/f4096 trusted.x 0123456789
EOF
	debugfs -w -f "$WORK/host.cmds" "$img" >"$WORK/debugfs.log" 2>&1
	grep -v '^debugfs\|^Allocated inode: [0-9]*$' "$WORK/debugfs.log" | grep . &&
		die "debugfs reported errors while populating the image"
	fsck_clean "$img" "$name as written by e2fsprogs"
	if has_feature "$img" dir_index; then
		flags=$(dbg "$img" "stat /big" | sed -n 's/.*Flags: \(0x[0-9a-f]*\).*/\1/p')
		[ $((flags & 0x1000)) -ne 0 ] || die "e2fsck -D did not index /big"
	fi
	# mke2fs -d also copies the source files' ACLs, if the host has any;
	# the expected list of names comes from debugfs.
	xattrs=$(dbg "$img" "ea_list /dir/f4096" |
		sed -n 's/^  \([^ ]*\) (.*/\1/p' | LC_ALL=C sort | paste -sd, -)
	case ",$xattrs," in
	*,trusted.x,*user.color,*) ;;
	*) die "debugfs lists xattrs '$xattrs'" ;;
	esac

	cksum_before=$(cksum <"$img")
	acc "$img" <<EOF
mount ro
verify /empty 0 1
verify /one 1 2
verify /dir/f4095 4095 3
verify /dir/f4096 4096 4
verify /dir/sub/f1m 1048593 5
verify /hardlink 4095 3
count /big $NBIG
exists /big/entry_0 file
exists /big/entry_$((NBIG - 1)) file
exists /big/entry_1234 file
fail ENOENT exists /big/entry_$NBIG file
readlink /fastlink dir/f4096
readlink /slowlink $LONG_TARGET
exists /fastlink symlink
exists /slowlink symlink
exists /fifo fifo
exists /chr chr
exists /blk blk
getxattr /dir/f4096 user.color blue
getxattr /dir/f4096 trusted.x 0123456789
listxattr /dir/f4096 $xattrs
size /large $((BIGOFF + 1048576))
seek_verify /large 0 65536 6
seek_verify /large $BIGOFF 1048576 7
umount
EOF
	[ "$(cksum <"$img")" = "$cksum_before" ] ||
		die "read-only mount modified the image"
	pass "$name: lwext4 reads everything e2fsprogs wrote (incl. a 5 GiB sparse file)"

	# Modify what e2fsprogs created: the htree directory, the large file
	{
		echo mount
		if has_feature "$img" has_journal; then echo recover; else echo "fail ENOTSUP recover"; fi
		echo journal_start
		i=0
		while [ $i -lt 1000 ]; do
			echo "remove /big/entry_$((i * 3))"
			i=$((i + 1))
		done
		i=0
		while [ $i -lt 500 ]; do
			echo "write /big/new_entry_with_a_longer_name_$i 1 $i"
			i=$((i + 1))
		done
		echo "rename /big/entry_1 /big/renamed"
		echo "append /large 70000 7"
		echo "setxattr /dir/f4096 user.color green"
		echo "journal_stop"
		echo "umount"
		echo "mount ro"
		echo "count /big $((NBIG - 1000 + 500))"
		echo "seek_verify /large $BIGOFF $((1048576 + 70000)) 7"
		echo "umount"
	} >"$WORK/modify.txt"
	acc "$img" <"$WORK/modify.txt"
	pass "$name: lwext4 modified the htree directory and appended beyond 5 GiB"
	fsck_clean "$img" "$name after lwext4 modified it"
	n=$(dir_names "$img" /big | wc -l)
	[ "$n" -eq $((NBIG - 1000 + 500)) ] || die "debugfs sees $n entries in /big"
	pass "debugfs sees the modified directory ($n entries)"
	[ "$(file_size "$img" /large)" -eq $((BIGOFF + 1048576 + 70000)) ] ||
		die "debugfs: wrong size of /large"
	dump_range "$img" /large "$BIGOFF" $((1048576 + 70000)) >"$WORK/tail.bin"
	pattern $((1048576 + 70000)) 7 "$BIGOFF" | cmp - "$WORK/tail.bin" ||
		die "debugfs reads different data at the end of /large"
	pass "debugfs reads back the data lwext4 appended at 5 GiB"
	expect_match "$(dbg "$img" "ea_get /dir/f4096 user.color")" 'green' \
		"debugfs sees the xattr lwext4 changed"
}

host_case ext2-1k -t ext2 -b 1024
host_case ext3-4k -t ext3 -b 4096
host_case ext4-1k -t ext4 -b 1024
host_case ext4-4k -t ext4 -b 4096

# ---------------------------------------------------------------------------
# "directory indexing - fast directory find", "extents - fast big file
# truncate": measured in block device reads/writes, not in time.
# ---------------------------------------------------------------------------
step "directory indexing: lookups in a $NBIG entry directory"
for idx in dir_index ^dir_index; do
	img="$WORK/lookup-$idx.img"
	lwext4_mke2fs -t ext4 -b 1024 -O "$idx" "$img" 64M
	{
		echo mount
		echo mkdir /big
		i=0
		while [ $i -lt $NBIG ]; do
			echo "write /big/file_number_$i 1 1"
			i=$((i + 1))
		done
		echo umount
		# fresh mount, cold cache
		echo mount
		echo "io exists /big/file_number_$((NBIG - 1)) file"
		echo "io fail ENOENT exists /big/not_there file"
		echo umount
	} >"$WORK/lookup.txt"
	acc "$img" <"$WORK/lookup.txt" >"$WORK/lookup-$idx.log"
	fsck_clean "$img" "$NBIG entry directory ($idx)"
done
reads()
{
	sed -n "s/^io $2: reads \([0-9]*\) .*/\1/p" "$WORK/lookup-$1.log"
}
hit_idx=$(reads dir_index exists)
hit_lin=$(reads ^dir_index exists)
miss_idx=$(reads dir_index fail)
miss_lin=$(reads ^dir_index fail)
log "block reads, indexed/linear: hit $hit_idx/$hit_lin, miss $miss_idx/$miss_lin"
[ "$hit_idx" -le 20 ] && [ "$miss_idx" -le 20 ] ||
	die "indexed lookups need more than 20 block reads"
[ $((hit_idx * 10)) -le "$hit_lin" ] && [ $((miss_idx * 10)) -le "$miss_lin" ] ||
	die "indexed lookups are not 10x cheaper than linear ones"
pass "htree lookups: $hit_idx/$miss_idx block reads vs. $hit_lin/$miss_lin without dir_index"

step "extents: truncating a 64 MiB file"
for fs in ext4 ext3; do
	img="$WORK/trunc-$fs.img"
	lwext4_mke2fs -t $fs -b 1024 "$img" 128M
	acc "$img" >"$WORK/trunc-$fs.log" <<EOF
mount
write /f 67108864 1 1048576
umount
mount
io truncate /f 0
umount
EOF
	fsck_clean "$img" "after truncating 64 MiB ($fs)"
done
tio()
{
	sed -n 's/^io truncate: reads \([0-9]*\) writes \([0-9]*\)$/\1 \2/p' \
		"$WORK/trunc-$1.log" | awk '{ print $1 + $2 }'
}
io_ext=$(tio ext4)
io_ind=$(tio ext3)
[ $((io_ext * 10)) -le "$io_ind" ] ||
	die "extent truncate needs $io_ext block I/Os, block map truncate $io_ind"
pass "extent truncate: $io_ext block I/Os vs. $io_ind with block maps"

# ---------------------------------------------------------------------------
# dir_nlink: more than 65000 subdirectories
# ---------------------------------------------------------------------------
step "dir_nlink: 65010 subdirectories"
img="$WORK/nlink.img"
lwext4_mke2fs -t ext4 -b 1024 -N 70000 "$img" 256M
{
	echo mount
	echo mkdir /p
	echo cache_write_back 1
	i=0
	while [ $i -lt 65010 ]; do
		echo "mkdir /p/$i"
		i=$((i + 1))
	done
	echo cache_write_back 0
	echo dir_rm /p/17
	echo umount
} >"$WORK/nlink.txt"
acc "$img" <"$WORK/nlink.txt"
fsck_clean "$img" "65009 subdirectories"
has_feature "$img" dir_nlink || die "dir_nlink not set"
expect_match "$(dbg "$img" "stat /p")" 'Links: 1 ' \
	"directory with more than 65000 subdirectories has link count 1"

# ---------------------------------------------------------------------------
# Features README.md lists as unsupported
# ---------------------------------------------------------------------------

# unsupported_incompat <name> <mke2fs options...>: mount must be refused and
# the image left alone.
unsupported_incompat()
{
	name=$1
	shift
	img="$WORK/unsupported-$name.img"
	lwext4_mke2fs "$@" "$img" 64M
	cksum_before=$(cksum <"$img")
	acc "$img" <<EOF
fail ENOTSUP mount
EOF
	[ "$(cksum <"$img")" = "$cksum_before" ] || die "$name: image modified"
	pass "incompatible feature $name: mount refused (ENOTSUP), image untouched"
}

# unsupported_ro <name> <mke2fs options...>: mount only read-only, reading
# works, writing is refused, the image is left alone.
unsupported_ro()
{
	name=$1
	shift
	img="$WORK/unsupported-$name.img"
	rm -rf "$WORK/src"
	mkdir -p "$WORK/src"
	pattern 100000 8 >"$WORK/src/file"
	lwext4_mke2fs "$@" -d "$WORK/src" "$img" 64M
	cksum_before=$(cksum <"$img")
	acc "$img" <<EOF
mount
fail EROFS mkdir /x
fail EROFS write /y 10 1
fail EROFS remove /file
umount
EOF
	# Reading must return the right data or fail, never wrong data.
	if acc "$img" >"$WORK/read.log" 2>&1 <<EOF
mount
verify /file 100000 8
umount
EOF
	then
		how="data readable"
	elif grep -q 'mismatch\|short read\|size' "$WORK/read.log"; then
		cat "$WORK/read.log"
		die "$name: lwext4 returns wrong data"
	else
		how="reading fails with an error: $(tail -n 1 "$WORK/read.log" | sed 's/^ *//')"
	fi
	[ "$(cksum <"$img")" = "$cksum_before" ] || die "$name: image modified"
	pass "read-only compatible feature $name: read-only mount, writes refused, image untouched, $how"
}

step "unsupported features"
unsupported_incompat inline_data -t ext4 -O inline_data
unsupported_incompat large_dir -t ext4 -O large_dir
unsupported_incompat ea_inode -t ext4 -O ea_inode
unsupported_incompat journal_dev -O journal_dev
unsupported_ro quota -t ext4 -O quota
unsupported_ro bigalloc -t ext4 -O bigalloc -C 16384

# README: e2fsprogs >= 1.47 enables metadata_csum_seed and orphan_file by
# default; images for lwext4 have to be created without them.
step "e2fsprogs defaults (README note)"
img="$WORK/defaults.img"
mke2fs -q -F -t ext4 "$img" 64M
log "mke2fs -t ext4 defaults: $(features "$img")"
if has_feature "$img" metadata_csum_seed; then
	acc "$img" <<EOF
fail ENOTSUP mount
EOF
	pass "metadata_csum_seed (e2fsprogs default) is refused as README.md says"
fi
img="$WORK/readme-mke2fs.img"
mke2fs -q -F -t ext4 -O ^metadata_csum_seed,^orphan_file "$img" 64M
acc "$img" <<EOF
mount
write /f 1000 1
umount
EOF
fsck_clean "$img" "image made with README.md's mke2fs options"

finish
