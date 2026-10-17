#!/bin/sh
# Usage: run_test.sh <setup script> <work dir> [emulator...] <test executable>
#
# Runs the optional setup script to build a scratch image in <work dir>, then
# runs the test executable with the image path as its first argument.
set -e

setup="$1"
work="$2"
shift 2

rm -rf "$work"
mkdir -p "$work"
img="$work/image"

if [ -f "$setup" ]; then
	(
		PATH="$PATH:/sbin:/usr/sbin"
		set -- "$img"
		. "$(dirname "$0")/common/mkimage.sh"
		. "$setup"
	)
fi

exec "$@" "$img"
