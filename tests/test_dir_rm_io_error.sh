# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks: d/ with 200 files and a subdirectory with 20 more.
dir="$1.d"
mkdir -p "$dir/d/sub"
i=0
while [ $i -lt 200 ]; do
	echo "file $i" >"$dir/d/f$i"
	i=$((i + 1))
done
i=0
while [ $i -lt 20 ]; do
	echo "file $i" >"$dir/d/sub/f$i"
	i=$((i + 1))
done
lwext4_mke2fs -t ext4 -b 1024 -d "$dir" "$1" 4M
cp "$1" "$1.orig"
