#!/bin/sh
# Usage: run_test.sh <setup script> <work dir> [emulator...] <test executable>
#
# Runs the optional setup script to build a scratch image in <work dir>, then
# runs the test executable with the image path as its first argument.
#
# If a check script exists next to the setup script (test_<name>.check.sh),
# it runs after the test succeeded, on the host, with the image path as $1.
# It verifies what lwext4 wrote with independent tools (e2fsck, debugfs);
# common/check.sh has helpers for that.
set -e

setup="$1"
work="$2"
shift 2

rm -rf "$work"
mkdir -p "$work"
img="$work/image"
check="${setup%.sh}.check.sh"

if [ -f "$setup" ]; then
	(
		PATH="$PATH:/sbin:/usr/sbin"
		# Setup scripts produce bytes. The BSD tools of macOS (tr, sed,
		# sort) are locale aware: in a UTF-8 locale tr '\000' '\245'
		# writes two bytes per input byte. Use the C locale, as the
		# Linux CI containers do.
		LC_ALL=C
		export LC_ALL
		set -- "$img"
		. "$(dirname "$0")/common/mkimage.sh"
		. "$setup"
	)
fi

if [ ! -f "$check" ]; then
	exec "$@" "$img"
fi

"$@" "$img"
(
	PATH="$PATH:/sbin:/usr/sbin"
	set -- "$img"
	. "$(dirname "$0")/common/check.sh"
	. "$check"
)
