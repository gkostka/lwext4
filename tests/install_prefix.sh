#!/bin/sh
# Usage: install_prefix.sh <cmake> <build dir> <work dir>
#
# Installs the build tree into a scratch prefix chosen at install time and
# checks that everything landed below it.
set -e

cmake="$1"
build="$2"
work="$3"

rm -rf "$work"
mkdir -p "$work"

"$cmake" --install "$build" --prefix "$work/prefix" > "$work/install.log"

status=0
for f in bin/lwext4-generic bin/lwext4-mkfs lib/liblwext4.a \
	 include/lwext4/ext4.h; do
	# Windows builds install bin/<tool>.exe
	if [ ! -f "$work/prefix/$f" ] && [ ! -f "$work/prefix/$f.exe" ]; then
		echo "missing: $work/prefix/$f"
		status=1
	fi
done

if [ $status -ne 0 ]; then
	echo "install log:"
	cat "$work/install.log"
fi
exit $status
