#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Run every example in examples/ and check what it wrote with e2fsprogs.
#
# Usage: check-examples.sh <build dir>
#
# Each example writes an ext4 image; e2fsck -fn must find it clean and
# debugfs must see exactly the files and contents the example claims to
# have written. An examples/<name>/ directory without a check here fails
# the script, so a new example cannot go unchecked.
set -eu

build="$1"
PATH="$PATH:/sbin:/usr/sbin"
work="$build/examples-check"
rm -rf "$work"
mkdir -p "$work"
failed=0

fail()
{
	echo "FAIL: $name: $*"
	failed=$((failed + 1))
}

# run_example <name> <executable>: runs it with $work/<name>.img
run_example()
{
	name=$1
	img="$work/$name.img"
	echo "== $name"
	if ! "$2" "$img" >"$work/$name.log" 2>&1; then
		cat "$work/$name.log"
		fail "the example failed"
		return 1
	fi
	sed 's/^/    /' "$work/$name.log"
	if ! e2fsck -fn "$img" >"$work/$name.fsck.log" 2>&1; then
		sed 's/^/    /' "$work/$name.fsck.log"
		fail "e2fsck -fn finds errors"
		return 1
	fi
	echo "    e2fsck -fn: clean"
}

# expect_file <path> <content>: the file exists in the image with exactly
# this content (printf format).
expect_file()
{
	printf "$2" >"$work/expected"
	if ! debugfs -R "dump $1 $work/actual" "$img" >/dev/null 2>&1 ||
	    ! [ -f "$work/actual" ] || ! cmp -s "$work/expected" "$work/actual"; then
		fail "debugfs: $1 is missing or has the wrong content"
	else
		echo "    debugfs: $1 ok"
	fi
	rm -f "$work/actual"
}

# expect_ls <dir> <names...>: the directory lists exactly these entries
# (besides . and ..).
expect_ls()
{
	dir=$1
	shift
	expected=$(printf '%s\n' "$@" | LC_ALL=C sort | tr '\n' ' ')
	# ls -p prints /inode/mode/uid/gid/name/size/
	actual=$(debugfs -R "ls -p $dir" "$img" 2>/dev/null |
		awk -F/ 'NF > 5 && $6 != "." && $6 != ".." { print $6 }' |
		LC_ALL=C sort | tr '\n' ' ')
	if [ "$expected" != "$actual" ]; then
		fail "debugfs: $dir contains '$actual', expected '$expected'"
	else
		echo "    debugfs: $dir contains $actual"
	fi
}

expect_label()
{
	actual=$(dumpe2fs -h "$img" 2>/dev/null |
		sed -n 's/^Filesystem volume name: *//p')
	if [ "$actual" != "$1" ]; then
		fail "label is '$actual', expected '$1'"
	else
		echo "    label: $actual"
	fi
}

checked=
check()
{
	checked="$checked $1 "
}

check basic
if run_example basic "$build/examples/lwext4-example-basic"; then
	expect_label lwext4-basic
	expect_ls / lost+found docs
	expect_ls /docs hello.txt readme.txt
	expect_file /docs/hello.txt 'Hello from lwext4!\n'
	expect_file /docs/readme.txt 'This file is renamed below.\n'
fi

check blockdev-template
if run_example blockdev-template \
    "$build/examples/lwext4-example-blockdev-template"; then
	expect_label my_blockdev
	expect_ls /data hello.txt
	expect_file /data/hello.txt 'Written through my_blockdev.\n'
fi

# Examples with their own CI workflow (firmware for boards/emulators).
check esp-idf    # firmware, built and run in QEMU by its own workflow (esp32.yml)
check baremetal-sdcard    # firmware, built and run in Renode by its own workflow (renode.yml)
check zephyr    # firmware, built and run in QEMU by its own workflow (zephyr.yml)

for d in examples/*/; do
	d=${d%/}
	d=${d#examples/}
	case "$checked" in
	*" $d "*) ;;
	*) name=$d; fail "no check for examples/$d in $0" ;;
	esac
done

[ "$failed" -eq 0 ] || { echo "check-examples: $failed failed"; exit 1; }
echo "check-examples: all examples ok"
