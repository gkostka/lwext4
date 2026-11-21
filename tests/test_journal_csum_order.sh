# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks, 1 MiB journal with checksums: $1 journal_checksum_v3,
# $1.v2 journal_checksum_v2 (mke2fs makes journals without checksums,
# debugfs "jo -c" turns them on). $1.jstart and $1.v2.jstart: the first
# block of the journal, which is contiguous.
# $1.replay: v3 checksums, /target (4 KiB of 'X') and two transactions
# debugfs wrote and did not replay: 'Y' over blocks 0 and 1 of /target,
# then 'Z' over block 2 with a revoke record for block 1. Replaying them
# leaves Y, X, Z, X in the four blocks.
for v in 3 2; do
	img=$1
	[ $v = 3 ] || img=$1.v$v
	lwext4_mke2fs -t ext4 -b 1024 -J size=1 "$img" 16M
	printf 'jo -c -v %s\njc\n' $v >"$img.cmds"
	debugfs -w -f "$img.cmds" "$img" >/dev/null 2>&1
	dumpe2fs -h "$img" 2>/dev/null |
		grep -q "Journal features:.*journal_checksum_v$v" ||
		{ echo "$img: no journal_checksum_v$v" >&2; exit 1; }
	debugfs -R 'stat <8>' "$img" 2>/dev/null |
		sed -n 's/^(0-1023):\([0-9]*\)-[0-9]*$/\1/p' >"$img.jstart"
	[ -s "$img.jstart" ] || { echo "$img: journal not contiguous" >&2; exit 1; }
done

dir="$1.d"
mkdir -p "$dir"
awk 'BEGIN { for (i = 0; i < 4096; i++) printf "X" }' >"$dir/target"
awk 'BEGIN { for (i = 0; i < 2048; i++) printf "Y" }' >"$1.y"
awk 'BEGIN { for (i = 0; i < 1024; i++) printf "Z" }' >"$1.z"
lwext4_mke2fs -t ext4 -b 1024 -J size=1 -d "$dir" "$1.replay" 16M
b0=$(debugfs -R "bmap /target 0" "$1.replay" 2>/dev/null)
b1=$(debugfs -R "bmap /target 1" "$1.replay" 2>/dev/null)
b2=$(debugfs -R "bmap /target 2" "$1.replay" 2>/dev/null)
printf 'jo -c\njw -b %s,%s %s\njw -b %s -r %s %s\njc\n' \
	"$b0" "$b1" "$1.y" "$b2" "$b1" "$1.z" >"$1.replay.cmds"
debugfs -w -f "$1.replay.cmds" "$1.replay" >/dev/null 2>&1
debugfs -R logdump "$1.replay" 2>/dev/null | grep -q 'revoke table' ||
	{ echo "$1.replay: no transactions to replay" >&2; exit 1; }
