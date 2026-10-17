# SPDX-License-Identifier: BSD-3-Clause
# ext4 with 1 KiB blocks. The test makes its htree directories with
# lwext4, then damages the block bitmap with
# "sh -e $1.only-free <directory> <logical block>": every block of the
# image in use except that block of the directory.
lwext4_mke2fs -t ext4 -b 1024 -O ^has_journal "$1" 8M
cat >"$1.only-free" <<'EOF'
PATH="$PATH:/sbin:/usr/sbin"
img=${0%.only-free}
b=$(debugfs -R "bmap $1 $2" "$img" 2>/dev/null)
[ "$b" -gt 0 ]
# Free ranges of every group ("  Free blocks: 120-135, 140"), all set
dumpe2fs "$img" 2>/dev/null | sed -n 's/^  *Free blocks: //p' | tr ',' '\n' |
	awk -F- 'NF { print "setb", $1, $NF - $1 + 1 }' >"$img.cmd"
echo "freeb $b" >>"$img.cmd"
debugfs -w -f "$img.cmd" "$img" >/dev/null 2>&1
[ "$(dumpe2fs "$img" 2>/dev/null | sed -n 's/^  *Free blocks: *//p' | tr -d '\n')" = "$b" ]
EOF
