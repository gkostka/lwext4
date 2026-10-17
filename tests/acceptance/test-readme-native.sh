#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Runs README.md's own commands, in README order, on the build host, and
# checks their results with e2fsprogs:
#   Compile & install tools, lwext4-generic demo application, Run automatic
#   tests (make test, make test_all), Using lwext4-mkfs tool, Run regression
#   tests (native part).
set -eu
. "$(dirname "$0")/lib.sh"

cd "$TOP_DIR"
blocks="sh $ACC_DIR/readme-blocks.sh"
README_DESTDIR="$WORK/install"
export README_DESTDIR

# lwext4-generic always writes /test1 (rw_count chunks of rw_size bytes,
# chunk i filled with '0' + i % 10) and /hello.txt.
check_generic_output()
{
	img=$1
	: >"$WORK/expect.bin"
	for i in 0 1 2 3 4 5 6 7 8 9; do
		head -c 1048576 /dev/zero | tr '\0' "$i" >>"$WORK/expect.bin"
	done
	dbg "$img" "dump /test1 $WORK/test1.bin" >/dev/null
	cmp "$WORK/test1.bin" "$WORK/expect.bin" ||
		die "$img: /test1 is not what lwext4-generic wrote"
	[ "$(dbg "$img" "cat /hello.txt")" = "Hello World !" ] ||
		die "$img: /hello.txt differs"
	pass "debugfs reads back what lwext4-generic wrote to $img"
}

step "Compile & install tools"
$blocks run compile-install-tools#1
for tool in lwext4-generic lwext4-mkfs lwext4-mbr lwext4-server lwext4-client; do
	[ -x "$README_DESTDIR/usr/local/bin/$tool" ] ||
		die "make install did not install $tool"
	"$README_DESTDIR/usr/local/bin/$tool" --version >/dev/null ||
		die "installed $tool does not run"
done
pass "make install installs lwext4-generic, -mkfs, -mbr, -server and -client"

step "Run automatic tests: make test"
$blocks run run-automatic-tests#1
pass "make test (server/client suite t0..t20 on ext2, ext3 and ext4)"
for fs in ext2 ext3 ext4; do
	img=ext_images/$fs
	fsck_clean "$img" "$fs after make test"
	[ "$(dir_names "$img" / | tr '\n' ' ')" = "lost+found " ] ||
		die "$fs: make test left files behind"
	pass "$fs: make test removed everything it created"
done

step "lwext4-generic demo application"
$blocks run lwext4-generic-demo-application#1
for fs in ext2 ext3 ext4; do
	fsck_clean "ext_images/$fs" "$fs after lwext4-generic"
	check_generic_output "ext_images/$fs"
done
$blocks run lwext4-generic-demo-application#2 >"$WORK/generic-help.txt"
grep -q -- '--sbstat' "$WORK/generic-help.txt" ||
	die "lwext4-generic --help does not show the option set"
pass "lwext4-generic --help shows the full option set"

step "Using lwext4-mkfs tool"
$blocks run using-lwext4-mkfs-tool#1
[ "$(wc -c <ext_image)" -eq 1073741824 ] || die "ext_image is not 1 GiB"
for e in 2 3 4; do
	$blocks run "using-lwext4-mkfs-tool#$e"
	fsck_clean ext_image "lwext4-mkfs -i ext_image -e $e"
	case $e in
	2) ! has_feature ext_image has_journal && ! has_feature ext_image extent ;;
	3) has_feature ext_image has_journal && ! has_feature ext_image extent ;;
	4) has_feature ext_image has_journal && has_feature ext_image extent ;;
	esac || die "-e $e created: $(features ext_image)"
	pass "lwext4-mkfs -e $e created ext$e ($(features ext_image))"
	"$README_DESTDIR/usr/local/bin/lwext4-generic" -i ext_image >/dev/null
	fsck_clean ext_image "lwext4-generic on the ext$e lwext4-mkfs created"
	check_generic_output ext_image
done
$blocks run using-lwext4-mkfs-tool#5 >"$WORK/mkfs-help.txt"
grep -q -- '--block' "$WORK/mkfs-help.txt" ||
	die "lwext4-mkfs --help does not show the option set"
pass "lwext4-mkfs --help shows the full option set"
rm -f ext_image

step "Run automatic tests: make test_all"
$blocks run run-automatic-tests#2
pass "make test_all (t0..t26 on 1 GiB ext2, ext3, ext4 images, then e2fsck)"
for fs in ext2 ext3 ext4; do
	[ "$(dir_names "ext_images/$fs" / | tr '\n' ' ')" = "lost+found " ] ||
		die "$fs: make test_all left files behind"
done
pass "make test_all removed everything it created"
rm -rf ext_images

step "Run regression tests"
$blocks run run-regression-tests#1
pass "ctest passes"

finish
