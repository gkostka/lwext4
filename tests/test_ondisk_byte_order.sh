# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks, no metadata_csum (two level htrees need fix/htree-two-levels there); a 3000 entry directory indexed by
# e2fsck -D and a file with an xattr written by debugfs.
dir="$1.d"
rm -rf "$dir"
mkdir -p "$dir/big"
i=0
while [ $i -lt 3000 ]; do
	: >"$dir/big/entry_$i"
	i=$((i + 1))
done
: >"$dir/x"
lwext4_mke2fs -t ext4 -b 1024 -O ^metadata_csum -N 16384 -d "$dir" "$1" 32M
rc=0
e2fsck -fyD "$1" >/dev/null 2>&1 || rc=$?
[ $rc -le 1 ]
debugfs -w -R "ea_set /x user.color blue" "$1" >/dev/null 2>&1
rm -rf "$dir"
