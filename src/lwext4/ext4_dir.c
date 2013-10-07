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
 * @file  ext4_dir.h
 * @brief Directory handle procedures.
 */

#include <ext4_config.h>
#include <ext4_dir.h>
#include <ext4_dir_idx.h>
#include <ext4_inode.h>
#include <ext4_fs.h>

#include <string.h>

uint32_t ext4_dir_entry_ll_get_inode(struct ext4_directory_entry_ll *de)
{
	return to_le32(de->inode);
}

void 	 ext4_dir_entry_ll_set_inode(struct ext4_directory_entry_ll *de,
		uint32_t inode)
{
	de->inode = to_le32(inode);
}


uint16_t ext4_dir_entry_ll_get_entry_length(struct ext4_directory_entry_ll *de)
{
	return to_le16(de->entry_length);
}

void 	 ext4_dir_entry_ll_set_entry_length(struct ext4_directory_entry_ll *de,
		uint16_t len)
{
	de->entry_length = to_le16(len);
}


uint16_t ext4_dir_entry_ll_get_name_length(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de)
{
	uint16_t v = de->name_length;

	if ((ext4_get32(sb, rev_level) == 0) &&
	    (ext4_get32(sb, minor_rev_level) < 5))
		v |= ((uint16_t)de->name_length_high) << 8;

	return v;
}
void 	ext4_dir_entry_ll_set_name_length(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de, uint16_t len)
{
	de->name_length = (len << 8) >> 8;

	if ((ext4_get32(sb, rev_level) == 0) &&
	    (ext4_get32(sb, minor_rev_level) < 5))
		de->name_length_high = len >> 8;
}



uint8_t ext4_dir_entry_ll_get_inode_type(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de)
{
	if ((ext4_get32(sb, rev_level) > 0) ||
	    (ext4_get32(sb, minor_rev_level) >= 5))
		return de->inode_type;

	return EXT4_DIRECTORY_FILETYPE_UNKNOWN;
}


void 	ext4_dir_entry_ll_set_inode_type(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de, uint8_t type)
{
	if ((ext4_get32(sb, rev_level) > 0) ||
	    (ext4_get32(sb, minor_rev_level) >= 5))
		de->inode_type = type;
}

/*****************************************************************************/


static int ext4_dir_iterator_set(struct ext4_directory_iterator *it, uint32_t block_size)
{
	it->current = NULL;

	uint32_t offset_in_block = it->current_offset % block_size;

	/* Ensure proper alignment */
	if ((offset_in_block % 4) != 0)
		return EIO;

	/* Ensure that the core of the entry does not overflow the block */
	if (offset_in_block > block_size - 8)
		return EIO;

	struct ext4_directory_entry_ll *entry =
	    (void *)(it->current_block.data + offset_in_block);

	/* Ensure that the whole entry does not overflow the block */
	uint16_t length = ext4_dir_entry_ll_get_entry_length(entry);
	if (offset_in_block + length > block_size)
		return EIO;

	/* Ensure the name length is not too large */
	if (ext4_dir_entry_ll_get_name_length(
	    &it->inode_ref->fs->sb, entry) > length-8)
		return EIO;

	/* Everything OK - "publish" the entry */
	it->current = entry;
	return EOK;
}

static int ext4_dir_iterator_seek(struct ext4_directory_iterator *it, uint64_t pos)
{
	uint64_t size = ext4_inode_get_size(&it->inode_ref->fs->sb,
	    it->inode_ref->inode);

	/* The iterator is not valid until we seek to the desired position */
	it->current = NULL;

	/* Are we at the end? */
	if (pos >= size) {
		if (it->current_block.lb_id) {

			int rc = ext4_block_set(it->inode_ref->fs->bdev, &it->current_block);
			it->current_block.lb_id = 0;

			if (rc != EOK)
				return rc;
		}

		it->current_offset = pos;
		return EOK;
	}

	/* Compute next block address */
	uint32_t block_size =
	    ext4_sb_get_block_size(&it->inode_ref->fs->sb);
	uint64_t current_block_idx = it->current_offset / block_size;
	uint64_t next_block_idx = pos / block_size;

	/*
	 * If we don't have a block or are moving accross block boundary,
	 * we need to get another block
	 */
	if ((it->current_block.lb_id == 0) ||
	    (current_block_idx != next_block_idx)) {
		if (it->current_block.lb_id) {
			int rc = ext4_block_set(it->inode_ref->fs->bdev, &it->current_block);
			it->current_block.lb_id = 0;

			if (rc != EOK)
				return rc;
		}

		uint32_t next_block_phys_idx;
		int rc = ext4_fs_get_inode_data_block_index(it->inode_ref,
		    next_block_idx, &next_block_phys_idx);
		if (rc != EOK)
			return rc;


		rc = ext4_block_get(it->inode_ref->fs->bdev, &it->current_block,
		    next_block_phys_idx);
		if (rc != EOK) {
			it->current_block.lb_id = 0;
			return rc;
		}
	}

	it->current_offset = pos;

	return ext4_dir_iterator_set(it, block_size);
}


int ext4_dir_iterator_init(struct ext4_directory_iterator *it,
		struct ext4_inode_ref *inode_ref, uint64_t pos)
{
	it->inode_ref = inode_ref;
	it->current = 0;
	it->current_offset = 0;
	it->current_block.lb_id = 0;

	return ext4_dir_iterator_seek(it, pos);
}

int ext4_dir_iterator_next(struct ext4_directory_iterator *it)
{
	uint16_t skip = ext4_dir_entry_ll_get_entry_length(it->current);

	return ext4_dir_iterator_seek(it, it->current_offset + skip);
}

int ext4_dir_iterator_fini(struct ext4_directory_iterator *it)
{
	it->current = 0;

	if (it->current_block.lb_id)
		return ext4_block_set(it->inode_ref->fs->bdev, &it->current_block);

	return EOK;
}

void ext4_dir_write_entry(struct ext4_sblock *sb, struct ext4_directory_entry_ll *entry,
		uint16_t entry_len, struct ext4_inode_ref *child,  const char *name, size_t name_len)
{
	/* Check maximum entry length */
	uint32_t block_size = ext4_sb_get_block_size(sb);
	ext4_assert(entry_len <= block_size);

	/* Set basic attributes */
	ext4_dir_entry_ll_set_inode(entry, child->index);
	ext4_dir_entry_ll_set_entry_length(entry, entry_len);
	ext4_dir_entry_ll_set_name_length(sb, entry, name_len);

	/* Write name */
	memcpy(entry->name, name, name_len);

	/* Set type of entry */
	if (ext4_inode_is_type(sb, child->inode, EXT4_INODE_MODE_DIRECTORY))
		ext4_dir_entry_ll_set_inode_type(sb, entry,
		    EXT4_DIRECTORY_FILETYPE_DIR);
	else
		ext4_dir_entry_ll_set_inode_type(sb, entry,
		    EXT4_DIRECTORY_FILETYPE_REG_FILE);
}

int ext4_dir_add_entry(struct ext4_inode_ref *parent, const char *name,
		struct ext4_inode_ref *child)
{
	struct ext4_fs *fs = parent->fs;

#if CONFIG_DIR_INDEX_ENABLE
	/* Index adding (if allowed) */
	if ((ext4_sb_check_feature_compatible(&fs->sb,
	    EXT4_FEATURE_COMPAT_DIR_INDEX)) &&
	    (ext4_inode_has_flag(parent->inode, EXT4_INODE_FLAG_INDEX))) {
		int rc = ext4_dir_dx_add_entry(parent, child, name);

		/* Check if index is not corrupted */
		if (rc != EXT4_ERR_BAD_DX_DIR) {
			if (rc != EOK)
				return rc;

			return EOK;
		}

		/* Needed to clear dir index flag if corrupted */
		ext4_inode_clear_flag(parent->inode, EXT4_INODE_FLAG_INDEX);
		parent->dirty = true;
	}
#endif

	/* Linear algorithm */
	uint32_t iblock = 0;
	uint32_t fblock = 0;
	uint32_t block_size = ext4_sb_get_block_size(&fs->sb);
	uint32_t inode_size = ext4_inode_get_size(&fs->sb, parent->inode);
	uint32_t total_blocks = inode_size / block_size;

	uint32_t name_len = strlen(name);

	/* Find block, where is space for new entry and try to add */
	bool success = false;
	for (iblock = 0; iblock < total_blocks; ++iblock) {
		int rc = ext4_fs_get_inode_data_block_index(parent,
		    iblock, &fblock);
		if (rc != EOK)
			return rc;


		struct ext4_block block;
		rc = ext4_block_get(fs->bdev, &block, fblock);
		if (rc != EOK)
			return rc;

		/* If adding is successful, function can finish */
		rc = ext4_dir_try_insert_entry(&fs->sb, &block,
		    child, name, name_len);
		if (rc == EOK)
			success = true;

		rc = ext4_block_set(fs->bdev, &block);
		if (rc != EOK)
			return rc;

		if (success)
			return EOK;
	}

	/* No free block found - needed to allocate next data block */

	iblock = 0;
	fblock = 0;
	int rc = ext4_fs_append_inode_block(parent, &fblock, &iblock);
	if (rc != EOK)
		return rc;

	/* Load new block */
	struct ext4_block new_block;

	rc = ext4_block_get(fs->bdev, &new_block, fblock);
	if (rc != EOK)
		return rc;

	/* Fill block with zeroes */
	memset(new_block.data, 0, block_size);
	struct ext4_directory_entry_ll *block_entry = (void *)new_block.data;
	ext4_dir_write_entry(&fs->sb, block_entry, block_size,
	    child, name, name_len);

	/* Save new block */
	new_block.dirty = true;
	rc = ext4_block_set(fs->bdev, &new_block);

	return rc;
}

int ext4_dir_find_entry(struct ext4_directory_search_result *result,
    struct ext4_inode_ref *parent, const char *name)
{
	uint32_t name_len = strlen(name);

	struct ext4_sblock *sb = &parent->fs->sb;


#if CONFIG_DIR_INDEX_ENABLE
	/* Index search */
	if ((ext4_sb_check_feature_compatible(sb,
			EXT4_FEATURE_COMPAT_DIR_INDEX)) &&
			(ext4_inode_has_flag(parent->inode, EXT4_INODE_FLAG_INDEX))) {
		int rc = ext4_dir_dx_find_entry(result, parent, name_len,
				name);

		/* Check if index is not corrupted */
		if (rc != EXT4_ERR_BAD_DX_DIR) {
			if (rc != EOK)
				return rc;

			return EOK;
		}

		/* Needed to clear dir index flag if corrupted */
		ext4_inode_clear_flag(parent->inode, EXT4_INODE_FLAG_INDEX);
		parent->dirty = true;
	}
#endif

	/* Linear algorithm */

	uint32_t iblock;
	uint32_t fblock;
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint32_t inode_size = ext4_inode_get_size(sb, parent->inode);
	uint32_t total_blocks = inode_size / block_size;

	/* Walk through all data blocks */
	for (iblock = 0; iblock < total_blocks; ++iblock) {
		/* Load block address */
		int rc = ext4_fs_get_inode_data_block_index(parent, iblock,
				&fblock);
		if (rc != EOK)
			return rc;

		/* Load data block */
		struct ext4_block block;
		rc = ext4_block_get( parent->fs->bdev, &block, fblock);
		if (rc != EOK)
			return rc;

		/* Try to find entry in block */
		struct ext4_directory_entry_ll *res_entry;
		rc = ext4_dir_find_in_block(&block, sb, name_len, name,
				&res_entry);
		if (rc == EOK) {
			result->block = block;
			result->dentry = res_entry;
			return EOK;
		}

		/* Entry not found - put block and continue to the next block */

		rc = ext4_block_set(parent->fs->bdev, &block);
		if (rc != EOK)
			return rc;
	}

	/* Entry was not found */

	result->block.lb_id =  0;
	result->dentry 		=  NULL;

	return ENOENT;
}

int ext4_dir_remove_entry(struct ext4_inode_ref *parent, const char *name)
{
	/* Check if removing from directory */
	if (!ext4_inode_is_type(&parent->fs->sb, parent->inode,
	    EXT4_INODE_MODE_DIRECTORY))
		return ENOTDIR;

	/* Try to find entry */
	struct ext4_directory_search_result result;
	int rc = ext4_dir_find_entry(&result, parent, name);
	if (rc != EOK)
		return rc;

	/* Invalidate entry */
	ext4_dir_entry_ll_set_inode(result.dentry, 0);

	/* Store entry position in block */
	uint32_t pos = (uint8_t *) result.dentry - result.block.data;

	/*
	 * If entry is not the first in block, it must be merged
	 * with previous entry
	 */
	if (pos != 0) {
		uint32_t offset = 0;

		/* Start from the first entry in block */
		struct ext4_directory_entry_ll *tmp_dentry = (void *)result.block.data;
		uint16_t tmp_dentry_length =
		    ext4_dir_entry_ll_get_entry_length(tmp_dentry);

		/* Find direct predecessor of removed entry */
		while ((offset + tmp_dentry_length) < pos) {
			offset +=
			    ext4_dir_entry_ll_get_entry_length(tmp_dentry);
			tmp_dentry = (void *)(result.block.data + offset);
			tmp_dentry_length =
			    ext4_dir_entry_ll_get_entry_length(tmp_dentry);
		}

		ext4_assert(tmp_dentry_length + offset == pos);

		/* Add to removed entry length to predecessor's length */
		uint16_t del_entry_length =
		    ext4_dir_entry_ll_get_entry_length(result.dentry);
		ext4_dir_entry_ll_set_entry_length(tmp_dentry,
		    tmp_dentry_length + del_entry_length);
	}

	result.block.dirty = true;

	return ext4_dir_destroy_result(parent, &result);
}

int ext4_dir_try_insert_entry(struct ext4_sblock *sb, struct ext4_block *target_block,
    struct ext4_inode_ref *child, const char *name, uint32_t name_len)
{
	/* Compute required length entry and align it to 4 bytes */
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint16_t required_len = sizeof(struct ext4_fake_directory_entry) + name_len;

	if ((required_len % 4) != 0)
		required_len += 4 - (required_len % 4);

	/* Initialize pointers, stop means to upper bound */
	struct ext4_directory_entry_ll *dentry  = (void *)target_block->data;
	struct ext4_directory_entry_ll *stop 	= (void *)(target_block->data + block_size);

	/*
	 * Walk through the block and check for invalid entries
	 * or entries with free space for new entry
	 */
	while (dentry < stop) {
		uint32_t inode = ext4_dir_entry_ll_get_inode(dentry);
		uint16_t rec_len = ext4_dir_entry_ll_get_entry_length(dentry);

		/* If invalid and large enough entry, use it */
		if ((inode == 0) && (rec_len >= required_len)) {
			ext4_dir_write_entry(sb, dentry, rec_len, child,
			    name, name_len);
			target_block->dirty = true;

			return EOK;
		}

		/* Valid entry, try to split it */
		if (inode != 0) {
			uint16_t used_name_len =
			    ext4_dir_entry_ll_get_name_length(sb, dentry);

			uint16_t used_space =
			    sizeof(struct ext4_fake_directory_entry) + used_name_len;

			if ((used_name_len % 4) != 0)
				used_space += 4 - (used_name_len % 4);

			uint16_t free_space = rec_len - used_space;

			/* There is free space for new entry */
			if (free_space >= required_len) {
				/* Cut tail of current entry */
				ext4_dir_entry_ll_set_entry_length(dentry, used_space);
				struct ext4_directory_entry_ll *new_entry =
				    (void *) dentry + used_space;
				ext4_dir_write_entry(sb, new_entry,
				    free_space, child, name, name_len);

				target_block->dirty = true;

				return EOK;
			}
		}

		/* Jump to the next entry */
		dentry = (void *) dentry + rec_len;
	}

	/* No free space found for new entry */
	return ENOSPC;
}


int ext4_dir_find_in_block(struct ext4_block *block, struct ext4_sblock *sb,
		size_t name_len, const char *name, struct ext4_directory_entry_ll **res_entry)
{
	/* Start from the first entry in block */
	struct ext4_directory_entry_ll *dentry =
	    (struct ext4_directory_entry_ll *) block->data;

	/* Set upper bound for cycling */
	uint8_t *addr_limit = block->data + ext4_sb_get_block_size(sb);

	/* Walk through the block and check entries */
	while ((uint8_t *) dentry < addr_limit) {
		/* Termination condition */
		if ((uint8_t *) dentry + name_len > addr_limit)
			break;

		/* Valid entry - check it */
		if (dentry->inode != 0) {
			/* For more effectivity compare firstly only lengths */
			if (ext4_dir_entry_ll_get_name_length(sb, dentry) ==
			    name_len) {
				/* Compare names */
				if (memcmp((uint8_t *) name, dentry->name, name_len) == 0) {
					*res_entry = dentry;
					return EOK;
				}
			}
		}

		uint16_t dentry_len =
		    ext4_dir_entry_ll_get_entry_length(dentry);

		/* Corrupted entry */
		if (dentry_len == 0)
			return EINVAL;

		/* Jump to next entry */
		dentry = (struct ext4_directory_entry_ll *) ((uint8_t *) dentry + dentry_len);
	}

	/* Entry not found */
	return ENOENT;
}

int ext4_dir_destroy_result(struct ext4_inode_ref *parent, struct ext4_directory_search_result *result)
{
	if (result->block.lb_id)
		return ext4_block_set(parent->fs->bdev, &result->block);

	return EOK;
}

/**
 * @}
 */
