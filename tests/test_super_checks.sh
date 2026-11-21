# SPDX-License-Identifier: BSD-3-Clause
# ext4, 1 KiB blocks, 16 MiB: $1 without metadata_csum (the superblock
# fields can be patched without fixing a checksum), $1.csum with it, and
# $1.metabg with meta_bg. Each has /file.
dir="$1.d"
mkdir -p "$dir"
echo "superblock checks" >"$dir/file"
lwext4_mke2fs -t ext4 -b 1024 -O 64bit,^metadata_csum -d "$dir" "$1" 16M
lwext4_mke2fs -t ext4 -b 1024 -O 64bit,metadata_csum -d "$dir" "$1.csum" 16M
lwext4_mke2fs -t ext4 -b 1024 -O meta_bg,^resize_inode -d "$dir" "$1.metabg" 16M
