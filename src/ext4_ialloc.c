/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 *
 *
 * HelenOS:
 * Copyright (c) 2012 Martin Sucha
 * Copyright (c) 2012 Frantisek Princ
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_ialloc.c
 * @brief Inode allocation procedures.
 */

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_debug.h>

#include <ext4_trans.h>
#include <ext4_ialloc.h>
#include <ext4_super.h>
#include <ext4_crc32.h>
#include <ext4_fs.h>
#include <ext4_blockdev.h>
#include <ext4_block_group.h>
#include <ext4_bitmap.h>

/**@brief  Convert i-node number to relative index in block group.
 * @param sb    Superblock
 * @param inode I-node number to be converted
 * @return Index of the i-node in the block group
 */
static uint32_t ext4_ialloc_inode_to_bgidx(struct ext4_sblock *sb,
					   uint32_t inode)
{
	uint32_t inodes_per_group = ext4_get32(sb, inodes_per_group);
	return (inode - 1) % inodes_per_group;
}

/**@brief Convert relative index of i-node to absolute i-node number.
 * @param sb    Superblock
 * @param index Index to be converted
 * @return Absolute number of the i-node
 *
 */
static uint32_t ext4_ialloc_bgidx_to_inode(struct ext4_sblock *sb,
					   uint32_t index, uint32_t bgid)
{
	uint32_t inodes_per_group = ext4_get32(sb, inodes_per_group);
	return bgid * inodes_per_group + (index + 1);
}

/**@brief Compute block group number from the i-node number.
 * @param sb    Superblock
 * @param inode I-node number to be found the block group for
 * @return Block group number computed from i-node number
 */
static uint32_t ext4_ialloc_get_bgid_of_inode(struct ext4_sblock *sb,
					      uint32_t inode)
{
	uint32_t inodes_per_group = ext4_get32(sb, inodes_per_group);
	return (inode - 1) / inodes_per_group;
}

#if CONFIG_META_CSUM_ENABLE
static uint32_t ext4_ialloc_bitmap_csum(struct ext4_sblock *sb,	void *bitmap)
{
	uint32_t csum = 0;
	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		uint32_t inodes_per_group =
			ext4_get32(sb, inodes_per_group);

		/* First calculate crc32 checksum against fs uuid */
		csum = ext4_crc32c(EXT4_CRC32_INIT, sb->uuid, sizeof(sb->uuid));
		/* Then calculate crc32 checksum against inode bitmap */
		csum = ext4_crc32c(csum, bitmap, (inodes_per_group + 7) / 8);
	}
	return csum;
}
#else
#define ext4_ialloc_bitmap_csum(...) 0
#endif

void ext4_ialloc_set_bitmap_csum(struct ext4_sblock *sb, struct ext4_bgroup *bg,
				 void *bitmap __unused)
{
	int desc_size = ext4_sb_get_desc_size(sb);
	uint32_t csum = ext4_ialloc_bitmap_csum(sb, bitmap);
	uint16_t lo_csum = to_le16(csum & 0xFFFF),
		 hi_csum = to_le16(csum >> 16);

	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return;

	/* See if we need to assign a 32bit checksum */
	bg->inode_bitmap_csum_lo = lo_csum;
	if (desc_size == EXT4_MAX_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->inode_bitmap_csum_hi = hi_csum;

}

#if CONFIG_META_CSUM_ENABLE
static bool
ext4_ialloc_verify_bitmap_csum(struct ext4_sblock *sb, struct ext4_bgroup *bg,
			       void *bitmap __unused)
{

	int desc_size = ext4_sb_get_desc_size(sb);
	uint32_t csum = ext4_ialloc_bitmap_csum(sb, bitmap);
	uint16_t lo_csum = to_le16(csum & 0xFFFF),
		 hi_csum = to_le16(csum >> 16);

	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return true;

	if (bg->inode_bitmap_csum_lo != lo_csum)
		return false;

	if (desc_size == EXT4_MAX_BLOCK_GROUP_DESCRIPTOR_SIZE)
		if (bg->inode_bitmap_csum_hi != hi_csum)
			return false;

	return true;
}
#else
#define ext4_ialloc_verify_bitmap_csum(...) true
#endif

int ext4_ialloc_free_inode(struct ext4_fs *fs, uint32_t index, bool is_dir)
{
	struct ext4_sblock *sb = &fs->sb;

	/* Compute index of block group and load it */
	uint32_t block_group = ext4_ialloc_get_bgid_of_inode(sb, index);

	struct ext4_block_group_ref bg_ref;
	int rc = ext4_fs_get_block_group_ref(fs, block_group, &bg_ref);
	if (rc != EOK)
		return rc;

	struct ext4_bgroup *bg = bg_ref.block_group;

	/* Load i-node bitmap */
	ext4_fsblk_t bitmap_block_addr =
	    ext4_bg_get_inode_bitmap(bg, sb);

	struct ext4_block b;
	rc = ext4_trans_block_get(fs->bdev, &b, bitmap_block_addr);
	if (rc != EOK)
		return rc;

	if (!ext4_ialloc_verify_bitmap_csum(sb, bg, b.data)) {
		ext4_dbg(DEBUG_IALLOC,
			DBG_WARN "Bitmap checksum failed."
			"Group: %" PRIu32"\n",
			bg_ref.index);
	}

	/* Free i-node in the bitmap */
	uint32_t index_in_group = ext4_ialloc_inode_to_bgidx(sb, index);
	ext4_bmap_bit_clr(b.data, index_in_group);
	ext4_ialloc_set_bitmap_csum(sb, bg, b.data);
	ext4_trans_set_block_dirty(b.buf);

	/* Put back the block with bitmap */
	rc = ext4_block_set(fs->bdev, &b);
	if (rc != EOK) {
		/* Error in saving bitmap */
		ext4_fs_put_block_group_ref(&bg_ref);
		return rc;
	}

	/* If released i-node is a directory, decrement used directories count
	 */
	if (is_dir) {
		uint32_t bg_used_dirs = ext4_bg_get_used_dirs_count(bg, sb);
		bg_used_dirs--;
		ext4_bg_set_used_dirs_count(bg, sb, bg_used_dirs);
	}

	/* Update block group free inodes count */
	uint32_t free_inodes = ext4_bg_get_free_inodes_count(bg, sb);
	free_inodes++;
	ext4_bg_set_free_inodes_count(bg, sb, free_inodes);

	bg_ref.dirty = true;

	/* Put back the modified block group */
	rc = ext4_fs_put_block_group_ref(&bg_ref);
	if (rc != EOK)
		return rc;

	/* Update superblock free inodes count */
	ext4_set32(sb, free_inodes_count,
		   ext4_get32(sb, free_inodes_count) + 1);

	return EOK;
}

int ext4_ialloc_alloc_inode(struct ext4_fs *fs, uint32_t *idx, bool is_dir)
{
	struct ext4_sblock *sb = &fs->sb;

	uint32_t bgid = fs->last_inode_bg_id;
	uint32_t bg_count = ext4_block_group_cnt(sb);
	uint32_t sb_free_inodes = ext4_get32(sb, free_inodes_count);
	bool rewind = false;

	/* Try to find free i-node in all block groups */
	while (bgid <= bg_count) {

		if (bgid == bg_count) {
			if (rewind)
				break;
			bg_count = fs->last_inode_bg_id;
			bgid = 0;
			rewind = true;
			continue;
		}

		/* Load block group to check */
		struct ext4_block_group_ref bg_ref;
		int rc = ext4_fs_get_block_group_ref(fs, bgid, &bg_ref);
		if (rc != EOK)
			return rc;

		struct ext4_bgroup *bg = bg_ref.block_group;

		/* Read necessary values for algorithm */
		uint32_t free_inodes = ext4_bg_get_free_inodes_count(bg, sb);
		uint32_t used_dirs = ext4_bg_get_used_dirs_count(bg, sb);

		/* Check if this block group is good candidate for allocation */
		if (free_inodes > 0) {
			/* Load block with bitmap */
			ext4_fsblk_t bmp_blk_add = ext4_bg_get_inode_bitmap(bg, sb);

			struct ext4_block b;
			rc = ext4_trans_block_get(fs->bdev, &b, bmp_blk_add);
			if (rc != EOK) {
				ext4_fs_put_block_group_ref(&bg_ref);
				return rc;
			}

			if (!ext4_ialloc_verify_bitmap_csum(sb, bg, b.data)) {
				ext4_dbg(DEBUG_IALLOC,
					DBG_WARN "Bitmap checksum failed."
					"Group: %" PRIu32"\n",
					bg_ref.index);
			}

			/* Try to allocate i-node in the bitmap */
			uint32_t inodes_in_bg;
			uint32_t idx_in_bg;

			inodes_in_bg = ext4_inodes_in_group_cnt(sb, bgid);
			rc = ext4_bmap_bit_find_clr(b.data, 0, inodes_in_bg,
						    &idx_in_bg);
			/* Block group has not any free i-node */
			if (rc == ENOSPC) {
				rc = ext4_block_set(fs->bdev, &b);
				if (rc != EOK) {
					ext4_fs_put_block_group_ref(&bg_ref);
					return rc;
				}

				rc = ext4_fs_put_block_group_ref(&bg_ref);
				if (rc != EOK)
					return rc;

				continue;
			}

			ext4_bmap_bit_set(b.data, idx_in_bg);

			/* Free i-node found, save the bitmap */
			ext4_ialloc_set_bitmap_csum(sb,bg,
						    b.data);
			ext4_trans_set_block_dirty(b.buf);

			ext4_block_set(fs->bdev, &b);
			if (rc != EOK) {
				ext4_fs_put_block_group_ref(&bg_ref);
				return rc;
			}

			/* Modify filesystem counters */
			free_inodes--;
			ext4_bg_set_free_inodes_count(bg, sb, free_inodes);

			/* Increment used directories counter */
			if (is_dir) {
				used_dirs++;
				ext4_bg_set_used_dirs_count(bg, sb, used_dirs);
			}

			/* Decrease unused inodes count */
			uint32_t unused =
			    ext4_bg_get_itable_unused(bg, sb);

			uint32_t free = inodes_in_bg - unused;

			if (idx_in_bg >= free) {
				unused = inodes_in_bg - (idx_in_bg + 1);
				ext4_bg_set_itable_unused(bg, sb, unused);
			}

			/* Save modified block group */
			bg_ref.dirty = true;

			rc = ext4_fs_put_block_group_ref(&bg_ref);
			if (rc != EOK)
				return rc;

			/* Update superblock */
			sb_free_inodes--;
			ext4_set32(sb, free_inodes_count, sb_free_inodes);

			/* Compute the absolute i-nodex number */
			*idx = ext4_ialloc_bgidx_to_inode(sb, idx_in_bg, bgid);

			fs->last_inode_bg_id = bgid;

			return EOK;
		}

		/* Block group not modified, put it and jump to the next block
		 * group */
		ext4_fs_put_block_group_ref(&bg_ref);
		if (rc != EOK)
			return rc;

		++bgid;
	}

	return ENOSPC;
}

/**
 * @}
 */
