#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# README "journaling transactions & recovery - power loss resistance" and the
# documented ext4_journal_start/stop, ext4_recover and ext4_cache_write_back
# usage (include/ext4.h).
#
#  1. Journals written by e2fsprogs (debugfs journal_write) are replayed by
#     ext4_recover(): the logged blocks reach their home location, revoked
#     blocks do not, and the result passes e2fsck.
#  2. Power loss at every single block write of a workload: the image left
#     behind must be consistent after recovery, both when lwext4 recovers it
#     (ext4_recover) and when e2fsprogs does (e2fsck journal replay).
#  3. The write-through / write-back cache semantics documented for
#     ext4_cache_write_back() and ext4_cache_flush().
set -eu
. "$(dirname "$0")/lib.sh"

# Crash sweep step (1 = every write). CI uses 1; raise it for quick local runs.
CRASH_STEP=${CRASH_STEP:-1}

# ---------------------------------------------------------------------------
# 1. e2fsprogs writes the journal, lwext4 replays it
# ---------------------------------------------------------------------------

# replay_case <name> <mke2fs options> <journal_open options>
replay_case()
{
	name=$1
	mkfs_opts=$2
	jo_opts=$3
	img="$WORK/replay-$name.img"
	step "lwext4 replays a journal written by e2fsprogs: $name"
	rm -rf "$WORK/src"
	mkdir -p "$WORK/src"
	for f in a b c; do
		pattern 4096 1 >"$WORK/src/$f"
	done
	# shellcheck disable=SC2086
	lwext4_mke2fs $mkfs_opts -b 4096 -d "$WORK/src" "$img" 32M
	blk_a=$(dbg "$img" "bmap /a 0")
	blk_b=$(dbg "$img" "bmap /b 0")
	blk_c=$(dbg "$img" "bmap /c 0")
	pattern 4096 2 >"$WORK/new2.bin"
	pattern 4096 3 >"$WORK/new3.bin"
	pattern 4096 4 >"$WORK/new4.bin"
	# Every journal_write is a transaction: new contents for a, b and c,
	# then a revoke record for b. After replay a and c are new, b is old.
	cat >"$WORK/journal.cmds" <<EOF
jo $jo_opts
jw -b $blk_a $WORK/new2.bin
jw -b $blk_b $WORK/new4.bin
jw -b $blk_c $WORK/new3.bin
jw -r $blk_b
jc
EOF
	debugfs -w -f "$WORK/journal.cmds" "$img" >"$WORK/debugfs.log" 2>&1 ||
		{ cat "$WORK/debugfs.log"; die "debugfs could not write the journal"; }
	has_feature "$img" needs_recovery ||
		die "debugfs did not leave a journal to recover"
	dbg "$img" "logdump" >"$WORK/logdump.txt"
	expect_match "$(cat "$WORK/logdump.txt")" 'revoke' "journal has a revoke record"
	n=$(grep -c 'commit block' "$WORK/logdump.txt")
	[ "$n" -eq 4 ] || die "expected 4 journal transactions, found $n"
	pass "$name: e2fsprogs left 4 transactions (3 blocks, 1 revoke) to replay"

	acc "$img" <<EOF
mount
recover
verify /a 4096 2
verify /b 4096 1
verify /c 4096 3
journal_start
write /after 5000 9
journal_stop
umount
EOF
	pass "$name: lwext4 replayed the journal (a, c new; revoked b old)"
	if has_feature "$img" needs_recovery; then
		die "needs_recovery still set after ext4_recover"
	fi
	pass "$name: needs_recovery cleared"
	fsck_clean "$img" "$name after lwext4 replay"
	dump_cmp "$img" /a 4096 2
	dump_cmp "$img" /b 4096 1
	dump_cmp "$img" /c 4096 3
	dump_cmp "$img" /after 5000 9
}

replay_case ext4 "-t ext4" ""
replay_case ext4-journal-csum "-t ext4" "-c"
replay_case ext3 "-t ext3" ""

# ---------------------------------------------------------------------------
# 2. Power loss at every block write
# ---------------------------------------------------------------------------

workload()
{
	cat <<EOF
mount
recover
journal_start
cache_write_back $1
mkdir /d
write /d/a 20000 1
write /d/b 300000 2 4096
mkdir /d/e
rename /d/a /d/a2
link /d/b /d/c
symlink /d/b /s
symlink /$LONG_TARGET /slow
setxattr /d/b user.x value
remove /d/c
write /d/e/f 5000 3
truncate /d/b 1000
mknod /d/e/fifo fifo 0
chmod /d/b 600
dir_mv /d/e /moved
dir_rm /moved
write /big 17000000 4 1048576
remove /big
cache_write_back 0
journal_stop
umount
EOF
}
LONG_TARGET=a/symlink/target/that/is/long/enough/to/need/a/data/block/of/its/own

# crash_case <name> <write back 0|1> <mke2fs options...>
crash_case()
{
	name=$1
	wb=$2
	shift 2
	step "power loss at every block write: $name"
	base="$WORK/crash-$name.img"
	lwext4_mke2fs "$@" "$base" 64M
	# Warm-up: lwext4 initialises lazily initialised inode tables on the
	# first mount; do that once so the sweep covers the workload only.
	acc "$base" <<EOF
mount
recover
umount
EOF
	workload "$wb" >"$WORK/workload.txt"
	cp "$base" "$WORK/full.img"
	# The power loss counter starts after "mount" (see below), so count
	# the writes from there on.
	{ echo mount; echo writes; sed 1d "$WORK/workload.txt"; echo writes; } |
		acc "$WORK/full.img" >"$WORK/full.log"
	total=$(sed -n 's/^writes //p' "$WORK/full.log" |
		awk 'NR == 1 { first = $1 } END { print $1 - first }')
	fsck_clean "$WORK/full.img" "$name without power loss"
	acc "$WORK/full.img" <<EOF
mount ro
verify /d/b 1000 2
verify /d/a2 20000 1
missing /d/a
missing /d/e
missing /d/c
missing /moved
missing /big
readlink /s /d/b
readlink /slow /$LONG_TARGET
getxattr /d/b user.x value
mode /d/b 600
umount
EOF
	pass "$name: workload result verified ($total block writes)"

	printf 'mount\nrecover\njournal_start\njournal_stop\numount\n' >"$WORK/recover.txt"
	n=0
	points=0
	while [ "$n" -lt "$total" ]; do
		cp "$base" "$WORK/t.img"
		{ echo mount; echo "crash_after $n"; sed 1d "$WORK/workload.txt"; } |
			acc "$WORK/t.img" >"$WORK/crash.log"
		grep -q '^crash after' "$WORK/crash.log" ||
			die "$name: no power loss at write $n"
		where=$(sed -n 's/^crash after [0-9]* writes, in //p' "$WORK/crash.log")
		cp "$WORK/t.img" "$WORK/t2.img"

		# lwext4 recovers
		acc "$WORK/t.img" <"$WORK/recover.txt" >"$WORK/recover.log" 2>&1 ||
			{ cat "$WORK/recover.log"
			  die "$name: ext4_recover failed after power loss at write $n ($where)"; }
		if ! e2fsck -fn "$WORK/t.img" >"$WORK/fsck.log" 2>&1; then
			sed 's/^/    /' "$WORK/fsck.log" | head -40
			die "$name: inconsistent after power loss at write $n ($where) and ext4_recover"
		fi

		# e2fsprogs recovers (journal replay only, then a read-only check)
		rc=0
		e2fsck -y -E journal_only "$WORK/t2.img" >"$WORK/replay.log" 2>&1 || rc=$?
		[ "$rc" -le 1 ] ||
			{ cat "$WORK/replay.log"
			  die "$name: e2fsck could not replay the journal after write $n"; }
		if ! e2fsck -fn "$WORK/t2.img" >"$WORK/fsck.log" 2>&1; then
			sed 's/^/    /' "$WORK/fsck.log" | head -40
			die "$name: inconsistent after power loss at write $n ($where) and e2fsck journal replay"
		fi
		points=$((points + 1))
		n=$((n + CRASH_STEP))
	done
	pass "$name: consistent after power loss at each of $points write positions (lwext4 and e2fsprogs recovery)"
}

crash_case ext4-1k-write-through 0 -t ext4 -b 1024
crash_case ext4-1k-write-back 1 -t ext4 -b 1024
crash_case ext4-4k-write-back 1 -t ext4 -b 4096
crash_case ext3-1k-write-back 1 -t ext3 -b 1024

# ---------------------------------------------------------------------------
# 3. Cache modes (include/ext4.h, ext4_cache_write_back / ext4_cache_flush)
# ---------------------------------------------------------------------------
step "write-through and write-back cache"
img="$WORK/cache.img"
lwext4_mke2fs -t ext2 -b 1024 "$img" 32M

# "Default model of cache is write through": data is on the disk as soon as
# ext4_fwrite returns, even if the program never unmounts.
acc "$img" >"$WORK/cache.log" <<EOF
mount
mkdir /wt
write /wt/f 100000 5
crash
EOF
dump_cmp "$img" /wt/f 100000 5
pass "write-through: data reaches the image without umount"

# ext4_cache_write_back(..., 1): metadata stays in the cache until write
# back is disabled again (as often as it was enabled). ext4_fwrite() file
# data is not cached, it goes to the device right away (include/ext4.h).
acc "$img" >"$WORK/cache.log" <<EOF
mount
cache_write_back 1
cache_write_back 1
writes_at_most 0 mkdir /wbdir
writes_at_most 0 mkdir /wbdir/sub
io write /wb1 3000 6
writes_at_most 0 cache_write_back 0
crash
EOF
data_writes=$(sed -n 's/^io write: reads [0-9]* writes //p' "$WORK/cache.log")
[ "$data_writes" -eq 3 ] ||
	die "write back: ext4_fwrite of 3 blocks did $data_writes block writes"
for p in /wbdir /wb1; do
	if debugfs -R "stat $p" "$img" 2>&1 | grep -q '^Inode:'; then
		die "write back: metadata of $p reached the image before the flush"
	fi
done
fsck_clean "$img" "power loss with metadata still in the write back cache"
pass "write back: metadata stays in the cache (nothing written), file data (3 blocks) is written directly"

acc "$img" >"$WORK/cache.log" <<EOF
mount
cache_write_back 1
cache_write_back 1
writes_at_most 0 mkdir /wbdir
writes_at_most 1 write /wbdir/f 100 6
writes_at_most 0 cache_write_back 0
io cache_write_back 0
crash
EOF
writes=$(sed -n 's/^io cache_write_back: reads [0-9]* writes //p' "$WORK/cache.log" | tail -n 1)
[ "$writes" -gt 0 ] || die "disabling write back flushed nothing"
dump_cmp "$img" /wbdir/f 100 6
pass "write back: the last ext4_cache_write_back(0) flushed $writes blocks"

# ext4_cache_flush() writes the cache out while write back stays enabled
acc "$img" >"$WORK/cache.log" <<EOF
mount
cache_write_back 1
writes_at_most 0 mkdir /flushdir
write /flushdir/f 50000 7
io cache_flush
crash
EOF
writes=$(sed -n 's/^io cache_flush: reads [0-9]* writes //p' "$WORK/cache.log")
[ "$writes" -gt 0 ] || die "ext4_cache_flush wrote nothing"
dump_cmp "$img" /flushdir/f 50000 7
pass "ext4_cache_flush: metadata on the image ($writes block writes) while write back stays on"

finish
