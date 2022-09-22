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
 * @file  ext4_fs.c
 * @brief More complex filesystem functions.
 */

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_debug.h>

#include <ext4_trans.h>
#include <ext4_fs.h>
#include <ext4_blockdev.h>
#include <ext4_super.h>
#include <ext4_crc32.h>
#include <ext4_block_group.h>
#include <ext4_balloc.h>
#include <ext4_bitmap.h>
#include <ext4_inode.h>
#include <ext4_ialloc.h>
#include <ext4_extent.h>

#include <string.h>

int ext4_fs_init(struct ext4_fs *fs, struct ext4_blockdev *bdev,
		 bool read_only)
{
	int r, i;
	uint16_t tmp;
	uint32_t bsize;

	ext4_assert(fs && bdev);

	fs->bdev = bdev;

	fs->read_only = read_only;

	r = ext4_sb_read(fs->bdev, &fs->sb);
	if (r != EOK)
		return r;

	if (!ext4_sb_check(&fs->sb))
		return ENOTSUP;

	bsize = ext4_sb_get_block_size(&fs->sb);
	if (bsize > EXT4_MAX_BLOCK_SIZE)
		return ENXIO;

	r = ext4_fs_check_features(fs, &read_only);
	if (r != EOK)
		return r;

	if (read_only)
		fs->read_only = read_only;

	/* Compute limits for indirect block levels */
	uint32_t blocks_id = bsize / sizeof(uint32_t);

	fs->inode_block_limits[0] = EXT4_INODE_DIRECT_BLOCK_COUNT;
	fs->inode_blocks_per_level[0] = 1;

	for (i = 1; i < 4; i++) {
		fs->inode_blocks_per_level[i] =
		    fs->inode_blocks_per_level[i - 1] * blocks_id;
		fs->inode_block_limits[i] = fs->inode_block_limits[i - 1] +
					    fs->inode_blocks_per_level[i];
	}

	/*Validate FS*/
	tmp = ext4_get16(&fs->sb, state);
	if (tmp & EXT4_SUPERBLOCK_STATE_ERROR_FS)
		ext4_dbg(DEBUG_FS, DBG_WARN
				"last umount error: superblock fs_error flag\n");


	if (!fs->read_only) {
		/* Mark system as mounted */
		ext4_set16(&fs->sb, state, EXT4_SUPERBLOCK_STATE_ERROR_FS);
		r = ext4_sb_write(fs->bdev, &fs->sb);
		if (r != EOK)
			return r;

		/*Update mount count*/
		ext4_set16(&fs->sb, mount_count, ext4_get16(&fs->sb, mount_count) + 1);
	}

	return r;
}

int ext4_fs_fini(struct ext4_fs *fs)
{
	ext4_assert(fs);

	/*Set superblock state*/
	ext4_set16(&fs->sb, state, EXT4_SUPERBLOCK_STATE_VALID_FS);

	if (!fs->read_only)
		return ext4_sb_write(fs->bdev, &fs->sb);

	return EOK;
}

static void ext4_fs_debug_features_inc(uint32_t features_incompatible)
{
	if (features_incompatible & EXT4_FINCOM_COMPRESSION)
		ext4_dbg(DEBUG_FS, DBG_NONE "compression\n");
	if (features_incompatible & EXT4_FINCOM_FILETYPE)
		ext4_dbg(DEBUG_FS, DBG_NONE "filetype\n");
	if (features_incompatible & EXT4_FINCOM_RECOVER)
		ext4_dbg(DEBUG_FS, DBG_NONE "recover\n");
	if (features_incompatible & EXT4_FINCOM_JOURNAL_DEV)
		ext4_dbg(DEBUG_FS, DBG_NONE "journal_dev\n");
	if (features_incompatible & EXT4_FINCOM_META_BG)
		ext4_dbg(DEBUG_FS, DBG_NONE "meta_bg\n");
	if (features_incompatible & EXT4_FINCOM_EXTENTS)
		ext4_dbg(DEBUG_FS, DBG_NONE "extents\n");
	if (features_incompatible & EXT4_FINCOM_64BIT)
		ext4_dbg(DEBUG_FS, DBG_NONE "64bit\n");
	if (features_incompatible & EXT4_FINCOM_MMP)
		ext4_dbg(DEBUG_FS, DBG_NONE "mnp\n");
	if (features_incompatible & EXT4_FINCOM_FLEX_BG)
		ext4_dbg(DEBUG_FS, DBG_NONE "flex_bg\n");
	if (features_incompatible & EXT4_FINCOM_EA_INODE)
		ext4_dbg(DEBUG_FS, DBG_NONE "ea_inode\n");
	if (features_incompatible & EXT4_FINCOM_DIRDATA)
		ext4_dbg(DEBUG_FS, DBG_NONE "dirdata\n");
	if (features_incompatible & EXT4_FINCOM_BG_USE_META_CSUM)
		ext4_dbg(DEBUG_FS, DBG_NONE "meta_csum\n");
	if (features_incompatible & EXT4_FINCOM_LARGEDIR)
		ext4_dbg(DEBUG_FS, DBG_NONE "largedir\n");
	if (features_incompatible & EXT4_FINCOM_INLINE_DATA)
		ext4_dbg(DEBUG_FS, DBG_NONE "inline_data\n");
}
static void ext4_fs_debug_features_comp(uint32_t features_compatible)
{
	if (features_compatible & EXT4_FCOM_DIR_PREALLOC)
		ext4_dbg(DEBUG_FS, DBG_NONE "dir_prealloc\n");
	if (features_compatible & EXT4_FCOM_IMAGIC_INODES)
		ext4_dbg(DEBUG_FS, DBG_NONE "imagic_inodes\n");
	if (features_compatible & EXT4_FCOM_HAS_JOURNAL)
		ext4_dbg(DEBUG_FS, DBG_NONE "has_journal\n");
	if (features_compatible & EXT4_FCOM_EXT_ATTR)
		ext4_dbg(DEBUG_FS, DBG_NONE "ext_attr\n");
	if (features_compatible & EXT4_FCOM_RESIZE_INODE)
		ext4_dbg(DEBUG_FS, DBG_NONE "resize_inode\n");
	if (features_compatible & EXT4_FCOM_DIR_INDEX)
		ext4_dbg(DEBUG_FS, DBG_NONE "dir_index\n");
}

static void ext4_fs_debug_features_ro(uint32_t features_ro)
{
	if (features_ro & EXT4_FRO_COM_SPARSE_SUPER)
		ext4_dbg(DEBUG_FS, DBG_NONE "sparse_super\n");
	if (features_ro & EXT4_FRO_COM_LARGE_FILE)
		ext4_dbg(DEBUG_FS, DBG_NONE "large_file\n");
	if (features_ro & EXT4_FRO_COM_BTREE_DIR)
		ext4_dbg(DEBUG_FS, DBG_NONE "btree_dir\n");
	if (features_ro & EXT4_FRO_COM_HUGE_FILE)
		ext4_dbg(DEBUG_FS, DBG_NONE "huge_file\n");
	if (features_ro & EXT4_FRO_COM_GDT_CSUM)
		ext4_dbg(DEBUG_FS, DBG_NONE "gtd_csum\n");
	if (features_ro & EXT4_FRO_COM_DIR_NLINK)
		ext4_dbg(DEBUG_FS, DBG_NONE "dir_nlink\n");
	if (features_ro & EXT4_FRO_COM_EXTRA_ISIZE)
		ext4_dbg(DEBUG_FS, DBG_NONE "extra_isize\n");
	if (features_ro & EXT4_FRO_COM_QUOTA)
		ext4_dbg(DEBUG_FS, DBG_NONE "quota\n");
	if (features_ro & EXT4_FRO_COM_BIGALLOC)
		ext4_dbg(DEBUG_FS, DBG_NONE "bigalloc\n");
	if (features_ro & EXT4_FRO_COM_METADATA_CSUM)
		ext4_dbg(DEBUG_FS, DBG_NONE "metadata_csum\n");
}

int ext4_fs_check_features(struct ext4_fs *fs, bool *read_only)
{
	ext4_assert(fs && read_only);
	uint32_t v;
	if (ext4_get32(&fs->sb, rev_level) == 0) {
		*read_only = false;
		return EOK;
	}

	ext4_dbg(DEBUG_FS, DBG_INFO "sblock features_incompatible:\n");
	ext4_fs_debug_features_inc(ext4_get32(&fs->sb, features_incompatible));

	ext4_dbg(DEBUG_FS, DBG_INFO "sblock features_compatible:\n");
	ext4_fs_debug_features_comp(ext4_get32(&fs->sb, features_compatible));

	ext4_dbg(DEBUG_FS, DBG_INFO "sblock features_read_only:\n");
	ext4_fs_debug_features_ro(ext4_get32(&fs->sb, features_read_only));

	/*Check features_incompatible*/
	v = (ext4_get32(&fs->sb, features_incompatible) &
	     (~CONFIG_SUPPORTED_FINCOM));
	if (v) {
		ext4_dbg(DEBUG_FS, DBG_ERROR
				"sblock has unsupported features incompatible:\n");
		ext4_fs_debug_features_inc(v);
		return ENOTSUP;
	}

	/*Check features_read_only*/
	v = ext4_get32(&fs->sb, features_read_only);
	v &= ~CONFIG_SUPPORTED_FRO_COM;
	if (v) {
		ext4_dbg(DEBUG_FS, DBG_WARN
			"sblock has unsupported features read only:\n");
		ext4_fs_debug_features_ro(v);
		*read_only = true;
		return EOK;
	}
	*read_only = false;

	return EOK;
}

/**@brief Determine whether the block is inside the group.
 * @param baddr   block address
 * @param bgid    block group id
 * @return Error code
 */
static bool ext4_block_in_group(struct ext4_sblock *s, ext4_fsblk_t baddr,
			        uint32_t bgid)
{
	uint32_t actual_bgid;
	actual_bgid = ext4_balloc_get_bgid_of_block(s, baddr);
	if (actual_bgid == bgid)
		return true;
	return false;
}

/**@brief   To avoid calling the atomic setbit hundreds or thousands of times, we only
 *          need to use it within a single byte (to ensure we get endianness right).
 *          We can use memset for the rest of the bitmap as there are no other users.
 */
static void ext4_fs_mark_bitmap_end(int start_bit, int end_bit, void *bitmap)
{
	int i;

	if (start_bit >= end_bit)
		return;

	for (i = start_bit; (unsigned)i < ((start_bit + 7) & ~7UL); i++)
		ext4_bmap_bit_set(bitmap, i);

	if (i < end_bit)
		memset((char *)bitmap + (i >> 3), 0xff, (end_bit - i) >> 3);
}

/**@brief Initialize block bitmap in block group.
 * @param bg_ref Reference to block group
 * @return Error code
 */
static int ext4_fs_init_block_bitmap(struct ext4_block_group_ref *bg_ref)
{
	struct ext4_sblock *sb = &bg_ref->fs->sb;
	struct ext4_bgroup *bg = bg_ref->block_group;
	int rc;

	uint32_t bit, bit_max;
	uint32_t group_blocks;
	uint16_t inode_size = ext4_get16(sb, inode_size);
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint32_t inodes_per_group = ext4_get32(sb, inodes_per_group);

	ext4_fsblk_t i;
	ext4_fsblk_t bmp_blk = ext4_bg_get_block_bitmap(bg, sb);
	ext4_fsblk_t bmp_inode = ext4_bg_get_inode_bitmap(bg, sb);
	ext4_fsblk_t inode_table = ext4_bg_get_inode_table_first_block(bg, sb);
	ext4_fsblk_t first_bg = ext4_balloc_get_block_of_bgid(sb, bg_ref->index);

	uint32_t dsc_per_block =  block_size / ext4_sb_get_desc_size(sb);

	bool flex_bg = ext4_sb_feature_incom(sb, EXT4_FINCOM_FLEX_BG);
	bool meta_bg = ext4_sb_feature_incom(sb, EXT4_FINCOM_META_BG);

	uint32_t inode_table_bcnt = inodes_per_group * inode_size / block_size;

	struct ext4_block block_bitmap;
	rc = ext4_trans_block_get_noread(bg_ref->fs->bdev, &block_bitmap, bmp_blk);
	if (rc != EOK)
		return rc;

	memset(block_bitmap.data, 0, block_size);
	bit_max = ext4_sb_is_super_in_bg(sb, bg_ref->index);

	uint32_t count = ext4_sb_first_meta_bg(sb) * dsc_per_block;
	if (!meta_bg || bg_ref->index < count) {
		if (bit_max) {
			bit_max += ext4_bg_num_gdb(sb, bg_ref->index);
			bit_max += ext4_get16(sb, s_reserved_gdt_blocks);
		}
	} else { /* For META_BG_BLOCK_GROUPS */
		bit_max += ext4_bg_num_gdb(sb, bg_ref->index);
	}
	for (bit = 0; bit < bit_max; bit++)
		ext4_bmap_bit_set(block_bitmap.data, bit);

	if (bg_ref->index == ext4_block_group_cnt(sb) - 1) {
		/*
		 * Even though mke2fs always initialize first and last group
		 * if some other tool enabled the EXT4_BG_BLOCK_UNINIT we need
		 * to make sure we calculate the right free blocks
		 */

		group_blocks = (uint32_t)(ext4_sb_get_blocks_cnt(sb) -
					  ext4_get32(sb, first_data_block) -
					  ext4_get32(sb, blocks_per_group) *
					  (ext4_block_group_cnt(sb) - 1));
	} else {
		group_blocks = ext4_get32(sb, blocks_per_group);
	}

	bool in_bg;
	in_bg = ext4_block_in_group(sb, bmp_blk, bg_ref->index);
	if (!flex_bg || in_bg)
		ext4_bmap_bit_set(block_bitmap.data,
				  (uint32_t)(bmp_blk - first_bg));

	in_bg = ext4_block_in_group(sb, bmp_inode, bg_ref->index);
	if (!flex_bg || in_bg)
		ext4_bmap_bit_set(block_bitmap.data,
				  (uint32_t)(bmp_inode - first_bg));

        for (i = inode_table; i < inode_table + inode_table_bcnt; i++) {
		in_bg = ext4_block_in_group(sb, i, bg_ref->index);
		if (!flex_bg || in_bg)
			ext4_bmap_bit_set(block_bitmap.data,
					  (uint32_t)(i - first_bg));
	}
        /*
         * Also if the number of blocks within the group is
         * less than the blocksize * 8 ( which is the size
         * of bitmap ), set rest of the block bitmap to 1
         */
        ext4_fs_mark_bitmap_end(group_blocks, block_size * 8, block_bitmap.data);
	ext4_trans_set_block_dirty(block_bitmap.buf);

	ext4_balloc_set_bitmap_csum(sb, bg_ref->block_group, block_bitmap.data);
	bg_ref->dirty = true;

	/* Save bitmap */
	return ext4_block_set(bg_ref->fs->bdev, &block_bitmap);
}

/**@brief Initialize i-node bitmap in block group.
 * @param bg_ref Reference to block group
 * @return Error code
 */
static int ext4_fs_init_inode_bitmap(struct ext4_block_group_ref *bg_ref)
{
	int rc;
	struct ext4_sblock *sb = &bg_ref->fs->sb;
	struct ext4_bgroup *bg = bg_ref->block_group;

	/* Load bitmap */
	ext4_fsblk_t bitmap_block_addr = ext4_bg_get_inode_bitmap(bg, sb);

	struct ext4_block b;
	rc = ext4_trans_block_get_noread(bg_ref->fs->bdev, &b, bitmap_block_addr);
	if (rc != EOK)
		return rc;

	/* Initialize all bitmap bits to zero */
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint32_t inodes_per_group = ext4_get32(sb, inodes_per_group);

	memset(b.data, 0, (inodes_per_group + 7) / 8);

	uint32_t start_bit = inodes_per_group;
	uint32_t end_bit = block_size * 8;

	uint32_t i;
	for (i = start_bit; i < ((start_bit + 7) & ~7UL); i++)
		ext4_bmap_bit_set(b.data, i);

	if (i < end_bit)
		memset(b.data + (i >> 3), 0xff, (end_bit - i) >> 3);

	ext4_trans_set_block_dirty(b.buf);

	ext4_ialloc_set_bitmap_csum(sb, bg, b.data);
	bg_ref->dirty = true;

	/* Save bitmap */
	return ext4_block_set(bg_ref->fs->bdev, &b);
}

/**@brief Initialize i-node table in block group.
 * @param bg_ref Reference to block group
 * @return Error code
 */
static int ext4_fs_init_inode_table(struct ext4_block_group_ref *bg_ref)
{
	struct ext4_sblock *sb = &bg_ref->fs->sb;
	struct ext4_bgroup *bg = bg_ref->block_group;

	uint32_t inode_size = ext4_get16(sb, inode_size);
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint32_t inodes_per_block = block_size / inode_size;
	uint32_t inodes_in_group = ext4_inodes_in_group_cnt(sb, bg_ref->index);
	uint32_t table_blocks = inodes_in_group / inodes_per_block;
	ext4_fsblk_t fblock;

	if (inodes_in_group % inodes_per_block)
		table_blocks++;

	/* Compute initialization bounds */
	ext4_fsblk_t first_block = ext4_bg_get_inode_table_first_block(bg, sb);

	ext4_fsblk_t last_block = first_block + table_blocks - 1;

	/* Initialization of all itable blocks */
	for (fblock = first_block; fblock <= last_block; ++fblock) {
		struct ext4_block b;
		int rc = ext4_trans_block_get_noread(bg_ref->fs->bdev, &b, fblock);
		if (rc != EOK)
			return rc;

		memset(b.data, 0, block_size);
		ext4_trans_set_block_dirty(b.buf);

		rc = ext4_block_set(bg_ref->fs->bdev, &b);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

static ext4_fsblk_t ext4_fs_get_descriptor_block(struct ext4_sblock *s,
					     uint32_t bgid,
					     uint32_t dsc_per_block)
{
	uint32_t first_meta_bg, dsc_id;
	int has_super = 0;
	dsc_id = bgid / dsc_per_block;
	first_meta_bg = ext4_sb_first_meta_bg(s);

	bool meta_bg = ext4_sb_feature_incom(s, EXT4_FINCOM_META_BG);

	if (!meta_bg || dsc_id < first_meta_bg)
		return ext4_get32(s, first_data_block) + dsc_id + 1;

	if (ext4_sb_is_super_in_bg(s, bgid))
		has_super = 1;

	return (has_super + ext4_fs_first_bg_block_no(s, bgid));
}

/**@brief  Compute checksum of block group descriptor.
 * @param sb   Superblock
 * @param bgid Index of block group in the filesystem
 * @param bg   Block group to compute checksum for
 * @return Checksum value
 */
static uint16_t ext4_fs_bg_checksum(struct ext4_sblock *sb, uint32_t bgid,
				    struct ext4_bgroup *bg)
{
	/* If checksum not supported, 0 will be returned */
	uint16_t crc = 0;
#if CONFIG_META_CSUM_ENABLE
	/* Compute the checksum only if the filesystem supports it */
	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		/* Use metadata_csum algorithm instead */
		uint32_t le32_bgid = to_le32(bgid);
		uint32_t orig_checksum, checksum;

		/* Preparation: temporarily set bg checksum to 0 */
		orig_checksum = bg->checksum;
		bg->checksum = 0;

		/* First calculate crc32 checksum against fs uuid */
		checksum = ext4_crc32c(EXT4_CRC32_INIT, sb->uuid,
				sizeof(sb->uuid));
		/* Then calculate crc32 checksum against bgid */
		checksum = ext4_crc32c(checksum, &le32_bgid, sizeof(bgid));
		/* Finally calculate crc32 checksum against block_group_desc */
		checksum = ext4_crc32c(checksum, bg, ext4_sb_get_desc_size(sb));
		bg->checksum = orig_checksum;

		crc = checksum & 0xFFFF;
		return crc;
	}
#endif
	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_GDT_CSUM)) {
		uint8_t *base = (uint8_t *)bg;
		uint8_t *checksum = (uint8_t *)&bg->checksum;

		uint32_t offset = (uint32_t)(checksum - base);

		/* Convert block group index to little endian */
		uint32_t group = to_le32(bgid);

		/* Initialization */
		crc = ext4_bg_crc16(~0, sb->uuid, sizeof(sb->uuid));

		/* Include index of block group */
		crc = ext4_bg_crc16(crc, (uint8_t *)&group, sizeof(group));

		/* Compute crc from the first part (stop before checksum field)
		 */
		crc = ext4_bg_crc16(crc, (uint8_t *)bg, offset);

		/* Skip checksum */
		offset += sizeof(bg->checksum);

		/* Checksum of the rest of block group descriptor */
		if ((ext4_sb_feature_incom(sb, EXT4_FINCOM_64BIT)) &&
		    (offset < ext4_sb_get_desc_size(sb))) {

			const uint8_t *start = ((uint8_t *)bg) + offset;
			size_t len = ext4_sb_get_desc_size(sb) - offset;
			crc = ext4_bg_crc16(crc, start, len);
		}
	}
	return crc;
}

#if CONFIG_META_CSUM_ENABLE
static bool ext4_fs_verify_bg_csum(struct ext4_sblock *sb,
				   uint32_t bgid,
				   struct ext4_bgroup *bg)
{
	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return true;

	return ext4_fs_bg_checksum(sb, bgid, bg) == to_le16(bg->checksum);
}
#else
#define ext4_fs_verify_bg_csum(...) true
#endif

int ext4_fs_get_block_group_ref(struct ext4_fs *fs, uint32_t bgid,
				struct ext4_block_group_ref *ref)
{
	/* Compute number of descriptors, that fits in one data block */
	uint32_t block_size = ext4_sb_get_block_size(&fs->sb);
	uint32_t dsc_cnt = block_size / ext4_sb_get_desc_size(&fs->sb);

	/* Block group descriptor table starts at the next block after
	 * superblock */
	uint64_t block_id = ext4_fs_get_descriptor_block(&fs->sb, bgid, dsc_cnt);

	uint32_t offset = (bgid % dsc_cnt) * ext4_sb_get_desc_size(&fs->sb);

	int rc = ext4_trans_block_get(fs->bdev, &ref->block, block_id);
	if (rc != EOK)
		return rc;

	ref->block_group = (void *)(ref->block.data + offset);
	ref->fs = fs;
	ref->index = bgid;
	ref->dirty = false;
	struct ext4_bgroup *bg = ref->block_group;

	if (!ext4_fs_verify_bg_csum(&fs->sb, bgid, bg)) {
		ext4_dbg(DEBUG_FS,
			 DBG_WARN "Block group descriptor checksum failed."
			 "Block group index: %" PRIu32"\n",
			 bgid);
	}

	if (ext4_bg_has_flag(bg, EXT4_BLOCK_GROUP_BLOCK_UNINIT)) {
		rc = ext4_fs_init_block_bitmap(ref);
		if (rc != EOK) {
			ext4_block_set(fs->bdev, &ref->block);
			return rc;
		}
		ext4_bg_clear_flag(bg, EXT4_BLOCK_GROUP_BLOCK_UNINIT);
		ref->dirty = true;
	}

	if (ext4_bg_has_flag(bg, EXT4_BLOCK_GROUP_INODE_UNINIT)) {
		rc = ext4_fs_init_inode_bitmap(ref);
		if (rc != EOK) {
			ext4_block_set(ref->fs->bdev, &ref->block);
			return rc;
		}

		ext4_bg_clear_flag(bg, EXT4_BLOCK_GROUP_INODE_UNINIT);

		if (!ext4_bg_has_flag(bg, EXT4_BLOCK_GROUP_ITABLE_ZEROED)) {
			rc = ext4_fs_init_inode_table(ref);
			if (rc != EOK) {
				ext4_block_set(fs->bdev, &ref->block);
				return rc;
			}

			ext4_bg_set_flag(bg, EXT4_BLOCK_GROUP_ITABLE_ZEROED);
		}

		ref->dirty = true;
	}

	return EOK;
}

int ext4_fs_put_block_group_ref(struct ext4_block_group_ref *ref)
{
	/* Check if reference modified */
	if (ref->dirty) {
		/* Compute new checksum of block group */
		uint16_t cs;
		cs = ext4_fs_bg_checksum(&ref->fs->sb, ref->index,
					 ref->block_group);
		ref->block_group->checksum = to_le16(cs);

		/* Mark block dirty for writing changes to physical device */
		ext4_trans_set_block_dirty(ref->block.buf);
	}

	/* Put back block, that contains block group descriptor */
	return ext4_block_set(ref->fs->bdev, &ref->block);
}

#if CONFIG_META_CSUM_ENABLE
static uint32_t ext4_fs_inode_checksum(struct ext4_inode_ref *inode_ref)
{
	uint32_t checksum = 0;
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	uint16_t inode_size = ext4_get16(sb, inode_size);

	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		uint32_t orig_checksum;

		uint32_t ino_index = to_le32(inode_ref->index);
		uint32_t ino_gen =
			to_le32(ext4_inode_get_generation(inode_ref->inode));

		/* Preparation: temporarily set bg checksum to 0 */
		orig_checksum = ext4_inode_get_csum(sb, inode_ref->inode);
		ext4_inode_set_csum(sb, inode_ref->inode, 0);

		/* First calculate crc32 checksum against fs uuid */
		checksum = ext4_crc32c(EXT4_CRC32_INIT, sb->uuid,
				       sizeof(sb->uuid));
		/* Then calculate crc32 checksum against inode number
		 * and inode generation */
		checksum = ext4_crc32c(checksum, &ino_index, sizeof(ino_index));
		checksum = ext4_crc32c(checksum, &ino_gen, sizeof(ino_gen));
		/* Finally calculate crc32 checksum against
		 * the entire inode */
		checksum = ext4_crc32c(checksum, inode_ref->inode, inode_size);
		ext4_inode_set_csum(sb, inode_ref->inode, orig_checksum);

		/* If inode size is not large enough to hold the
		 * upper 16bit of the checksum */
		if (inode_size == EXT4_GOOD_OLD_INODE_SIZE)
			checksum &= 0xFFFF;

	}
	return checksum;
}
#else
#define ext4_fs_inode_checksum(...) 0
#endif

static void ext4_fs_set_inode_checksum(struct ext4_inode_ref *inode_ref)
{
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return;

	uint32_t csum = ext4_fs_inode_checksum(inode_ref);
	ext4_inode_set_csum(sb, inode_ref->inode, csum);
}

#if CONFIG_META_CSUM_ENABLE
static bool ext4_fs_verify_inode_csum(struct ext4_inode_ref *inode_ref)
{
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return true;

	return ext4_inode_get_csum(sb, inode_ref->inode) ==
		ext4_fs_inode_checksum(inode_ref);
}
#else
#define ext4_fs_verify_inode_csum(...) true
#endif

static int
__ext4_fs_get_inode_ref(struct ext4_fs *fs, uint32_t index,
			struct ext4_inode_ref *ref,
			bool initialized)
{
	/* Compute number of i-nodes, that fits in one data block */
	uint32_t inodes_per_group = ext4_get32(&fs->sb, inodes_per_group);

	/*
	 * Inode numbers are 1-based, but it is simpler to work with 0-based
	 * when computing indices
	 */
	index -= 1;
	uint32_t block_group = index / inodes_per_group;
	uint32_t offset_in_group = index % inodes_per_group;

	/* Load block group, where i-node is located */
	struct ext4_block_group_ref bg_ref;

	int rc = ext4_fs_get_block_group_ref(fs, block_group, &bg_ref);
	if (rc != EOK) {
		return rc;
	}

	/* Load block address, where i-node table is located */
	ext4_fsblk_t inode_table_start =
	    ext4_bg_get_inode_table_first_block(bg_ref.block_group, &fs->sb);

	/* Put back block group reference (not needed more) */
	rc = ext4_fs_put_block_group_ref(&bg_ref);
	if (rc != EOK) {
		return rc;
	}

	/* Compute position of i-node in the block group */
	uint16_t inode_size = ext4_get16(&fs->sb, inode_size);
	uint32_t block_size = ext4_sb_get_block_size(&fs->sb);
	uint32_t byte_offset_in_group = offset_in_group * inode_size;

	/* Compute block address */
	ext4_fsblk_t block_id =
	    inode_table_start + (byte_offset_in_group / block_size);

	rc = ext4_trans_block_get(fs->bdev, &ref->block, block_id);
	if (rc != EOK) {
		return rc;
	}

	/* Compute position of i-node in the data block */
	uint32_t offset_in_block = byte_offset_in_group % block_size;
	ref->inode = (struct ext4_inode *)(ref->block.data + offset_in_block);

	/* We need to store the original value of index in the reference */
	ref->index = index + 1;
	ref->fs = fs;
	ref->dirty = false;

	if (initialized && !ext4_fs_verify_inode_csum(ref)) {
		ext4_dbg(DEBUG_FS,
			DBG_WARN "Inode checksum failed."
			"Inode: %" PRIu32"\n",
			ref->index);
	}

	return EOK;
}

int ext4_fs_get_inode_ref(struct ext4_fs *fs, uint32_t index,
			  struct ext4_inode_ref *ref)
{
	return __ext4_fs_get_inode_ref(fs, index, ref, true);
}

int ext4_fs_put_inode_ref(struct ext4_inode_ref *ref)
{
	/* Check if reference modified */
	if (ref->dirty) {
		/* Mark block dirty for writing changes to physical device */
		ext4_fs_set_inode_checksum(ref);
		ext4_trans_set_block_dirty(ref->block.buf);
	}

	/* Put back block, that contains i-node */
	return ext4_block_set(ref->fs->bdev, &ref->block);
}

void ext4_fs_inode_blocks_init(struct ext4_fs *fs,
			       struct ext4_inode_ref *inode_ref)
{
	struct ext4_inode *inode = inode_ref->inode;

	/* Reset blocks array. For inode which is not directory or file, just
	 * fill in blocks with 0 */
	switch (ext4_inode_type(&fs->sb, inode_ref->inode)) {
	case EXT4_INODE_MODE_FILE:
	case EXT4_INODE_MODE_DIRECTORY:
		break;
	default:
		return;
	}

#if CONFIG_EXTENT_ENABLE && CONFIG_EXTENTS_ENABLE
	/* Initialize extents if needed */
	if (ext4_sb_feature_incom(&fs->sb, EXT4_FINCOM_EXTENTS)) {
		ext4_inode_set_flag(inode, EXT4_INODE_FLAG_EXTENTS);

		/* Initialize extent root header */
		ext4_extent_tree_init(inode_ref);
	}

	inode_ref->dirty = true;
#endif
}

uint32_t ext4_fs_correspond_inode_mode(int filetype)
{
	switch (filetype) {
	case EXT4_DE_DIR:
		return EXT4_INODE_MODE_DIRECTORY;
	case EXT4_DE_REG_FILE:
		return EXT4_INODE_MODE_FILE;
	case EXT4_DE_SYMLINK:
		return EXT4_INODE_MODE_SOFTLINK;
	case EXT4_DE_CHRDEV:
		return EXT4_INODE_MODE_CHARDEV;
	case EXT4_DE_BLKDEV:
		return EXT4_INODE_MODE_BLOCKDEV;
	case EXT4_DE_FIFO:
		return EXT4_INODE_MODE_FIFO;
	case EXT4_DE_SOCK:
		return EXT4_INODE_MODE_SOCKET;
	}
	/* FIXME: unsupported filetype */
	return EXT4_INODE_MODE_FILE;
}

int ext4_fs_alloc_inode(struct ext4_fs *fs, struct ext4_inode_ref *inode_ref,
			int filetype)
{
	/* Check if newly allocated i-node will be a directory */
	bool is_dir;
	uint16_t inode_size = ext4_get16(&fs->sb, inode_size);

	is_dir = (filetype == EXT4_DE_DIR);

	/* Allocate inode by allocation algorithm */
	uint32_t index;
	int rc = ext4_ialloc_alloc_inode(fs, &index, is_dir);
	if (rc != EOK)
		return rc;

	/* Load i-node from on-disk i-node table */
	rc = __ext4_fs_get_inode_ref(fs, index, inode_ref, false);
	if (rc != EOK) {
		ext4_ialloc_free_inode(fs, index, is_dir);
		return rc;
	}

	/* Initialize i-node */
	struct ext4_inode *inode = inode_ref->inode;

	memset(inode, 0, inode_size);

	uint32_t mode;
	if (is_dir) {
		/*
		 * Default directory permissions to be compatible with other
		 * systems
		 * 0777 (octal) == rwxrwxrwx
		 */

		mode = 0777;
		mode |= EXT4_INODE_MODE_DIRECTORY;
	} else if (filetype == EXT4_DE_SYMLINK) {
		/*
		 * Default symbolic link permissions to be compatible with other systems
		 * 0777 (octal) == rwxrwxrwx
		 */

		mode = 0777;
		mode |= EXT4_INODE_MODE_SOFTLINK;
	} else {
		/*
		 * Default file permissions to be compatible with other systems
		 * 0666 (octal) == rw-rw-rw-
		 */

		mode = 0666;
		mode |= ext4_fs_correspond_inode_mode(filetype);
	}
	ext4_inode_set_mode(&fs->sb, inode, mode);

	ext4_inode_set_links_cnt(inode, 0);
	ext4_inode_set_uid(inode, 0);
	ext4_inode_set_gid(inode, 0);
	ext4_inode_set_size(inode, 0);
	ext4_inode_set_access_time(inode, 0);
	ext4_inode_set_change_inode_time(inode, 0);
	ext4_inode_set_modif_time(inode, 0);
	ext4_inode_set_del_time(inode, 0);
	ext4_inode_set_blocks_count(&fs->sb, inode, 0);
	ext4_inode_set_flags(inode, 0);
	ext4_inode_set_generation(inode, 0);
	if (inode_size > EXT4_GOOD_OLD_INODE_SIZE) {
		uint16_t size = ext4_get16(&fs->sb, want_extra_isize);
		ext4_inode_set_extra_isize(&fs->sb, inode, size);
	}

	memset(inode->blocks, 0, sizeof(inode->blocks));
	inode_ref->dirty = true;

	return EOK;
}

int ext4_fs_free_inode(struct ext4_inode_ref *inode_ref)
{
	struct ext4_fs *fs = inode_ref->fs;
	uint32_t offset;
	uint32_t suboff;
	int rc;
#if CONFIG_EXTENT_ENABLE && CONFIG_EXTENTS_ENABLE
	/* For extents must be data block destroyed by other way */
	if ((ext4_sb_feature_incom(&fs->sb, EXT4_FINCOM_EXTENTS)) &&
	    (ext4_inode_has_flag(inode_ref->inode, EXT4_INODE_FLAG_EXTENTS))) {
		/* Data structures are released during truncate operation... */
		goto finish;
	}
#endif
	/* Release all indirect (no data) blocks */

	/* 1) Single indirect */
	ext4_fsblk_t fblock = ext4_inode_get_indirect_block(inode_ref->inode, 0);
	if (fblock != 0) {
		int rc = ext4_balloc_free_block(inode_ref, fblock);
		if (rc != EOK)
			return rc;

		ext4_inode_set_indirect_block(inode_ref->inode, 0, 0);
	}

	uint32_t block_size = ext4_sb_get_block_size(&fs->sb);
	uint32_t count = block_size / sizeof(uint32_t);

	struct ext4_block block;

	/* 2) Double indirect */
	fblock = ext4_inode_get_indirect_block(inode_ref->inode, 1);
	if (fblock != 0) {
		int rc = ext4_trans_block_get(fs->bdev, &block, fblock);
		if (rc != EOK)
			return rc;

		ext4_fsblk_t ind_block;
		for (offset = 0; offset < count; ++offset) {
			ind_block = to_le32(((uint32_t *)block.data)[offset]);

			if (ind_block == 0)
				continue;
			rc = ext4_balloc_free_block(inode_ref, ind_block);
			if (rc != EOK) {
				ext4_block_set(fs->bdev, &block);
				return rc;
			}

		}

		ext4_block_set(fs->bdev, &block);
		rc = ext4_balloc_free_block(inode_ref, fblock);
		if (rc != EOK)
			return rc;

		ext4_inode_set_indirect_block(inode_ref->inode, 1, 0);
	}

	/* 3) Tripple indirect */
	struct ext4_block subblock;
	fblock = ext4_inode_get_indirect_block(inode_ref->inode, 2);
	if (fblock == 0)
		goto finish;
	rc = ext4_trans_block_get(fs->bdev, &block, fblock);
	if (rc != EOK)
		return rc;

	ext4_fsblk_t ind_block;
	for (offset = 0; offset < count; ++offset) {
		ind_block = to_le32(((uint32_t *)block.data)[offset]);

		if (ind_block == 0)
			continue;
		rc = ext4_trans_block_get(fs->bdev, &subblock,
				ind_block);
		if (rc != EOK) {
			ext4_block_set(fs->bdev, &block);
			return rc;
		}

		ext4_fsblk_t ind_subblk;
		for (suboff = 0; suboff < count; ++suboff) {
			ind_subblk = to_le32(((uint32_t *)subblock.data)[suboff]);

			if (ind_subblk == 0)
				continue;
			rc = ext4_balloc_free_block(inode_ref, ind_subblk);
			if (rc != EOK) {
				ext4_block_set(fs->bdev, &subblock);
				ext4_block_set(fs->bdev, &block);
				return rc;
			}

		}

		ext4_block_set(fs->bdev, &subblock);

		rc = ext4_balloc_free_block(inode_ref,
				ind_block);
		if (rc != EOK) {
			ext4_block_set(fs->bdev, &block);
			return rc;
		}

	}

	ext4_block_set(fs->bdev, &block);
	rc = ext4_balloc_free_block(inode_ref, fblock);
	if (rc != EOK)
		return rc;

	ext4_inode_set_indirect_block(inode_ref->inode, 2, 0);
finish:
	/* Mark inode dirty for writing to the physical device */
	inode_ref->dirty = true;

	/* Free block with extended attributes if present */
	ext4_fsblk_t xattr_block =
	    ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);
	if (xattr_block) {
		int rc = ext4_balloc_free_block(inode_ref, xattr_block);
		if (rc != EOK)
			return rc;

		ext4_inode_set_file_acl(inode_ref->inode, &fs->sb, 0);
	}

	/* Free inode by allocator */
	if (ext4_inode_is_type(&fs->sb, inode_ref->inode,
			       EXT4_INODE_MODE_DIRECTORY))
		rc = ext4_ialloc_free_inode(fs, inode_ref->index, true);
	else
		rc = ext4_ialloc_free_inode(fs, inode_ref->index, false);

	return rc;
}


/**@brief Release data block from i-node
 * @param inode_ref I-node to release block from
 * @param iblock    Logical block to be released
 * @return Error code
 */
static int ext4_fs_release_inode_block(struct ext4_inode_ref *inode_ref,
				ext4_lblk_t iblock)
{
	ext4_fsblk_t fblock;

	struct ext4_fs *fs = inode_ref->fs;

	/* Extents are handled otherwise = there is not support in this function
	 */
	ext4_assert(!(
	    ext4_sb_feature_incom(&fs->sb, EXT4_FINCOM_EXTENTS) &&
	    (ext4_inode_has_flag(inode_ref->inode, EXT4_INODE_FLAG_EXTENTS))));

	struct ext4_inode *inode = inode_ref->inode;

	/* Handle simple case when we are dealing with direct reference */
	if (iblock < EXT4_INODE_DIRECT_BLOCK_COUNT) {
		fblock = ext4_inode_get_direct_block(inode, iblock);

		/* Sparse file */
		if (fblock == 0)
			return EOK;

		ext4_inode_set_direct_block(inode, iblock, 0);
		return ext4_balloc_free_block(inode_ref, fblock);
	}

	/* Determine the indirection level needed to get the desired block */
	unsigned int level = 0;
	unsigned int i;
	for (i = 1; i < 4; i++) {
		if (iblock < fs->inode_block_limits[i]) {
			level = i;
			break;
		}
	}

	if (level == 0)
		return EIO;

	/* Compute offsets for the topmost level */
	uint32_t block_offset_in_level =
		(uint32_t)(iblock - fs->inode_block_limits[level - 1]);
	ext4_fsblk_t current_block =
	    ext4_inode_get_indirect_block(inode, level - 1);
	uint32_t offset_in_block =
	    (uint32_t)(block_offset_in_level / fs->inode_blocks_per_level[level - 1]);

	/*
	 * Navigate through other levels, until we find the block number
	 * or find null reference meaning we are dealing with sparse file
	 */
	struct ext4_block block;

	while (level > 0) {

		/* Sparse check */
		if (current_block == 0)
			return EOK;

		int rc = ext4_trans_block_get(fs->bdev, &block, current_block);
		if (rc != EOK)
			return rc;

		current_block =
		    to_le32(((uint32_t *)block.data)[offset_in_block]);

		/* Set zero if physical data block address found */
		if (level == 1) {
			((uint32_t *)block.data)[offset_in_block] = to_le32(0);
			ext4_trans_set_block_dirty(block.buf);
		}

		rc = ext4_block_set(fs->bdev, &block);
		if (rc != EOK)
			return rc;

		level--;

		/*
		 * If we are on the last level, break here as
		 * there is no next level to visit
		 */
		if (level == 0)
			break;

		/* Visit the next level */
		block_offset_in_level %= fs->inode_blocks_per_level[level];
		offset_in_block = (uint32_t)(block_offset_in_level /
				  fs->inode_blocks_per_level[level - 1]);
	}

	fblock = current_block;
	if (fblock == 0)
		return EOK;

	/* Physical block is not referenced, it can be released */
	return ext4_balloc_free_block(inode_ref, fblock);
}

int ext4_fs_truncate_inode(struct ext4_inode_ref *inode_ref, uint64_t new_size)
{
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	uint32_t i;
	int r;
	bool v;

	/* Check flags, if i-node can be truncated */
	if (!ext4_inode_can_truncate(sb, inode_ref->inode))
		return EINVAL;

	/* If sizes are equal, nothing has to be done. */
	uint64_t old_size = ext4_inode_get_size(sb, inode_ref->inode);
	if (old_size == new_size)
		return EOK;

	/* It's not supported to make the larger file by truncate operation */
	if (old_size < new_size)
		return EINVAL;

	/* For symbolic link which is small enough */
	v = ext4_inode_is_type(sb, inode_ref->inode, EXT4_INODE_MODE_SOFTLINK);
	if (v && old_size < sizeof(inode_ref->inode->blocks) &&
	    !ext4_inode_get_blocks_count(sb, inode_ref->inode)) {
		char *content = (char *)inode_ref->inode->blocks + new_size;
		memset(content, 0,
		       sizeof(inode_ref->inode->blocks) - (uint32_t)new_size);
		ext4_inode_set_size(inode_ref->inode, new_size);
		inode_ref->dirty = true;

		return EOK;
	}

	i = ext4_inode_type(sb, inode_ref->inode);
	if (i == EXT4_INODE_MODE_CHARDEV ||
	    i == EXT4_INODE_MODE_BLOCKDEV ||
	    i == EXT4_INODE_MODE_SOCKET) {
		inode_ref->inode->blocks[0] = 0;
		inode_ref->inode->blocks[1] = 0;

		inode_ref->dirty = true;
		return EOK;
	}

	/* Compute how many blocks will be released */
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint32_t new_blocks_cnt = (uint32_t)((new_size + block_size - 1) / block_size);
	uint32_t old_blocks_cnt = (uint32_t)((old_size + block_size - 1) / block_size);
	uint32_t diff_blocks_cnt = old_blocks_cnt - new_blocks_cnt;
#if CONFIG_EXTENT_ENABLE && CONFIG_EXTENTS_ENABLE
	if ((ext4_sb_feature_incom(sb, EXT4_FINCOM_EXTENTS)) &&
	    (ext4_inode_has_flag(inode_ref->inode, EXT4_INODE_FLAG_EXTENTS))) {

		/* Extents require special operation */
		if (diff_blocks_cnt) {
			r = ext4_extent_remove_space(inode_ref, new_blocks_cnt,
						     EXT_MAX_BLOCKS);
			if (r != EOK)
				return r;

		}
	} else
#endif
	{
		/* Release data blocks from the end of file */

		/* Starting from 1 because of logical blocks are numbered from 0
		 */
		for (i = 0; i < diff_blocks_cnt; ++i) {
			r = ext4_fs_release_inode_block(inode_ref,
							new_blocks_cnt + i);
			if (r != EOK)
				return r;
		}
	}

	/* Update i-node */
	ext4_inode_set_size(inode_ref->inode, new_size);
	inode_ref->dirty = true;

	return EOK;
}

/**@brief Compute 'goal' for inode index
 * @param inode_ref Reference to inode, to allocate block for
 * @return goal
 */
ext4_fsblk_t ext4_fs_inode_to_goal_block(struct ext4_inode_ref *inode_ref)
{
	uint32_t grp_inodes = ext4_get32(&inode_ref->fs->sb, inodes_per_group);
	return (inode_ref->index - 1) / grp_inodes;
}

/**@brief Compute 'goal' for allocation algorithm (For blockmap).
 * @param inode_ref Reference to inode, to allocate block for
 * @return error code
 */
int ext4_fs_indirect_find_goal(struct ext4_inode_ref *inode_ref,
			       ext4_fsblk_t *goal)
{
	int r;
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	*goal = 0;

	uint64_t inode_size = ext4_inode_get_size(sb, inode_ref->inode);
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint32_t iblock_cnt = (uint32_t)(inode_size / block_size);

	if (inode_size % block_size != 0)
		iblock_cnt++;

	/* If inode has some blocks, get last block address + 1 */
	if (iblock_cnt > 0) {
		r = ext4_fs_get_inode_dblk_idx(inode_ref, iblock_cnt - 1,
					       goal, false);
		if (r != EOK)
			return r;

		if (*goal != 0) {
			(*goal)++;
			return r;
		}

		/* If goal == 0, sparse file -> continue */
	}

	/* Identify block group of inode */

	uint32_t inodes_per_bg = ext4_get32(sb, inodes_per_group);
	uint32_t block_group = (inode_ref->index - 1) / inodes_per_bg;
	block_size = ext4_sb_get_block_size(sb);

	/* Load block group reference */
	struct ext4_block_group_ref bg_ref;
	r = ext4_fs_get_block_group_ref(inode_ref->fs, block_group, &bg_ref);
	if (r != EOK)
		return r;

	struct ext4_bgroup *bg = bg_ref.block_group;

	/* Compute indexes */
	uint32_t bg_count = ext4_block_group_cnt(sb);
	ext4_fsblk_t itab_first_block = ext4_bg_get_inode_table_first_block(bg, sb);
	uint16_t itab_item_size = ext4_get16(sb, inode_size);
	uint32_t itab_bytes;

	/* Check for last block group */
	if (block_group < bg_count - 1) {
		itab_bytes = inodes_per_bg * itab_item_size;
	} else {
		/* Last block group could be smaller */
		uint32_t inodes_cnt = ext4_get32(sb, inodes_count);

		itab_bytes = (inodes_cnt - ((bg_count - 1) * inodes_per_bg));
		itab_bytes *= itab_item_size;
	}

	ext4_fsblk_t inode_table_blocks = itab_bytes / block_size;

	if (itab_bytes % block_size)
		inode_table_blocks++;

	*goal = itab_first_block + inode_table_blocks;

	return ext4_fs_put_block_group_ref(&bg_ref);
}

static int ext4_fs_get_inode_dblk_idx_internal(struct ext4_inode_ref *inode_ref,
				       ext4_lblk_t iblock, ext4_fsblk_t *fblock,
				       bool extent_create,
				       bool support_unwritten __unused)
{
	struct ext4_fs *fs = inode_ref->fs;

	/* For empty file is situation simple */
	if (ext4_inode_get_size(&fs->sb, inode_ref->inode) == 0) {
		*fblock = 0;
		return EOK;
	}

	ext4_fsblk_t current_block;

	(void)extent_create;
#if CONFIG_EXTENT_ENABLE && CONFIG_EXTENTS_ENABLE
	/* Handle i-node using extents */
	if ((ext4_sb_feature_incom(&fs->sb, EXT4_FINCOM_EXTENTS)) &&
	    (ext4_inode_has_flag(inode_ref->inode, EXT4_INODE_FLAG_EXTENTS))) {

		ext4_fsblk_t current_fsblk;
		int rc = ext4_extent_get_blocks(inode_ref, iblock, 1,
				&current_fsblk, extent_create, NULL);
		if (rc != EOK)
			return rc;

		current_block = current_fsblk;
		*fblock = current_block;

		ext4_assert(*fblock || support_unwritten);
		return EOK;
	}
#endif

	struct ext4_inode *inode = inode_ref->inode;

	/* Direct block are read directly from array in i-node structure */
	if (iblock < EXT4_INODE_DIRECT_BLOCK_COUNT) {
		current_block =
		    ext4_inode_get_direct_block(inode, (uint32_t)iblock);
		*fblock = current_block;
		return EOK;
	}

	/* Determine indirection level of the target block */
	unsigned int l = 0;
	unsigned int i;
	for (i = 1; i < 4; i++) {
		if (iblock < fs->inode_block_limits[i]) {
			l = i;
			break;
		}
	}

	if (l == 0)
		return EIO;

	/* Compute offsets for the topmost level */
	uint32_t blk_off_in_lvl = (uint32_t)(iblock - fs->inode_block_limits[l - 1]);
	current_block = ext4_inode_get_indirect_block(inode, l - 1);
	uint32_t off_in_blk = (uint32_t)(blk_off_in_lvl / fs->inode_blocks_per_level[l - 1]);

	/* Sparse file */
	if (current_block == 0) {
		*fblock = 0;
		return EOK;
	}

	struct ext4_block block;

	/*
	 * Navigate through other levels, until we find the block number
	 * or find null reference meaning we are dealing with sparse file
	 */
	while (l > 0) {
		/* Load indirect block */
		int rc = ext4_trans_block_get(fs->bdev, &block, current_block);
		if (rc != EOK)
			return rc;

		/* Read block address from indirect block */
		current_block =
		    to_le32(((uint32_t *)block.data)[off_in_blk]);

		/* Put back indirect block untouched */
		rc = ext4_block_set(fs->bdev, &block);
		if (rc != EOK)
			return rc;

		/* Check for sparse file */
		if (current_block == 0) {
			*fblock = 0;
			return EOK;
		}

		/* Jump to the next level */
		l--;

		/* Termination condition - we have address of data block loaded
		 */
		if (l == 0)
			break;

		/* Visit the next level */
		blk_off_in_lvl %= fs->inode_blocks_per_level[l];
		off_in_blk = (uint32_t)(blk_off_in_lvl / fs->inode_blocks_per_level[l - 1]);
	}

	*fblock = current_block;

	return EOK;
}


int ext4_fs_get_inode_dblk_idx(struct ext4_inode_ref *inode_ref,
			       ext4_lblk_t iblock, ext4_fsblk_t *fblock,
			       bool support_unwritten)
{
	return ext4_fs_get_inode_dblk_idx_internal(inode_ref, iblock, fblock,
						   false, support_unwritten);
}

int ext4_fs_init_inode_dblk_idx(struct ext4_inode_ref *inode_ref,
				ext4_lblk_t iblock, ext4_fsblk_t *fblock)
{
	return ext4_fs_get_inode_dblk_idx_internal(inode_ref, iblock, fblock,
						   true, true);
}

static int ext4_fs_set_inode_data_block_index(struct ext4_inode_ref *inode_ref,
				       ext4_lblk_t iblock, ext4_fsblk_t fblock)
{
	struct ext4_fs *fs = inode_ref->fs;

#if CONFIG_EXTENT_ENABLE && CONFIG_EXTENTS_ENABLE
	/* Handle inode using extents */
	if ((ext4_sb_feature_incom(&fs->sb, EXT4_FINCOM_EXTENTS)) &&
	    (ext4_inode_has_flag(inode_ref->inode, EXT4_INODE_FLAG_EXTENTS))) {
		/* Not reachable */
		return ENOTSUP;
	}
#endif

	/* Handle simple case when we are dealing with direct reference */
	if (iblock < EXT4_INODE_DIRECT_BLOCK_COUNT) {
		ext4_inode_set_direct_block(inode_ref->inode, (uint32_t)iblock,
					    (uint32_t)fblock);
		inode_ref->dirty = true;

		return EOK;
	}

	/* Determine the indirection level needed to get the desired block */
	unsigned int l = 0;
	unsigned int i;
	for (i = 1; i < 4; i++) {
		if (iblock < fs->inode_block_limits[i]) {
			l = i;
			break;
		}
	}

	if (l == 0)
		return EIO;

	uint32_t block_size = ext4_sb_get_block_size(&fs->sb);

	/* Compute offsets for the topmost level */
	uint32_t blk_off_in_lvl = (uint32_t)(iblock - fs->inode_block_limits[l - 1]);
	ext4_fsblk_t current_block =
			ext4_inode_get_indirect_block(inode_ref->inode, l - 1);
	uint32_t off_in_blk = (uint32_t)(blk_off_in_lvl / fs->inode_blocks_per_level[l - 1]);

	ext4_fsblk_t new_blk;

	struct ext4_block block;
	struct ext4_block new_block;

	/* Is needed to allocate indirect block on the i-node level */
	if (current_block == 0) {
		/* Allocate new indirect block */
		ext4_fsblk_t goal;
		int rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
		if (rc != EOK)
			return rc;

		rc = ext4_balloc_alloc_block(inode_ref, goal, &new_blk);
		if (rc != EOK)
			return rc;

		/* Update i-node */
		ext4_inode_set_indirect_block(inode_ref->inode, l - 1,
				(uint32_t)new_blk);
		inode_ref->dirty = true;

		/* Load newly allocated block */
		rc = ext4_trans_block_get_noread(fs->bdev, &new_block, new_blk);
		if (rc != EOK) {
			ext4_balloc_free_block(inode_ref, new_blk);
			return rc;
		}

		/* Initialize new block */
		memset(new_block.data, 0, block_size);
		ext4_trans_set_block_dirty(new_block.buf);

		/* Put back the allocated block */
		rc = ext4_block_set(fs->bdev, &new_block);
		if (rc != EOK)
			return rc;

		current_block = new_blk;
	}

	/*
	 * Navigate through other levels, until we find the block number
	 * or find null reference meaning we are dealing with sparse file
	 */
	while (l > 0) {
		int rc = ext4_trans_block_get(fs->bdev, &block, current_block);
		if (rc != EOK)
			return rc;

		current_block = to_le32(((uint32_t *)block.data)[off_in_blk]);
		if ((l > 1) && (current_block == 0)) {
			ext4_fsblk_t goal;
			rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
			if (rc != EOK) {
				ext4_block_set(fs->bdev, &block);
				return rc;
			}

			/* Allocate new block */
			rc =
			    ext4_balloc_alloc_block(inode_ref, goal, &new_blk);
			if (rc != EOK) {
				ext4_block_set(fs->bdev, &block);
				return rc;
			}

			/* Load newly allocated block */
			rc = ext4_trans_block_get_noread(fs->bdev, &new_block,
					    new_blk);

			if (rc != EOK) {
				ext4_block_set(fs->bdev, &block);
				return rc;
			}

			/* Initialize allocated block */
			memset(new_block.data, 0, block_size);
			ext4_trans_set_block_dirty(new_block.buf);

			rc = ext4_block_set(fs->bdev, &new_block);
			if (rc != EOK) {
				ext4_block_set(fs->bdev, &block);
				return rc;
			}

			/* Write block address to the parent */
			uint32_t * p = (uint32_t * )block.data;
			p[off_in_blk] = to_le32((uint32_t)new_blk);
			ext4_trans_set_block_dirty(block.buf);
			current_block = new_blk;
		}

		/* Will be finished, write the fblock address */
		if (l == 1) {
			uint32_t * p = (uint32_t * )block.data;
			p[off_in_blk] = to_le32((uint32_t)fblock);
			ext4_trans_set_block_dirty(block.buf);
		}

		rc = ext4_block_set(fs->bdev, &block);
		if (rc != EOK)
			return rc;

		l--;

		/*
		 * If we are on the last level, break here as
		 * there is no next level to visit
		 */
		if (l == 0)
			break;

		/* Visit the next level */
		blk_off_in_lvl %= fs->inode_blocks_per_level[l];
		off_in_blk = (uint32_t)(blk_off_in_lvl / fs->inode_blocks_per_level[l - 1]);
	}

	return EOK;
}


int ext4_fs_append_inode_dblk(struct ext4_inode_ref *inode_ref,
			      ext4_fsblk_t *fblock, ext4_lblk_t *iblock)
{
#if CONFIG_EXTENT_ENABLE && CONFIG_EXTENTS_ENABLE
	/* Handle extents separately */
	if ((ext4_sb_feature_incom(&inode_ref->fs->sb, EXT4_FINCOM_EXTENTS)) &&
	    (ext4_inode_has_flag(inode_ref->inode, EXT4_INODE_FLAG_EXTENTS))) {
		int rc;
		ext4_fsblk_t current_fsblk;
		struct ext4_sblock *sb = &inode_ref->fs->sb;
		uint64_t inode_size = ext4_inode_get_size(sb, inode_ref->inode);
		uint32_t block_size = ext4_sb_get_block_size(sb);
		*iblock = (uint32_t)((inode_size + block_size - 1) / block_size);

		rc = ext4_extent_get_blocks(inode_ref, *iblock, 1,
						&current_fsblk, true, NULL);
		if (rc != EOK)
			return rc;

		*fblock = current_fsblk;
		ext4_assert(*fblock);

		ext4_inode_set_size(inode_ref->inode, inode_size + block_size);
		inode_ref->dirty = true;


		return rc;
	}
#endif
	struct ext4_sblock *sb = &inode_ref->fs->sb;

	/* Compute next block index and allocate data block */
	uint64_t inode_size = ext4_inode_get_size(sb, inode_ref->inode);
	uint32_t block_size = ext4_sb_get_block_size(sb);

	/* Align size i-node size */
	if ((inode_size % block_size) != 0)
		inode_size += block_size - (inode_size % block_size);

	/* Logical blocks are numbered from 0 */
	uint32_t new_block_idx = (uint32_t)(inode_size / block_size);

	/* Allocate new physical block */
	ext4_fsblk_t goal, phys_block;
	int rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
	if (rc != EOK)
		return rc;

	rc = ext4_balloc_alloc_block(inode_ref, goal, &phys_block);
	if (rc != EOK)
		return rc;

	/* Add physical block address to the i-node */
	rc = ext4_fs_set_inode_data_block_index(inode_ref, new_block_idx,
						phys_block);
	if (rc != EOK) {
		ext4_balloc_free_block(inode_ref, phys_block);
		return rc;
	}

	/* Update i-node */
	ext4_inode_set_size(inode_ref->inode, inode_size + block_size);
	inode_ref->dirty = true;

	*fblock = phys_block;
	*iblock = new_block_idx;

	return EOK;
}

void ext4_fs_inode_links_count_inc(struct ext4_inode_ref *inode_ref)
{
	uint16_t link;
	bool is_dx;
	link = ext4_inode_get_links_cnt(inode_ref->inode);
	link++;
	ext4_inode_set_links_cnt(inode_ref->inode, link);

	is_dx = ext4_sb_feature_com(&inode_ref->fs->sb, EXT4_FCOM_DIR_INDEX) &&
		ext4_inode_has_flag(inode_ref->inode, EXT4_INODE_FLAG_INDEX);

	if (is_dx && link > 1) {
		if (link >= EXT4_LINK_MAX || link == 2) {
			ext4_inode_set_links_cnt(inode_ref->inode, 1);

			uint32_t v;
			v = ext4_get32(&inode_ref->fs->sb, features_read_only);
			v |= EXT4_FRO_COM_DIR_NLINK;
			ext4_set32(&inode_ref->fs->sb, features_read_only, v);
		}
	}
}

void ext4_fs_inode_links_count_dec(struct ext4_inode_ref *inode_ref)
{
	uint16_t links = ext4_inode_get_links_cnt(inode_ref->inode);
	if (!ext4_inode_is_type(&inode_ref->fs->sb, inode_ref->inode,
				EXT4_INODE_MODE_DIRECTORY)) {
		if (links > 0)
			ext4_inode_set_links_cnt(inode_ref->inode, links - 1);
		return;
	}

	if (links > 2)
		ext4_inode_set_links_cnt(inode_ref->inode, links - 1);
}

/**
 * @}
 */
