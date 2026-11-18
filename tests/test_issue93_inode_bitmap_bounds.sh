# Issue #93: s_inodes_per_group larger than the bits in one inode bitmap
# block. Group 0 is flagged INODE_UNINIT so that the first access to it
# (re)builds the inode bitmap. s_inodes_count is kept consistent with the
# (single) group so that only the per group limit is violated.
lwext4_mke2fs -t ext4 -b 4096 "$1" 8M
printf '%s\n' 'set_bg 0 flags 5' 'ssv inodes_per_group 200000' \
	'ssv inodes_count 200000' | debugfs -w "$1" >/dev/null 2>&1
