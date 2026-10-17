# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks: /u is 200 KiB of preallocated, unwritten blocks in one
# extent (debugfs fallocate, like fallocate(2) on Linux), /pre is 16 KiB of
# data with 64 unwritten blocks preallocated beyond its end. The unwritten
# blocks hold stale data (0xa5) on disk.
dir="$1.d"
mkdir -p "$dir"
: >"$dir/u"
awk 'BEGIN { for (i = 0; i < 16384; i++) printf "%c", 65 + i % 26 }' >"$dir/pre"
lwext4_mke2fs -t ext4 -b 1024 -d "$dir" "$1" 8M
debugfs -w -R "fallocate /u 0 199" "$1" 2>/dev/null
debugfs -w -R "sif /u size 204800" "$1" 2>/dev/null
debugfs -w -R "fallocate /pre 16 79" "$1" 2>/dev/null
for f in /u /pre; do
	debugfs -R "ex $f" "$1" 2>/dev/null | awk '$NF == "Uninit" { print $(NF-4), $(NF-1) }' |
	while read -r start len; do
		dd if=/dev/zero bs=1024 count="$len" 2>/dev/null | tr '\0' '\245' |
			dd of="$1" bs=1024 seek="$start" conv=notrunc 2>/dev/null
	done
done
debugfs -R "ex /u" "$1" 2>/dev/null | grep -q '0 -   199 .*200 Uninit' ||
	{ echo "/u: no unwritten extent of 200 blocks" >&2; exit 1; }
debugfs -R "ex /pre" "$1" 2>/dev/null | grep -q '16 -    79 .*64 Uninit' ||
	{ echo "/pre: no unwritten extent after the data" >&2; exit 1; }
