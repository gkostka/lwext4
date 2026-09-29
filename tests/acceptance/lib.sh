# SPDX-License-Identifier: BSD-3-Clause
#
# Shared helpers for the README acceptance tests. Source it after setting
# "set -eu"; every helper fails the calling script on error.
#
# Environment:
#   BUILD  build directory with the fs_test tools and lwext4-acceptance
#          (default: build_generic)
#   EMU    optional emulator command line for the lwext4 binaries, e.g.
#          "qemu-s390x -L /usr/s390x-linux-gnu". e2fsprogs always run on
#          the host and act as the independent oracle.
#   WORK   scratch directory (default: build_acceptance/<build dir>/<script>)

PATH="$PATH:/sbin:/usr/sbin"
export PATH

ACC_DIR=$(cd "$(dirname "$0")" && pwd)
TOP_DIR=$(cd "$ACC_DIR/../.." && pwd)
BUILD=${BUILD:-build_generic}
case "$BUILD" in
/*) ;;
*) BUILD="$TOP_DIR/$BUILD" ;;
esac
EMU=${EMU:-}
WORK=${WORK:-$TOP_DIR/build_acceptance/$(basename "$BUILD")/$(basename "$0" .sh)}

ACC="$BUILD/tests/acceptance/lwext4-acceptance"
GENERIC="$BUILD/fs_test/lwext4-generic"
MKFS="$BUILD/fs_test/lwext4-mkfs"
MBR="$BUILD/fs_test/lwext4-mbr"
SERVER="$BUILD/fs_test/lwext4-server"
CLIENT="$BUILD/fs_test/lwext4-client"

rm -rf "$WORK"
mkdir -p "$WORK"

CHECKS=0

log()
{
	printf '%s\n' "$*"
}

step()
{
	printf '\n=== %s\n' "$*"
}

die()
{
	printf 'FAIL: %s\n' "$*" >&2
	exit 1
}

pass()
{
	CHECKS=$((CHECKS + 1))
	printf 'ok %d - %s\n' "$CHECKS" "$*"
}

finish()
{
	printf '\n%s: all %d checks passed\n' "$(basename "$0")" "$CHECKS"
}

# Run an lwext4 binary (through $EMU when cross testing).
lw()
{
	# shellcheck disable=SC2086
	$EMU "$@"
}

# acc <image> <<EOF ... EOF: run an lwext4-acceptance script from stdin
acc()
{
	lw "$ACC" "$1"
}

# pattern <size> <seed> [offset]: the data pattern lwext4-acceptance writes
pattern()
{
	lw "$ACC" --pattern "$@"
}

# mke2fs wrapper: newer e2fsprogs enable orphan_file and metadata_csum_seed,
# which lwext4 does not support.
lwext4_mke2fs()
{
	mke2fs -q -F -O ^orphan_file,^metadata_csum_seed "$@" 2>/dev/null ||
		mke2fs -q -F "$@"
}

# fsck_clean <image> <what>: e2fsck -fn must find nothing to fix.
fsck_clean()
{
	if ! e2fsck -fn "$1" >"$WORK/fsck.log" 2>&1; then
		sed 's/^/    /' "$WORK/fsck.log" | head -60
		die "e2fsck -fn $1 reports errors ($2)"
	fi
	pass "e2fsck -fn clean: $2"
}

# dbg <image> <request>: run a debugfs request, fail on debugfs errors.
dbg()
{
	out=$(debugfs -R "$2" "$1" 2>"$WORK/debugfs.err") ||
		die "debugfs -R '$2' failed: $(cat "$WORK/debugfs.err")"
	# debugfs reports most request errors on stderr but still exits 0.
	if grep -v '^debugfs [0-9]' "$WORK/debugfs.err" | grep -q .; then
		cat "$WORK/debugfs.err" >&2
		die "debugfs -R '$2' reported an error"
	fi
	printf '%s\n' "$out"
}

# dump_cmp <image> <path> <size> <seed>: read a file with debugfs and
# compare it with the pattern.
dump_cmp()
{
	rm -f "$WORK/dump.bin"
	dbg "$1" "dump $2 $WORK/dump.bin" >/dev/null
	[ -f "$WORK/dump.bin" ] || die "debugfs could not dump $2"
	pattern "$3" "$4" >"$WORK/expect.bin"
	cmp "$WORK/dump.bin" "$WORK/expect.bin" ||
		die "debugfs sees different data in $2"
	pass "debugfs reads back $2 ($3 bytes)"
}

# dir_names <image> <dir>: names in a directory according to debugfs, one per
# line, without "." and "..". debugfs "ls -p" also prints the placeholder a
# deleted first entry of a directory block leaves behind (inode 0); those
# are not directory entries.
dir_names()
{
	dbg "$1" "ls -p $2" | awk -F/ '$2 != "" && $2 != 0 && $6 != "." && $6 != ".." { print $6 }'
}

# dump_range <image> <path> <offset> <length>: file content in that range,
# read through the block map debugfs reports (debugfs "dump" would have to
# write out the whole, possibly huge, sparse file). Holes read as zeros.
dump_range()
{
	bs=$(dumpe2fs -h "$1" 2>/dev/null | sed -n 's/^Block size: *//p')
	first=$(($3 / bs))
	last=$((($3 + $4 - 1) / bs))
	seq "$first" "$last" | sed "s|^|bmap $2 |" >"$WORK/bmap.cmds"
	debugfs -f "$WORK/bmap.cmds" "$1" 2>/dev/null | grep -v '^debugfs' \
		>"$WORK/bmap.out"
	[ "$(wc -l <"$WORK/bmap.out")" -eq $((last - first + 1)) ] ||
		die "debugfs bmap failed for $2"
	# Merge physically contiguous blocks into runs: "<phys> <count>"
	awk 'NR == 1 || $1 == 0 || $1 != prev + 1 || prev == 0 {
		if (NR > 1) print start, n
		start = $1; n = 0 }
	{ n++; prev = $1 }
	END { print start, n }' "$WORK/bmap.out" |
	while read -r phys count; do
		if [ "$phys" -eq 0 ]; then
			head -c $((count * bs)) /dev/zero
		else
			dd if="$1" bs="$bs" skip="$phys" count="$count" 2>/dev/null
		fi
	done | tail -c +$(($3 - first * bs + 1)) | head -c "$4"
}

# file_size <image> <path>: i_size according to debugfs
file_size()
{
	dbg "$1" "stat $2" | sed -n '/^User:/s/.*Size: *\([0-9]*\)$/\1/p'
}

# expect_match <text> <extended regex> <what>
expect_match()
{
	printf '%s\n' "$1" | grep -Eq -- "$2" ||
		{ printf '%s\n' "$1" | sed 's/^/    /' | head -40
		  die "$3: no match for /$2/"; }
	pass "$3"
}

# features <image>: filesystem features line from dumpe2fs
features()
{
	dumpe2fs -h "$1" 2>/dev/null | sed -n 's/^Filesystem features: *//p'
}

has_feature()
{
	features "$1" | tr ' ' '\n' | grep -qx "$2"
}
