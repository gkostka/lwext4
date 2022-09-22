/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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
 * @file  ext4_balloc.c
 * @brief Physical block allocator.
 */

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_debug.h>

#include <ext4_trans.h>
#include <ext4_balloc.h>
#include <ext4_super.h>
#include <ext4_crc32.h>
#include <ext4_block_group.h>
#include <ext4_fs.h>
#include <ext4_bitmap.h>
#include <ext4_inode.h>

/**@brief Compute number of block group from block address.
 * @param s superblock pointer.
 * @param baddr Absolute address of block.
 * @return Block group index
 */
uint32_t ext4_balloc_get_bgid_of_block(struct ext4_sblock *s,
				       uint64_t baddr)
{
	if (ext4_get32(s, first_data_block) && baddr)
		baddr--;

	return (uint32_t)(baddr / ext4_get32(s, blocks_per_group));
}

/**@brief Compute the starting block address of a block group
 * @param s   superblock pointer.
 * @param bgid block group index
 * @return Block address
 */
uint64_t ext4_balloc_get_block_of_bgid(struct ext4_sblock *s,
				       uint32_t bgid)
{
	uint64_t baddr = 0;
	if (ext4_get32(s, first_data_block))
		baddr++;

	baddr += bgid * ext4_get32(s, blocks_per_group);
	return baddr;
}

#if CONFIG_META_CSUM_ENABLE
static uint32_t ext4_balloc_bitmap_csum(struct ext4_sblock *sb,
					void *bitmap)
{
	uint32_t checksum = 0;
	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		uint32_t blocks_per_group = ext4_get32(sb, blocks_per_group);

		/* First calculate crc32 checksum against fs uuid */
		checksum = ext4_crc32c(EXT4_CRC32_INIT, sb->uuid,
				sizeof(sb->uuid));
		/* Then calculate crc32 checksum against block_group_desc */
		checksum = ext4_crc32c(checksum, bitmap, blocks_per_group / 8);
	}
	return checksum;
}
#else
#define ext4_balloc_bitmap_csum(...) 0
#endif

void ext4_balloc_set_bitmap_csum(struct ext4_sblock *sb,
				 struct ext4_bgroup *bg,
				 void *bitmap __unused)
{
	int desc_size = ext4_sb_get_desc_size(sb);
	uint32_t checksum = ext4_balloc_bitmap_csum(sb, bitmap);
	uint16_t lo_checksum = to_le16(checksum & 0xFFFF),
		 hi_checksum = to_le16(checksum >> 16);

	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return;

	/* See if we need to assign a 32bit checksum */
	bg->block_bitmap_csum_lo = lo_checksum;
	if (desc_size == EXT4_MAX_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->block_bitmap_csum_hi = hi_checksum;

}

#if CONFIG_META_CSUM_ENABLE
static bool
ext4_balloc_verify_bitmap_csum(struct ext4_sblock *sb,
			       struct ext4_bgroup *bg,
			       void *bitmap __unused)
{
	int desc_size = ext4_sb_get_desc_size(sb);
	uint32_t checksum = ext4_balloc_bitmap_csum(sb, bitmap);
	uint16_t lo_checksum = to_le16(checksum & 0xFFFF),
		 hi_checksum = to_le16(checksum >> 16);

	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return true;

	if (bg->block_bitmap_csum_lo != lo_checksum)
		return false;

	if (desc_size == EXT4_MAX_BLOCK_GROUP_DESCRIPTOR_SIZE)
		if (bg->block_bitmap_csum_hi != hi_checksum)
			return false;

	return true;
}
#else
#define ext4_balloc_verify_bitmap_csum(...) true
#endif

int ext4_balloc_free_block(struct ext4_inode_ref *inode_ref, ext4_fsblk_t baddr)
{
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_sblock *sb = &fs->sb;

	uint32_t bg_id = ext4_balloc_get_bgid_of_block(sb, baddr);
	uint32_t index_in_group = ext4_fs_addr_to_idx_bg(sb, baddr);

	/* Load block group reference */
	struct ext4_block_group_ref bg_ref;
	int rc = ext4_fs_get_block_group_ref(fs, bg_id, &bg_ref);
	if (rc != EOK)
		return rc;

	struct ext4_bgroup *bg = bg_ref.block_group;

	/* Load block with bitmap */
	ext4_fsblk_t bitmap_block_addr =
	    ext4_bg_get_block_bitmap(bg, sb);

	struct ext4_block bitmap_block;

	rc = ext4_trans_block_get(fs->bdev, &bitmap_block, bitmap_block_addr);
	if (rc != EOK) {
		ext4_fs_put_block_group_ref(&bg_ref);
		return rc;
	}

	if (!ext4_balloc_verify_bitmap_csum(sb, bg, bitmap_block.data)) {
		ext4_dbg(DEBUG_BALLOC,
			DBG_WARN "Bitmap checksum failed."
			"Group: %" PRIu32"\n",
			bg_ref.index);
	}

	/* Modify bitmap */
	ext4_bmap_bit_clr(bitmap_block.data, index_in_group);
	ext4_balloc_set_bitmap_csum(sb, bg, bitmap_block.data);
	ext4_trans_set_block_dirty(bitmap_block.buf);

	/* Release block with bitmap */
	rc = ext4_block_set(fs->bdev, &bitmap_block);
	if (rc != EOK) {
		/* Error in saving bitmap */
		ext4_fs_put_block_group_ref(&bg_ref);
		return rc;
	}

	uint32_t block_size = ext4_sb_get_block_size(sb);

	/* Update superblock free blocks count */
	uint64_t sb_free_blocks = ext4_sb_get_free_blocks_cnt(sb);
	sb_free_blocks++;
	ext4_sb_set_free_blocks_cnt(sb, sb_free_blocks);

	/* Update inode blocks count */
	uint64_t ino_blocks = ext4_inode_get_blocks_count(sb, inode_ref->inode);
	ino_blocks -= block_size / EXT4_INODE_BLOCK_SIZE;
	ext4_inode_set_blocks_count(sb, inode_ref->inode, ino_blocks);
	inode_ref->dirty = true;

	/* Update block group free blocks count */
	uint32_t free_blocks = ext4_bg_get_free_blocks_count(bg, sb);
	free_blocks++;
	ext4_bg_set_free_blocks_count(bg, sb, free_blocks);

	bg_ref.dirty = true;

	rc = ext4_trans_try_revoke_block(fs->bdev, baddr);
	if (rc != EOK) {
		bg_ref.dirty = false;
		ext4_fs_put_block_group_ref(&bg_ref);
		return rc;
	}
	ext4_bcache_invalidate_lba(fs->bdev->bc, baddr, 1);
	/* Release block group reference */
	rc = ext4_fs_put_block_group_ref(&bg_ref);

	return rc;
}

int ext4_balloc_free_blocks(struct ext4_inode_ref *inode_ref,
			    ext4_fsblk_t first, uint32_t count)
{
	int rc = EOK;
	uint32_t blk_cnt = count;
	ext4_fsblk_t start_block = first;
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_sblock *sb = &fs->sb;

	/* Compute indexes */
	uint32_t bg_first = ext4_balloc_get_bgid_of_block(sb, first);

	/* Compute indexes */
	uint32_t bg_last = ext4_balloc_get_bgid_of_block(sb, first + count - 1);

	if (!ext4_sb_feature_incom(sb, EXT4_FINCOM_FLEX_BG)) {
		/*It is not possible without flex_bg that blocks are continuous
		 * and and last block belongs to other bg.*/
		if (bg_last != bg_first) {
			ext4_dbg(DEBUG_BALLOC, DBG_WARN "FLEX_BG: disabled & "
				"bg_last: %"PRIu32" bg_first: %"PRIu32"\n",
				bg_last, bg_first);
		}
	}

	/* Load block group reference */
	struct ext4_block_group_ref bg_ref;
	while (bg_first <= bg_last) {

		rc = ext4_fs_get_block_group_ref(fs, bg_first, &bg_ref);
		if (rc != EOK)
			return rc;

		struct ext4_bgroup *bg = bg_ref.block_group;

		uint32_t idx_in_bg_first;
		idx_in_bg_first = ext4_fs_addr_to_idx_bg(sb, first);

		/* Load block with bitmap */
		ext4_fsblk_t bitmap_blk = ext4_bg_get_block_bitmap(bg, sb);

		struct ext4_block blk;
		rc = ext4_trans_block_get(fs->bdev, &blk, bitmap_blk);
		if (rc != EOK) {
			ext4_fs_put_block_group_ref(&bg_ref);
			return rc;
		}

		if (!ext4_balloc_verify_bitmap_csum(sb, bg, blk.data)) {
			ext4_dbg(DEBUG_BALLOC,
				DBG_WARN "Bitmap checksum failed."
				"Group: %" PRIu32"\n",
				bg_ref.index);
		}
		uint32_t free_cnt;
		free_cnt = ext4_sb_get_block_size(sb) * 8 - idx_in_bg_first;

		/*If last block, free only count blocks*/
		free_cnt = count > free_cnt ? free_cnt : count;

		/* Modify bitmap */
		ext4_bmap_bits_free(blk.data, idx_in_bg_first, free_cnt);
		ext4_balloc_set_bitmap_csum(sb, bg, blk.data);
		ext4_trans_set_block_dirty(blk.buf);

		count -= free_cnt;
		first += free_cnt;

		/* Release block with bitmap */
		rc = ext4_block_set(fs->bdev, &blk);
		if (rc != EOK) {
			ext4_fs_put_block_group_ref(&bg_ref);
			return rc;
		}

		uint32_t block_size = ext4_sb_get_block_size(sb);

		/* Update superblock free blocks count */
		uint64_t sb_free_blocks = ext4_sb_get_free_blocks_cnt(sb);
		sb_free_blocks += free_cnt;
		ext4_sb_set_free_blocks_cnt(sb, sb_free_blocks);

		/* Update inode blocks count */
		uint64_t ino_blocks;
		ino_blocks = ext4_inode_get_blocks_count(sb, inode_ref->inode);
		ino_blocks -= free_cnt * (block_size / EXT4_INODE_BLOCK_SIZE);
		ext4_inode_set_blocks_count(sb, inode_ref->inode, ino_blocks);
		inode_ref->dirty = true;

		/* Update block group free blocks count */
		uint32_t free_blocks;
		free_blocks = ext4_bg_get_free_blocks_count(bg, sb);
		free_blocks += free_cnt;
		ext4_bg_set_free_blocks_count(bg, sb, free_blocks);
		bg_ref.dirty = true;

		/* Release block group reference */
		rc = ext4_fs_put_block_group_ref(&bg_ref);
		if (rc != EOK)
			break;

		bg_first++;
	}

	uint32_t i;
	for (i = 0;i < blk_cnt;i++) {
		rc = ext4_trans_try_revoke_block(fs->bdev, start_block + i);
		if (rc != EOK)
			return rc;

	}

	ext4_bcache_invalidate_lba(fs->bdev->bc, start_block, blk_cnt);
	/*All blocks should be released*/
	ext4_assert(count == 0);

	return rc;
}

int ext4_balloc_alloc_block(struct ext4_inode_ref *inode_ref,
			    ext4_fsblk_t goal,
			    ext4_fsblk_t *fblock)
{
	ext4_fsblk_t alloc = 0;
	ext4_fsblk_t bmp_blk_adr;
	uint32_t rel_blk_idx = 0;
	uint64_t free_blocks;
	int r;
	struct ext4_sblock *sb = &inode_ref->fs->sb;

	/* Load block group number for goal and relative index */
	uint32_t bg_id = ext4_balloc_get_bgid_of_block(sb, goal);
	uint32_t idx_in_bg = ext4_fs_addr_to_idx_bg(sb, goal);

	struct ext4_block b;
	struct ext4_block_group_ref bg_ref;

	/* Load block group reference */
	r = ext4_fs_get_block_group_ref(inode_ref->fs, bg_id, &bg_ref);
	if (r != EOK)
		return r;

	struct ext4_bgroup *bg = bg_ref.block_group;

	free_blocks = ext4_bg_get_free_blocks_count(bg_ref.block_group, sb);
	if (free_blocks == 0) {
		/* This group has no free blocks */
		goto goal_failed;
	}

	/* Compute indexes */
	ext4_fsblk_t first_in_bg;
	first_in_bg = ext4_balloc_get_block_of_bgid(sb, bg_ref.index);

	uint32_t first_in_bg_index;
	first_in_bg_index = ext4_fs_addr_to_idx_bg(sb, first_in_bg);

	if (idx_in_bg < first_in_bg_index)
		idx_in_bg = first_in_bg_index;

	/* Load block with bitmap */
	bmp_blk_adr = ext4_bg_get_block_bitmap(bg_ref.block_group, sb);

	r = ext4_trans_block_get(inode_ref->fs->bdev, &b, bmp_blk_adr);
	if (r != EOK) {
		ext4_fs_put_block_group_ref(&bg_ref);
		return r;
	}

	if (!ext4_balloc_verify_bitmap_csum(sb, bg, b.data)) {
		ext4_dbg(DEBUG_BALLOC,
			DBG_WARN "Bitmap checksum failed."
			"Group: %" PRIu32"\n",
			bg_ref.index);
	}

	/* Check if goal is free */
	if (ext4_bmap_is_bit_clr(b.data, idx_in_bg)) {
		ext4_bmap_bit_set(b.data, idx_in_bg);
		ext4_balloc_set_bitmap_csum(sb, bg_ref.block_group,
					    b.data);
		ext4_trans_set_block_dirty(b.buf);
		r = ext4_block_set(inode_ref->fs->bdev, &b);
		if (r != EOK) {
			ext4_fs_put_block_group_ref(&bg_ref);
			return r;
		}

		alloc = ext4_fs_bg_idx_to_addr(sb, idx_in_bg, bg_id);
		goto success;
	}

	uint32_t blk_in_bg = ext4_blocks_in_group_cnt(sb, bg_id);

	uint32_t end_idx = (idx_in_bg + 63) & ~63;
	if (end_idx > blk_in_bg)
		end_idx = blk_in_bg;

	/* Try to find free block near to goal */
	uint32_t tmp_idx;
	for (tmp_idx = idx_in_bg + 1; tmp_idx < end_idx; ++tmp_idx) {
		if (ext4_bmap_is_bit_clr(b.data, tmp_idx)) {
			ext4_bmap_bit_set(b.data, tmp_idx);

			ext4_balloc_set_bitmap_csum(sb, bg, b.data);
			ext4_trans_set_block_dirty(b.buf);
			r = ext4_block_set(inode_ref->fs->bdev, &b);
			if (r != EOK) {
				ext4_fs_put_block_group_ref(&bg_ref);
				return r;
			}

			alloc = ext4_fs_bg_idx_to_addr(sb, tmp_idx, bg_id);
			goto success;
		}
	}

	/* Find free bit in bitmap */
	r = ext4_bmap_bit_find_clr(b.data, idx_in_bg, blk_in_bg, &rel_blk_idx);
	if (r == EOK) {
		ext4_bmap_bit_set(b.data, rel_blk_idx);
		ext4_balloc_set_bitmap_csum(sb, bg_ref.block_group, b.data);
		ext4_trans_set_block_dirty(b.buf);
		r = ext4_block_set(inode_ref->fs->bdev, &b);
		if (r != EOK) {
			ext4_fs_put_block_group_ref(&bg_ref);
			return r;
		}

		alloc = ext4_fs_bg_idx_to_addr(sb, rel_blk_idx, bg_id);
		goto success;
	}

	/* No free block found yet */
	r = ext4_block_set(inode_ref->fs->bdev, &b);
	if (r != EOK) {
		ext4_fs_put_block_group_ref(&bg_ref);
		return r;
	}

goal_failed:

	r = ext4_fs_put_block_group_ref(&bg_ref);
	if (r != EOK)
		return r;

	/* Try other block groups */
	uint32_t block_group_count = ext4_block_group_cnt(sb);
	uint32_t bgid = (bg_id + 1) % block_group_count;
	uint32_t count = block_group_count;

	while (count > 0) {
		r = ext4_fs_get_block_group_ref(inode_ref->fs, bgid, &bg_ref);
		if (r != EOK)
			return r;

		struct ext4_bgroup *bg = bg_ref.block_group;
		free_blocks = ext4_bg_get_free_blocks_count(bg, sb);
		if (free_blocks == 0) {
			/* This group has no free blocks */
			goto next_group;
		}

		/* Load block with bitmap */
		bmp_blk_adr = ext4_bg_get_block_bitmap(bg, sb);
		r = ext4_trans_block_get(inode_ref->fs->bdev, &b, bmp_blk_adr);
		if (r != EOK) {
			ext4_fs_put_block_group_ref(&bg_ref);
			return r;
		}

		if (!ext4_balloc_verify_bitmap_csum(sb, bg, b.data)) {
			ext4_dbg(DEBUG_BALLOC,
				DBG_WARN "Bitmap checksum failed."
				"Group: %" PRIu32"\n",
				bg_ref.index);
		}

		/* Compute indexes */
		first_in_bg = ext4_balloc_get_block_of_bgid(sb, bgid);
		idx_in_bg = ext4_fs_addr_to_idx_bg(sb, first_in_bg);
		blk_in_bg = ext4_blocks_in_group_cnt(sb, bgid);
		first_in_bg_index = ext4_fs_addr_to_idx_bg(sb, first_in_bg);

		if (idx_in_bg < first_in_bg_index)
			idx_in_bg = first_in_bg_index;

		r = ext4_bmap_bit_find_clr(b.data, idx_in_bg, blk_in_bg,
				&rel_blk_idx);
		if (r == EOK) {
			ext4_bmap_bit_set(b.data, rel_blk_idx);
			ext4_balloc_set_bitmap_csum(sb, bg, b.data);
			ext4_trans_set_block_dirty(b.buf);
			r = ext4_block_set(inode_ref->fs->bdev, &b);
			if (r != EOK) {
				ext4_fs_put_block_group_ref(&bg_ref);
				return r;
			}

			alloc = ext4_fs_bg_idx_to_addr(sb, rel_blk_idx, bgid);
			goto success;
		}

		r = ext4_block_set(inode_ref->fs->bdev, &b);
		if (r != EOK) {
			ext4_fs_put_block_group_ref(&bg_ref);
			return r;
		}

	next_group:
		r = ext4_fs_put_block_group_ref(&bg_ref);
		if (r != EOK) {
			return r;
		}

		/* Goto next group */
		bgid = (bgid + 1) % block_group_count;
		count--;
	}

	return ENOSPC;

success:
    /* Empty command - because of syntax */
    ;

	uint32_t block_size = ext4_sb_get_block_size(sb);

	/* Update superblock free blocks count */
	uint64_t sb_free_blocks = ext4_sb_get_free_blocks_cnt(sb);
	sb_free_blocks--;
	ext4_sb_set_free_blocks_cnt(sb, sb_free_blocks);

	/* Update inode blocks (different block size!) count */
	uint64_t ino_blocks = ext4_inode_get_blocks_count(sb, inode_ref->inode);
	ino_blocks += block_size / EXT4_INODE_BLOCK_SIZE;
	ext4_inode_set_blocks_count(sb, inode_ref->inode, ino_blocks);
	inode_ref->dirty = true;

	/* Update block group free blocks count */

	uint32_t fb_cnt = ext4_bg_get_free_blocks_count(bg_ref.block_group, sb);
	fb_cnt--;
	ext4_bg_set_free_blocks_count(bg_ref.block_group, sb, fb_cnt);

	bg_ref.dirty = true;
	r = ext4_fs_put_block_group_ref(&bg_ref);

	*fblock = alloc;
	return r;
}

int ext4_balloc_try_alloc_block(struct ext4_inode_ref *inode_ref,
				ext4_fsblk_t baddr, bool *free)
{
	int rc;

	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_sblock *sb = &fs->sb;

	/* Compute indexes */
	uint32_t block_group = ext4_balloc_get_bgid_of_block(sb, baddr);
	uint32_t index_in_group = ext4_fs_addr_to_idx_bg(sb, baddr);

	/* Load block group reference */
	struct ext4_block_group_ref bg_ref;
	rc = ext4_fs_get_block_group_ref(fs, block_group, &bg_ref);
	if (rc != EOK)
		return rc;

	/* Load block with bitmap */
	ext4_fsblk_t bmp_blk_addr;
	bmp_blk_addr = ext4_bg_get_block_bitmap(bg_ref.block_group, sb);

	struct ext4_block b;
	rc = ext4_trans_block_get(fs->bdev, &b, bmp_blk_addr);
	if (rc != EOK) {
		ext4_fs_put_block_group_ref(&bg_ref);
		return rc;
	}

	if (!ext4_balloc_verify_bitmap_csum(sb, bg_ref.block_group, b.data)) {
		ext4_dbg(DEBUG_BALLOC,
			DBG_WARN "Bitmap checksum failed."
			"Group: %" PRIu32"\n",
			bg_ref.index);
	}

	/* Check if block is free */
	*free = ext4_bmap_is_bit_clr(b.data, index_in_group);

	/* Allocate block if possible */
	if (*free) {
		ext4_bmap_bit_set(b.data, index_in_group);
		ext4_balloc_set_bitmap_csum(sb, bg_ref.block_group, b.data);
		ext4_trans_set_block_dirty(b.buf);
	}

	/* Release block with bitmap */
	rc = ext4_block_set(fs->bdev, &b);
	if (rc != EOK) {
		/* Error in saving bitmap */
		ext4_fs_put_block_group_ref(&bg_ref);
		return rc;
	}

	/* If block is not free, return */
	if (!(*free))
		goto terminate;

	uint32_t block_size = ext4_sb_get_block_size(sb);

	/* Update superblock free blocks count */
	uint64_t sb_free_blocks = ext4_sb_get_free_blocks_cnt(sb);
	sb_free_blocks--;
	ext4_sb_set_free_blocks_cnt(sb, sb_free_blocks);

	/* Update inode blocks count */
	uint64_t ino_blocks = ext4_inode_get_blocks_count(sb, inode_ref->inode);
	ino_blocks += block_size / EXT4_INODE_BLOCK_SIZE;
	ext4_inode_set_blocks_count(sb, inode_ref->inode, ino_blocks);
	inode_ref->dirty = true;

	/* Update block group free blocks count */
	uint32_t fb_cnt = ext4_bg_get_free_blocks_count(bg_ref.block_group, sb);
	fb_cnt--;
	ext4_bg_set_free_blocks_count(bg_ref.block_group, sb, fb_cnt);

	bg_ref.dirty = true;

terminate:
	return ext4_fs_put_block_group_ref(&bg_ref);
}

/**
 * @}
 */
