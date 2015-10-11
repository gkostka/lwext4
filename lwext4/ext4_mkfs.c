/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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
 * @file  ext4_mkfs.c
 * @brief
 */

#include "ext4_config.h"
#include "ext4_super.h"
#include "ext4_debug.h"
#include "ext4_mkfs.h"

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#define DIV_ROUND_UP(x, y) (((x) + (y) - 1)/(y))
#define EXT4_ALIGN(x, y) ((y) * DIV_ROUND_UP((x), (y)))

static int sb2info(struct ext4_sblock *sb, struct ext4_mkfs_info *info)
{
        if (to_le16(sb->magic) != EXT4_SUPERBLOCK_MAGIC)
                return EINVAL;

	info->block_size = 1024 << to_le32(sb->log_block_size);
	info->blocks_per_group = to_le32(sb->blocks_per_group);
	info->inodes_per_group = to_le32(sb->inodes_per_group);
	info->inode_size = to_le16(sb->inode_size);
	info->inodes = to_le32(sb->inodes_count);
	info->feat_ro_compat = to_le32(sb->features_read_only);
	info->feat_compat = to_le32(sb->features_compatible);
	info->feat_incompat = to_le32(sb->features_incompatible);
	info->bg_desc_reserve_blocks = to_le16(sb->s_reserved_gdt_blocks);
	info->label = sb->volume_name;
	info->len = (uint64_t)info->block_size * ext4_sb_get_blocks_cnt(sb);

	return EOK;
}

static int info2sb(struct ext4_mkfs_info *info, struct ext4_sblock *sb)
{
	(void)info;
	memset(sb, 0, sizeof(struct ext4_sblock));
	/*TODO: Fill superblock with proper values*/

	return EOK;
}

static uint32_t compute_blocks_per_group(struct ext4_mkfs_info *info)
{
	return info->block_size * 8;
}

static uint32_t compute_inodes(struct ext4_mkfs_info *info)
{
	return DIV_ROUND_UP(info->len, info->block_size) / 4;
}

static uint32_t compute_inodes_per_group(struct ext4_mkfs_info *info)
{
	uint32_t blocks = DIV_ROUND_UP(info->len, info->block_size);
	uint32_t block_groups = DIV_ROUND_UP(blocks, info->blocks_per_group);
	uint32_t inodes = DIV_ROUND_UP(info->inodes, block_groups);
	inodes = EXT4_ALIGN(inodes, (info->block_size / info->inode_size));

	/* After properly rounding up the number of inodes/group,
	 * make sure to update the total inodes field in the info struct.
	 */
	info->inodes = inodes * block_groups;

	return inodes;
}

static uint32_t compute_bg_desc_reserve_blocks(struct ext4_mkfs_info *info)
{
	uint32_t blocks = DIV_ROUND_UP(info->len, info->block_size);
	uint32_t block_groups = DIV_ROUND_UP(blocks, info->blocks_per_group);
	uint32_t bg_desc_blocks = DIV_ROUND_UP(block_groups * sizeof(struct ext4_bgroup),
			info->block_size);

	uint32_t bg_desc_reserve_blocks =
			DIV_ROUND_UP(block_groups * 1024 * sizeof(struct ext4_bgroup),
					info->block_size) - bg_desc_blocks;

	if (bg_desc_reserve_blocks > info->block_size / sizeof(uint32_t))
		bg_desc_reserve_blocks = info->block_size / sizeof(uint32_t);

	return bg_desc_reserve_blocks;
}

static uint32_t compute_journal_blocks(struct ext4_mkfs_info *info)
{
	uint32_t journal_blocks = DIV_ROUND_UP(info->len, info->block_size) / 64;
	if (journal_blocks < 1024)
		journal_blocks = 1024;
	if (journal_blocks > 32768)
		journal_blocks = 32768;
	return journal_blocks;
}



int ext4_mkfs_read_info(struct ext4_blockdev *bd, struct ext4_mkfs_info *info)
{
	int r;
	struct ext4_sblock *sb = NULL;
	r = ext4_block_init(bd);
	if (r != EOK)
		return r;

	sb = malloc(sizeof(struct ext4_sblock));
	if (!sb)
		goto Finish;


	r = ext4_sb_read(bd, sb);
	if (r != EOK)
		goto Finish;

	r = sb2info(sb, info);

Finish:
	if (sb)
		free(sb);
	ext4_block_fini(bd);
	return r;
}

int ext4_mkfs(struct ext4_blockdev *bd, struct ext4_mkfs_info *info)
{
	int r;
	struct ext4_sblock *sb = NULL;
	r = ext4_block_init(bd);
	if (r != EOK)
		return r;

	sb = malloc(sizeof(struct ext4_sblock));
	if (!sb)
		goto Finish;

	if (info->len == 0)
		info->len = bd->ph_bcnt * bd->ph_bsize;

	if (info->block_size == 0)
		info->block_size = 4096; /*Set block size to default value*/

	/* Round down the filesystem length to be a multiple of the block size */
	info->len &= ~((uint64_t)info->block_size - 1);

	if (info->journal_blocks == 0)
		info->journal_blocks = compute_journal_blocks(info);

	if (info->blocks_per_group == 0)
		info->blocks_per_group = compute_blocks_per_group(info);

	if (info->inodes == 0)
		info->inodes = compute_inodes(info);

	if (info->inode_size == 0)
		info->inode_size = 256;

	if (info->label == NULL)
		info->label = "";

	info->inodes_per_group = compute_inodes_per_group(info);

	info->feat_compat = CONFIG_FEATURE_COMPAT_SUPP;
	info->feat_ro_compat = CONFIG_FEATURE_RO_COMPAT_SUPP;
	info->feat_incompat = CONFIG_FEATURE_INCOMPAT_SUPP;

	if (info->no_journal == 0)
		info->feat_compat |= EXT4_FEATURE_COMPAT_HAS_JOURNAL;

	info->bg_desc_reserve_blocks = compute_bg_desc_reserve_blocks(info);

	r = info2sb(info, sb);
	if (r != EOK)
		return r;

	ext4_dbg(DEBUG_MKFS, DBG_INFO "Creating filesystem with parameters:\n");
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Size: %"PRIu64"\n", info->len);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Block size: %d\n", info->block_size);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Blocks per group: %d\n",
			info->blocks_per_group);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Inodes per group: %d\n",
			info->inodes_per_group);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Inode size: %d\n", info->inode_size);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Inodes: %d\n", info->inodes);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Journal blocks: %d\n",
			info->journal_blocks);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Features ro_compat: 0x%x\n",
			info->feat_ro_compat);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Features compat: 0x%x\n",
			info->feat_compat);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Features incompat: 0x%x\n",
			info->feat_incompat);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "BG desc reserve: 0x%x\n",
			info->bg_desc_reserve_blocks);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "journal: %s\n",
			!info->no_journal ? "yes" : "no");
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Label: %s\n", info->label);

	/*TODO: write filesystem metadata*/

	Finish:
	if (sb)
		free(sb);
	ext4_block_fini(bd);
	return r;
}

/**
 * @}
 */
