# Issue #50: block group descriptors that claim free inodes while their
# inode bitmaps are full. 8 groups of 16 inodes each; metadata_csum and
# uninit_bg are off so the descriptors and bitmaps can be edited directly.
#
# $1:      only group 0's inode bitmap is full; later groups have room.
# $1.full: every inode bitmap is full.
# In both images the descriptors keep their original free inode counts.
for img in "$1" "$1.full"; do
	lwext4_mke2fs -t ext4 -O ^metadata_csum,^uninit_bg -b 1024 -g 1024 \
		-N 128 "$img" 8M
done
debugfs -w -R "seti <1> 16" "$1" >/dev/null 2>&1
debugfs -w -R "seti <1> 128" "$1.full" >/dev/null 2>&1
