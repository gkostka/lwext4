# Issue #92: superblocks whose geometry does not fit in a single block
# bitmap. Group 0 is flagged BLOCK_UNINIT so that the first access to it
# (re)builds the block bitmap from the superblock geometry.

# s_first_meta_bg far past the number of group descriptor blocks: the
# number of descriptor blocks marked in the bitmap follows it.
lwext4_mke2fs -t ext4 -b 4096 "$1.meta_bg" 8M
printf '%s\n' 'set_bg 0 flags 6' 'feature meta_bg' \
	'ssv first_meta_bg 0x10000000' | debugfs -w "$1.meta_bg" >/dev/null 2>&1

# s_reserved_gdt_blocks larger than a block of descriptor pointers.
lwext4_mke2fs -t ext4 -b 4096 "$1.rsv_gdt" 8M
printf '%s\n' 'set_bg 0 flags 6' 'ssv reserved_gdt_blocks 60000' |
	debugfs -w "$1.rsv_gdt" >/dev/null 2>&1

# s_blocks_per_group larger than the bits in one bitmap block.
lwext4_mke2fs -t ext4 -b 4096 "$1.bpg" 8M
printf '%s\n' 'set_bg 0 flags 6' 'ssv blocks_per_group 65536' \
	'ssv clusters_per_group 65536' | debugfs -w "$1.bpg" >/dev/null 2>&1
