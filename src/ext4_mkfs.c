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

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_debug.h>

#include <ext4_super.h>
#include <ext4_block_group.h>
#include <ext4_dir.h>
#include <ext4_dir_idx.h>
#include <ext4_fs.h>
#include <ext4_inode.h>
#include <ext4_ialloc.h>
#include <ext4_mkfs.h>

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

struct fs_aux_info {
	struct ext4_sblock *sb;
	uint8_t *bg_desc_blk;
	struct xattr_list_element *xattrs;
	uint32_t first_data_block;
	uint64_t len_blocks;
	uint32_t inode_table_blocks;
	uint32_t groups;
	uint32_t bg_desc_blocks;
	uint32_t default_i_flags;
	uint32_t blocks_per_ind;
	uint32_t blocks_per_dind;
	uint32_t blocks_per_tind;
};

static inline int log_2(int j)
{
	int i;

	for (i = 0; j > 0; i++)
		j >>= 1;

	return i - 1;
}

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
	info->dsc_size = to_le16(sb->desc_size);
	memcpy(info->uuid, sb->uuid, UUID_SIZE);

	return EOK;
}

static uint32_t compute_blocks_per_group(struct ext4_mkfs_info *info)
{
	return info->block_size * 8;
}

static uint32_t compute_inodes(struct ext4_mkfs_info *info)
{
	return (uint32_t)EXT4_DIV_ROUND_UP(info->len, info->block_size) / 4;
}

static uint32_t compute_inodes_per_group(struct ext4_mkfs_info *info)
{
	uint32_t blocks = (uint32_t)EXT4_DIV_ROUND_UP(info->len, info->block_size);
	uint32_t block_groups = EXT4_DIV_ROUND_UP(blocks, info->blocks_per_group);
	uint32_t inodes = EXT4_DIV_ROUND_UP(info->inodes, block_groups);
	inodes = EXT4_ALIGN(inodes, (info->block_size / info->inode_size));

	/* After properly rounding up the number of inodes/group,
	 * make sure to update the total inodes field in the info struct.
	 */
	info->inodes = inodes * block_groups;

	return inodes;
}


static uint32_t compute_journal_blocks(struct ext4_mkfs_info *info)
{
	uint32_t journal_blocks = (uint32_t)EXT4_DIV_ROUND_UP(info->len,
						 info->block_size) / 64;
	if (journal_blocks < 1024)
		journal_blocks = 1024;
	if (journal_blocks > 32768)
		journal_blocks = 32768;
	return journal_blocks;
}

static bool has_superblock(struct ext4_mkfs_info *info, uint32_t bgid)
{
	if (!(info->feat_ro_compat & EXT4_FRO_COM_SPARSE_SUPER))
		return true;

	return ext4_sb_sparse(bgid);
}

static int create_fs_aux_info(struct fs_aux_info *aux_info,
			      struct ext4_mkfs_info *info)
{
	aux_info->first_data_block = (info->block_size > 1024) ? 0 : 1;
	aux_info->len_blocks = info->len / info->block_size;
	aux_info->inode_table_blocks = EXT4_DIV_ROUND_UP(info->inodes_per_group *
			info->inode_size, info->block_size);
	aux_info->groups = (uint32_t)EXT4_DIV_ROUND_UP(aux_info->len_blocks -
			aux_info->first_data_block, info->blocks_per_group);
	aux_info->blocks_per_ind = info->block_size / sizeof(uint32_t);
	aux_info->blocks_per_dind =
			aux_info->blocks_per_ind * aux_info->blocks_per_ind;
	aux_info->blocks_per_tind =
			aux_info->blocks_per_dind * aux_info->blocks_per_dind;

	aux_info->bg_desc_blocks =
		EXT4_DIV_ROUND_UP(aux_info->groups * info->dsc_size,
			info->block_size);

	aux_info->default_i_flags = EXT4_INODE_FLAG_NOATIME;

	uint32_t last_group_size = aux_info->len_blocks % info->blocks_per_group;
	uint32_t last_header_size = 2 + aux_info->inode_table_blocks;
	if (has_superblock(info, aux_info->groups - 1))
		last_header_size += 1 + aux_info->bg_desc_blocks +
			info->bg_desc_reserve_blocks;

	if (last_group_size > 0 && last_group_size < last_header_size) {
		aux_info->groups--;
		aux_info->len_blocks -= last_group_size;
	}

	aux_info->sb = ext4_calloc(1, EXT4_SUPERBLOCK_SIZE);
	if (!aux_info->sb)
		return ENOMEM;

	aux_info->bg_desc_blk = ext4_calloc(1, info->block_size);
	if (!aux_info->bg_desc_blk)
		return ENOMEM;

	aux_info->xattrs = NULL;


	ext4_dbg(DEBUG_MKFS, DBG_INFO "create_fs_aux_info\n");
	ext4_dbg(DEBUG_MKFS, DBG_NONE "first_data_block: %"PRIu32"\n",
			aux_info->first_data_block);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "len_blocks: %"PRIu64"\n",
			aux_info->len_blocks);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "inode_table_blocks: %"PRIu32"\n",
			aux_info->inode_table_blocks);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "groups: %"PRIu32"\n",
			aux_info->groups);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "bg_desc_blocks: %"PRIu32"\n",
			aux_info->bg_desc_blocks);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "default_i_flags: %"PRIu32"\n",
			aux_info->default_i_flags);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "blocks_per_ind: %"PRIu32"\n",
			aux_info->blocks_per_ind);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "blocks_per_dind: %"PRIu32"\n",
			aux_info->blocks_per_dind);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "blocks_per_tind: %"PRIu32"\n",
			aux_info->blocks_per_tind);

	return EOK;
}

static void release_fs_aux_info(struct fs_aux_info *aux_info)
{
	if (aux_info->sb)
		ext4_free(aux_info->sb);
	if (aux_info->bg_desc_blk)
		ext4_free(aux_info->bg_desc_blk);
}


/* Fill in the superblock memory buffer based on the filesystem parameters */
static void fill_sb(struct fs_aux_info *aux_info, struct ext4_mkfs_info *info)
{
	struct ext4_sblock *sb = aux_info->sb;

	sb->inodes_count = to_le32(info->inodes_per_group * aux_info->groups);

	ext4_sb_set_blocks_cnt(sb, aux_info->len_blocks);
	ext4_sb_set_free_blocks_cnt(sb, aux_info->len_blocks);
	sb->free_inodes_count = to_le32(info->inodes_per_group * aux_info->groups);

	sb->reserved_blocks_count_lo = to_le32(0);
	sb->first_data_block = to_le32(aux_info->first_data_block);
	sb->log_block_size = to_le32(log_2(info->block_size / 1024));
	sb->log_cluster_size = to_le32(log_2(info->block_size / 1024));
	sb->blocks_per_group = to_le32(info->blocks_per_group);
	sb->frags_per_group = to_le32(info->blocks_per_group);
	sb->inodes_per_group = to_le32(info->inodes_per_group);
	sb->mount_time = to_le32(0);
	sb->write_time = to_le32(0);
	sb->mount_count = to_le16(0);
	sb->max_mount_count = to_le16(0xFFFF);
	sb->magic = to_le16(EXT4_SUPERBLOCK_MAGIC);
	sb->state = to_le16(EXT4_SUPERBLOCK_STATE_VALID_FS);
	sb->errors = to_le16(EXT4_SUPERBLOCK_ERRORS_RO);
	sb->minor_rev_level = to_le16(0);
	sb->last_check_time = to_le32(0);
	sb->check_interval = to_le32(0);
	sb->creator_os = to_le32(EXT4_SUPERBLOCK_OS_LINUX);
	sb->rev_level = to_le32(1);
	sb->def_resuid = to_le16(0);
	sb->def_resgid = to_le16(0);

	sb->first_inode = to_le32(EXT4_GOOD_OLD_FIRST_INO);
	sb->inode_size = to_le16(info->inode_size);
	sb->block_group_index = to_le16(0);

	sb->features_compatible = to_le32(info->feat_compat);
	sb->features_incompatible = to_le32(info->feat_incompat);
	sb->features_read_only = to_le32(info->feat_ro_compat);

	memcpy(sb->uuid, info->uuid, UUID_SIZE);

	memset(sb->volume_name, 0, sizeof(sb->volume_name));
	strncpy(sb->volume_name, info->label, sizeof(sb->volume_name));
	memset(sb->last_mounted, 0, sizeof(sb->last_mounted));

	sb->algorithm_usage_bitmap = to_le32(0);
	sb->s_prealloc_blocks = 0;
	sb->s_prealloc_dir_blocks = 0;
	sb->s_reserved_gdt_blocks = to_le16(info->bg_desc_reserve_blocks);

	if (info->feat_compat & EXT4_FCOM_HAS_JOURNAL)
		sb->journal_inode_number = to_le32(EXT4_JOURNAL_INO);

	sb->journal_backup_type = 1;
	sb->journal_dev = to_le32(0);
	sb->last_orphan = to_le32(0);
	sb->hash_seed[0] = to_le32(0x11111111);
	sb->hash_seed[1] = to_le32(0x22222222);
	sb->hash_seed[2] = to_le32(0x33333333);
	sb->hash_seed[3] = to_le32(0x44444444);
	sb->default_hash_version = EXT2_HTREE_HALF_MD4;
	sb->checksum_type = 1;
	sb->desc_size = to_le16(info->dsc_size);
	sb->default_mount_opts = to_le32(0);
	sb->first_meta_bg = to_le32(0);
	sb->mkfs_time = to_le32(0);

	sb->reserved_blocks_count_hi = to_le32(0);
	sb->min_extra_isize = to_le32(sizeof(struct ext4_inode) -
		EXT4_GOOD_OLD_INODE_SIZE);
	sb->want_extra_isize = to_le32(sizeof(struct ext4_inode) -
		EXT4_GOOD_OLD_INODE_SIZE);
	sb->flags = to_le32(EXT4_SUPERBLOCK_FLAGS_SIGNED_HASH);
}


static int write_bgroup_block(struct ext4_blockdev *bd,
			      struct fs_aux_info *aux_info,
			      struct ext4_mkfs_info *info,
			      uint32_t blk)
{
	int r = EOK;
	uint32_t j;
	struct ext4_block b;

	uint32_t block_size = ext4_sb_get_block_size(aux_info->sb);

	for (j = 0; j < aux_info->groups; j++) {
		uint64_t bg_start_block = aux_info->first_data_block +
					  j * info->blocks_per_group;
		uint32_t blk_off = 0;

		blk_off += aux_info->bg_desc_blocks;
		if (has_superblock(info, j)) {
			bg_start_block++;
			blk_off += info->bg_desc_reserve_blocks;
		}

		uint64_t dsc_blk = bg_start_block + blk;

		r = ext4_block_get_noread(bd, &b, dsc_blk);
		if (r != EOK)
			return r;

		memcpy(b.data, aux_info->bg_desc_blk, block_size);

		ext4_bcache_set_dirty(b.buf);
		r = ext4_block_set(bd, &b);
		if (r != EOK)
			return r;
	}

	return r;
}

static int write_bgroups(struct ext4_blockdev *bd, struct fs_aux_info *aux_info,
			 struct ext4_mkfs_info *info)
{
	int r = EOK;

	struct ext4_block b;
	struct ext4_bgroup *bg_desc;

	uint32_t i;
	uint32_t bg_free_blk = 0;
	uint64_t sb_free_blk = 0;
	uint32_t block_size = ext4_sb_get_block_size(aux_info->sb);
	uint32_t dsc_size = ext4_sb_get_desc_size(aux_info->sb);
	uint32_t dsc_per_block = block_size / dsc_size;
	uint32_t k = 0;

	for (i = 0; i < aux_info->groups; i++) {
		uint64_t bg_start_block = aux_info->first_data_block +
			aux_info->first_data_block + i * info->blocks_per_group;
		uint32_t blk_off = 0;

		bg_desc = (void *)(aux_info->bg_desc_blk + k * dsc_size);
		bg_free_blk = info->blocks_per_group -
				aux_info->inode_table_blocks;

		bg_free_blk -= 2;
		blk_off += aux_info->bg_desc_blocks;

		if (i == (aux_info->groups - 1))
			bg_free_blk -= aux_info->first_data_block;

		if (has_superblock(info, i)) {
			bg_start_block++;
			blk_off += info->bg_desc_reserve_blocks;
			bg_free_blk -= info->bg_desc_reserve_blocks + 1;
			bg_free_blk -= aux_info->bg_desc_blocks;
		}

		ext4_bg_set_block_bitmap(bg_desc, aux_info->sb,
					 bg_start_block + blk_off + 1);

		ext4_bg_set_inode_bitmap(bg_desc, aux_info->sb,
					 bg_start_block + blk_off + 2);

		ext4_bg_set_inode_table_first_block(bg_desc,
						aux_info->sb,
						bg_start_block + blk_off + 3);

		ext4_bg_set_free_blocks_count(bg_desc, aux_info->sb,
					      bg_free_blk);

		ext4_bg_set_free_inodes_count(bg_desc,
				aux_info->sb, to_le32(aux_info->sb->inodes_per_group));

		ext4_bg_set_used_dirs_count(bg_desc, aux_info->sb, 0);

		ext4_bg_set_flag(bg_desc,
				 EXT4_BLOCK_GROUP_BLOCK_UNINIT |
				 EXT4_BLOCK_GROUP_INODE_UNINIT);

		sb_free_blk += bg_free_blk;

		r = ext4_block_get_noread(bd, &b, bg_start_block + blk_off + 1);
		if (r != EOK)
			return r;
		memset(b.data, 0, block_size);
		ext4_bcache_set_dirty(b.buf);
		r = ext4_block_set(bd, &b);
		if (r != EOK)
			return r;
		r = ext4_block_get_noread(bd, &b, bg_start_block + blk_off + 2);
		if (r != EOK)
			return r;
		memset(b.data, 0, block_size);
		ext4_bcache_set_dirty(b.buf);
		r = ext4_block_set(bd, &b);
		if (r != EOK)
			return r;

		if (++k != dsc_per_block)
			continue;

		k = 0;
		r = write_bgroup_block(bd, aux_info, info, i / dsc_per_block);
		if (r != EOK)
			return r;

	}

	r = write_bgroup_block(bd, aux_info, info, i / dsc_per_block);
	if (r != EOK)
		return r;

	ext4_sb_set_free_blocks_cnt(aux_info->sb, sb_free_blk);
	return r;
}

static int write_sblocks(struct ext4_blockdev *bd, struct fs_aux_info *aux_info,
			  struct ext4_mkfs_info *info)
{
	uint64_t offset;
	uint32_t i;
	int r;

	/* write out the backup superblocks */
	for (i = 1; i < aux_info->groups; i++) {
		if (has_superblock(info, i)) {
			offset = info->block_size * (aux_info->first_data_block
				+ i * info->blocks_per_group);

			aux_info->sb->block_group_index = to_le16(i);
			r = ext4_block_writebytes(bd, offset, aux_info->sb,
						  EXT4_SUPERBLOCK_SIZE);
			if (r != EOK)
				return r;
		}
	}

	/* write out the primary superblock */
	aux_info->sb->block_group_index = to_le16(0);
	return ext4_block_writebytes(bd, 1024, aux_info->sb,
			EXT4_SUPERBLOCK_SIZE);
}


int ext4_mkfs_read_info(struct ext4_blockdev *bd, struct ext4_mkfs_info *info)
{
	int r;
	struct ext4_sblock *sb = NULL;
	r = ext4_block_init(bd);
	if (r != EOK)
		return r;

	sb = ext4_malloc(EXT4_SUPERBLOCK_SIZE);
	if (!sb)
		goto Finish;


	r = ext4_sb_read(bd, sb);
	if (r != EOK)
		goto Finish;

	r = sb2info(sb, info);

Finish:
	if (sb)
		ext4_free(sb);
	ext4_block_fini(bd);
	return r;
}

static int mkfs_init(struct ext4_blockdev *bd, struct ext4_mkfs_info *info)
{
	int r;
	struct fs_aux_info aux_info;
	memset(&aux_info, 0, sizeof(struct fs_aux_info));

	r = create_fs_aux_info(&aux_info, info);
	if (r != EOK)
		goto Finish;

	fill_sb(&aux_info, info);

	r = write_bgroups(bd, &aux_info, info);
	if (r != EOK)
		goto Finish;

	r = write_sblocks(bd, &aux_info, info);
	if (r != EOK)
		goto Finish;

	Finish:
	release_fs_aux_info(&aux_info);
	return r;
}

static int init_bgs(struct ext4_fs *fs)
{
	int r = EOK;
	struct ext4_block_group_ref ref;
	uint32_t i;
	uint32_t bg_count = ext4_block_group_cnt(&fs->sb);
	for (i = 0; i < bg_count; ++i) {
		r = ext4_fs_get_block_group_ref(fs, i, &ref);
		if (r != EOK)
			break;

		r = ext4_fs_put_block_group_ref(&ref);
		if (r != EOK)
			break;
	}
	return r;
}

static int alloc_inodes(struct ext4_fs *fs)
{
	int r = EOK;
	int i;
	struct ext4_inode_ref inode_ref;
	for (i = 1; i < 12; ++i) {
		int filetype = EXT4_DE_REG_FILE;

		switch (i) {
		case EXT4_ROOT_INO:
		case EXT4_GOOD_OLD_FIRST_INO:
			filetype = EXT4_DE_DIR;
			break;
		default:
			break;
		}

		r = ext4_fs_alloc_inode(fs, &inode_ref, filetype);
		if (r != EOK)
			return r;

		ext4_inode_set_mode(&fs->sb, inode_ref.inode, 0);

		switch (i) {
		case EXT4_ROOT_INO:
		case EXT4_JOURNAL_INO:
			ext4_fs_inode_blocks_init(fs, &inode_ref);
			break;
		}

		ext4_fs_put_inode_ref(&inode_ref);
	}

	return r;
}

static int create_dirs(struct ext4_fs *fs)
{
	int r = EOK;
	struct ext4_inode_ref root;
	struct ext4_inode_ref child;

	r = ext4_fs_get_inode_ref(fs, EXT4_ROOT_INO, &root);
	if (r != EOK)
		return r;

	r = ext4_fs_get_inode_ref(fs, EXT4_GOOD_OLD_FIRST_INO, &child);
	if (r != EOK)
		return r;

	ext4_inode_set_mode(&fs->sb, child.inode,
			EXT4_INODE_MODE_DIRECTORY | 0777);

	ext4_inode_set_mode(&fs->sb, root.inode,
			EXT4_INODE_MODE_DIRECTORY | 0777);

#if CONFIG_DIR_INDEX_ENABLE
	/* Initialize directory index if supported */
	if (ext4_sb_feature_com(&fs->sb, EXT4_FCOM_DIR_INDEX)) {
		r = ext4_dir_dx_init(&root, &root);
		if (r != EOK)
			return r;

		r = ext4_dir_dx_init(&child, &root);
		if (r != EOK)
			return r;

		ext4_inode_set_flag(root.inode,	EXT4_INODE_FLAG_INDEX);
		ext4_inode_set_flag(child.inode, EXT4_INODE_FLAG_INDEX);
	} else
#endif
	{
		r = ext4_dir_add_entry(&root, ".", strlen("."), &root);
		if (r != EOK)
			return r;

		r = ext4_dir_add_entry(&root, "..", strlen(".."), &root);
		if (r != EOK)
			return r;

		r = ext4_dir_add_entry(&child, ".", strlen("."), &child);
		if (r != EOK)
			return r;

		r = ext4_dir_add_entry(&child, "..", strlen(".."), &root);
		if (r != EOK)
			return r;
	}

	r = ext4_dir_add_entry(&root, "lost+found", strlen("lost+found"), &child);
	if (r != EOK)
		return r;

	ext4_inode_set_links_cnt(root.inode, 3);
	ext4_inode_set_links_cnt(child.inode, 2);

	child.dirty = true;
	root.dirty = true;
	ext4_fs_put_inode_ref(&child);
	ext4_fs_put_inode_ref(&root);
	return r;
}

static int create_journal_inode(struct ext4_fs *fs,
				struct ext4_mkfs_info *info)
{
	int ret;
	struct ext4_inode_ref inode_ref;
	uint64_t blocks_count;

	if (!info->journal)
		return EOK;

	ret = ext4_fs_get_inode_ref(fs, EXT4_JOURNAL_INO, &inode_ref);
	if (ret != EOK)
		return ret;

	struct ext4_inode *inode = inode_ref.inode;

	ext4_inode_set_mode(&fs->sb, inode, EXT4_INODE_MODE_FILE | 0600);
	ext4_inode_set_links_cnt(inode, 1);

	blocks_count = ext4_inode_get_blocks_count(&fs->sb, inode);

	while (blocks_count++ < info->journal_blocks)
	{
		ext4_fsblk_t fblock;
		ext4_lblk_t iblock;
		struct ext4_block blk;

		ret = ext4_fs_append_inode_dblk(&inode_ref, &fblock, &iblock);
		if (ret != EOK)
			goto Finish;

		if (iblock != 0)
			continue;

		ret = ext4_block_get(fs->bdev, &blk, fblock);
		if (ret != EOK)
			goto Finish;


		struct jbd_sb * jbd_sb = (struct jbd_sb * )blk.data;
		memset(jbd_sb, 0, sizeof(struct jbd_sb));

		jbd_sb->header.magic = to_be32(JBD_MAGIC_NUMBER);
		jbd_sb->header.blocktype = to_be32(JBD_SUPERBLOCK_V2);
		jbd_sb->blocksize = to_be32(info->block_size);
		jbd_sb->maxlen = to_be32(info->journal_blocks);
		jbd_sb->nr_users = to_be32(1);
		jbd_sb->first = to_be32(1);
		jbd_sb->sequence = to_be32(1);

		ext4_bcache_set_dirty(blk.buf);
		ret = ext4_block_set(fs->bdev, &blk);
		if (ret != EOK)
			goto Finish;
	}

	memcpy(fs->sb.journal_blocks, inode->blocks, sizeof(inode->blocks));

	Finish:
	ext4_fs_put_inode_ref(&inode_ref);

	return ret;
}

int ext4_mkfs(struct ext4_fs *fs, struct ext4_blockdev *bd,
	      struct ext4_mkfs_info *info, int fs_type)
{
	int r;

	r = ext4_block_init(bd);
	if (r != EOK)
		return r;

	bd->fs = fs;

	if (info->len == 0)
		info->len = bd->part_size;

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

	switch (fs_type) {
	case F_SET_EXT2:
		info->feat_compat = EXT2_SUPPORTED_FCOM;
		info->feat_ro_compat = EXT2_SUPPORTED_FRO_COM;
		info->feat_incompat = EXT2_SUPPORTED_FINCOM;
		break;
	case F_SET_EXT3:
		info->feat_compat = EXT3_SUPPORTED_FCOM;
		info->feat_ro_compat = EXT3_SUPPORTED_FRO_COM;
		info->feat_incompat = EXT3_SUPPORTED_FINCOM;
		break;
	case F_SET_EXT4:
		info->feat_compat = EXT4_SUPPORTED_FCOM;
		info->feat_ro_compat = EXT4_SUPPORTED_FRO_COM;
		info->feat_incompat = EXT4_SUPPORTED_FINCOM;
		break;
	}

	/*TODO: handle this features some day...*/
	info->feat_incompat &= ~EXT4_FINCOM_META_BG;
	info->feat_incompat &= ~EXT4_FINCOM_FLEX_BG;
	info->feat_incompat &= ~EXT4_FINCOM_64BIT;

	info->feat_ro_compat &= ~EXT4_FRO_COM_METADATA_CSUM;
	info->feat_ro_compat &= ~EXT4_FRO_COM_GDT_CSUM;
	info->feat_ro_compat &= ~EXT4_FRO_COM_DIR_NLINK;
	info->feat_ro_compat &= ~EXT4_FRO_COM_EXTRA_ISIZE;
	info->feat_ro_compat &= ~EXT4_FRO_COM_HUGE_FILE;

	if (info->journal)
		info->feat_compat |= EXT4_FCOM_HAS_JOURNAL;

	if (info->dsc_size == 0) {

		if (info->feat_incompat & EXT4_FINCOM_64BIT)
			info->dsc_size = EXT4_MAX_BLOCK_GROUP_DESCRIPTOR_SIZE;
		else
			info->dsc_size = EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE;
	}

	info->bg_desc_reserve_blocks = 0;

	ext4_dbg(DEBUG_MKFS, DBG_INFO "Creating filesystem with parameters:\n");
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Size: %"PRIu64"\n", info->len);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Block size: %"PRIu32"\n",
			info->block_size);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Blocks per group: %"PRIu32"\n",
			info->blocks_per_group);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Inodes per group: %"PRIu32"\n",
			info->inodes_per_group);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Inode size: %"PRIu32"\n",
			info->inode_size);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Inodes: %"PRIu32"\n", info->inodes);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Journal blocks: %"PRIu32"\n",
			info->journal_blocks);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Features ro_compat: 0x%x\n",
			info->feat_ro_compat);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Features compat: 0x%x\n",
			info->feat_compat);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Features incompat: 0x%x\n",
			info->feat_incompat);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "BG desc reserve: %"PRIu32"\n",
			info->bg_desc_reserve_blocks);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Descriptor size: %"PRIu16"\n",
			info->dsc_size);
	ext4_dbg(DEBUG_MKFS, DBG_NONE "journal: %s\n",
			info->journal ? "yes" : "no");
	ext4_dbg(DEBUG_MKFS, DBG_NONE "Label: %s\n", info->label);

	struct ext4_bcache bc;

	memset(&bc, 0, sizeof(struct ext4_bcache));
	ext4_block_set_lb_size(bd, info->block_size);

	r = ext4_bcache_init_dynamic(&bc, CONFIG_BLOCK_DEV_CACHE_SIZE,
				      info->block_size);
	if (r != EOK)
		goto block_fini;

	/*Bind block cache to block device*/
	r = ext4_block_bind_bcache(bd, &bc);
	if (r != EOK)
		goto cache_fini;

	r = ext4_block_cache_write_back(bd, 1);
	if (r != EOK)
		goto cache_fini;

	r = mkfs_init(bd, info);
	if (r != EOK)
		goto cache_fini;

	r = ext4_fs_init(fs, bd, false);
	if (r != EOK)
		goto cache_fini;

	r = init_bgs(fs);
	if (r != EOK)
		goto fs_fini;

	r = alloc_inodes(fs);
	if (r != EOK)
		goto fs_fini;

	r = create_dirs(fs);
	if (r != EOK)
		goto fs_fini;

	r = create_journal_inode(fs, info);
	if (r != EOK)
		goto fs_fini;

	fs_fini:
	ext4_fs_fini(fs);

	cache_fini:
	ext4_block_cache_write_back(bd, 0);
	ext4_bcache_fini_dynamic(&bc);

	block_fini:
	ext4_block_fini(bd);

	return r;
}

/**
 * @}
 */
