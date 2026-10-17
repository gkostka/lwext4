# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks, 256 byte inodes, one group: 128 files in d/.
dir="$1.d"
mkdir -p "$dir/d"
i=0
while [ $i -lt 128 ]; do
	echo "file $i" >"$dir/d/$i"
	i=$((i + 1))
done
lwext4_mke2fs -t ext4 -b 1024 -I 256 -d "$dir" "$1" 4M
