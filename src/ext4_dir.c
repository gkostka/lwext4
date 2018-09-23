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
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_debug.h>

#include <ext4_trans.h>
#include <ext4_dir.h>
#include <ext4_dir_idx.h>
#include <ext4_crc32.h>
#include <ext4_inode.h>
#include <ext4_fs.h>

#include <string.h>

/****************************************************************************/

/* Walk through a dirent block to find a checksum "dirent" at the tail */
static struct ext4_dir_entry_tail *
ext4_dir_get_tail(struct ext4_inode_ref *inode_ref,
		struct ext4_dir_en *de)
{
	struct ext4_dir_entry_tail *t;
	struct ext4_sblock *sb = &inode_ref->fs->sb;

	t = EXT4_DIRENT_TAIL(de, ext4_sb_get_block_size(sb));

	if (t->reserved_zero1 || t->reserved_zero2)
		return NULL;
	if (to_le16(t->rec_len) != sizeof(struct ext4_dir_entry_tail))
		return NULL;
	if (t->reserved_ft != EXT4_DIRENTRY_DIR_CSUM)
		return NULL;

	return t;
}

#if CONFIG_META_CSUM_ENABLE
static uint32_t ext4_dir_csum(struct ext4_inode_ref *inode_ref,
			      struct ext4_dir_en *dirent, int size)
{
	uint32_t csum;
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	uint32_t ino_index = to_le32(inode_ref->index);
	uint32_t ino_gen = to_le32(ext4_inode_get_generation(inode_ref->inode));

	/* First calculate crc32 checksum against fs uuid */
	csum = ext4_crc32c(EXT4_CRC32_INIT, sb->uuid, sizeof(sb->uuid));
	/* Then calculate crc32 checksum against inode number
	 * and inode generation */
	csum = ext4_crc32c(csum, &ino_index, sizeof(ino_index));
	csum = ext4_crc32c(csum, &ino_gen, sizeof(ino_gen));
	/* Finally calculate crc32 checksum against directory entries */
	csum = ext4_crc32c(csum, dirent, size);
	return csum;
}
#else
#define ext4_dir_csum(...) 0
#endif

bool ext4_dir_csum_verify(struct ext4_inode_ref *inode_ref,
			      struct ext4_dir_en *dirent)
{
#ifdef CONFIG_META_CSUM_ENABLE
	struct ext4_dir_entry_tail *t;
	struct ext4_sblock *sb = &inode_ref->fs->sb;

	/* Compute the checksum only if the filesystem supports it */
	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		t = ext4_dir_get_tail(inode_ref, dirent);
		if (!t) {
			/* There is no space to hold the checksum */
			return false;
		}

		ptrdiff_t __unused diff = (char *)t - (char *)dirent;
		uint32_t csum = ext4_dir_csum(inode_ref, dirent, diff);
		if (t->checksum != to_le32(csum))
			return false;

	}
#endif
	return true;
}

void ext4_dir_init_entry_tail(struct ext4_dir_entry_tail *t)
{
	memset(t, 0, sizeof(struct ext4_dir_entry_tail));
	t->rec_len = to_le16(sizeof(struct ext4_dir_entry_tail));
	t->reserved_ft = EXT4_DIRENTRY_DIR_CSUM;
}

void ext4_dir_set_csum(struct ext4_inode_ref *inode_ref,
			   struct ext4_dir_en *dirent)
{
	struct ext4_dir_entry_tail *t;
	struct ext4_sblock *sb = &inode_ref->fs->sb;

	/* Compute the checksum only if the filesystem supports it */
	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		t = ext4_dir_get_tail(inode_ref, dirent);
		if (!t) {
			/* There is no space to hold the checksum */
			return;
		}

		ptrdiff_t __unused diff = (char *)t - (char *)dirent;
		uint32_t csum = ext4_dir_csum(inode_ref, dirent, diff);
		t->checksum = to_le32(csum);
	}
}

/**@brief Do some checks before returning iterator.
 * @param it Iterator to be checked
 * @param block_size Size of data block
 * @return Error code
 */
static int ext4_dir_iterator_set(struct ext4_dir_iter *it,
				 uint32_t block_size)
{
	uint32_t off_in_block = it->curr_off % block_size;
	struct ext4_sblock *sb = &it->inode_ref->fs->sb;

	it->curr = NULL;

	/* Ensure proper alignment */
	if ((off_in_block % 4) != 0)
		return EIO;

	/* Ensure that the core of the entry does not overflow the block */
	if (off_in_block > block_size - 8)
		return EIO;

	struct ext4_dir_en *en;
	en = (void *)(it->curr_blk.data + off_in_block);

	/* Ensure that the whole entry does not overflow the block */
	uint16_t length = ext4_dir_en_get_entry_len(en);
	if (off_in_block + length > block_size)
		return EIO;

	/* Ensure the name length is not too large */
	if (ext4_dir_en_get_name_len(sb, en) > length - 8)
		return EIO;

	/* Everything OK - "publish" the entry */
	it->curr = en;
	return EOK;
}

/**@brief Seek to next valid directory entry.
 *        Here can be jumped to the next data block.
 * @param it  Initialized iterator
 * @param pos Position of the next entry
 * @return Error code
 */
static int ext4_dir_iterator_seek(struct ext4_dir_iter *it, uint64_t pos)
{
	struct ext4_sblock *sb = &it->inode_ref->fs->sb;
	struct ext4_inode *inode = it->inode_ref->inode;
	struct ext4_blockdev *bdev = it->inode_ref->fs->bdev;
	uint64_t size = ext4_inode_get_size(sb, inode);
	int r;

	/* The iterator is not valid until we seek to the desired position */
	it->curr = NULL;

	/* Are we at the end? */
	if (pos >= size) {
		if (it->curr_blk.lb_id) {

			r = ext4_block_set(bdev, &it->curr_blk);
			it->curr_blk.lb_id = 0;
			if (r != EOK)
				return r;
		}

		it->curr_off = pos;
		return EOK;
	}

	/* Compute next block address */
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint64_t current_blk_idx = it->curr_off / block_size;
	uint32_t next_blk_idx = (uint32_t)(pos / block_size);

	/*
	 * If we don't have a block or are moving across block boundary,
	 * we need to get another block
	 */
	if ((it->curr_blk.lb_id == 0) ||
	    (current_blk_idx != next_blk_idx)) {
		if (it->curr_blk.lb_id) {
			r = ext4_block_set(bdev, &it->curr_blk);
			it->curr_blk.lb_id = 0;

			if (r != EOK)
				return r;
		}

		ext4_fsblk_t next_blk;
		r = ext4_fs_get_inode_dblk_idx(it->inode_ref, next_blk_idx,
					       &next_blk, false);
		if (r != EOK)
			return r;

		r = ext4_trans_block_get(bdev, &it->curr_blk, next_blk);
		if (r != EOK) {
			it->curr_blk.lb_id = 0;
			return r;
		}
	}

	it->curr_off = pos;
	return ext4_dir_iterator_set(it, block_size);
}

int ext4_dir_iterator_init(struct ext4_dir_iter *it,
			   struct ext4_inode_ref *inode_ref, uint64_t pos)
{
	it->inode_ref = inode_ref;
	it->curr = 0;
	it->curr_off = 0;
	it->curr_blk.lb_id = 0;

	return ext4_dir_iterator_seek(it, pos);
}

int ext4_dir_iterator_next(struct ext4_dir_iter *it)
{
	int r = EOK;
	uint16_t skip;

	while (r == EOK) {
		skip = ext4_dir_en_get_entry_len(it->curr);
		r = ext4_dir_iterator_seek(it, it->curr_off + skip);

		if (!it->curr)
			break;
		/*Skip NULL referenced entry*/
		if (ext4_dir_en_get_inode(it->curr) != 0)
			break;
	}

	return r;
}

int ext4_dir_iterator_fini(struct ext4_dir_iter *it)
{
	it->curr = 0;

	if (it->curr_blk.lb_id)
		return ext4_block_set(it->inode_ref->fs->bdev, &it->curr_blk);

	return EOK;
}

void ext4_dir_write_entry(struct ext4_sblock *sb, struct ext4_dir_en *en,
			  uint16_t entry_len, struct ext4_inode_ref *child,
			  const char *name, size_t name_len)
{
	/* Check maximum entry length */
	ext4_assert(entry_len <= ext4_sb_get_block_size(sb));

	/* Set type of entry */
	switch (ext4_inode_type(sb, child->inode)) {
	case EXT4_INODE_MODE_DIRECTORY:
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_DIR);
		break;
	case EXT4_INODE_MODE_FILE:
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_REG_FILE);
		break;
	case EXT4_INODE_MODE_SOFTLINK:
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_SYMLINK);
		break;
	case EXT4_INODE_MODE_CHARDEV:
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_CHRDEV);
		break;
	case EXT4_INODE_MODE_BLOCKDEV:
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_BLKDEV);
		break;
	case EXT4_INODE_MODE_FIFO:
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_FIFO);
		break;
	case EXT4_INODE_MODE_SOCKET:
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_SOCK);
		break;
	default:
		/* FIXME: unsupported filetype */
		ext4_dir_en_set_inode_type(sb, en, EXT4_DE_UNKNOWN);
	}

	/* Set basic attributes */
	ext4_dir_en_set_inode(en, child->index);
	ext4_dir_en_set_entry_len(en, entry_len);
	ext4_dir_en_set_name_len(sb, en, (uint16_t)name_len);

	/* Write name */
	memcpy(en->name, name, name_len);
}

int ext4_dir_add_entry(struct ext4_inode_ref *parent, const char *name,
		       uint32_t name_len, struct ext4_inode_ref *child)
{
	int r;
	struct ext4_fs *fs = parent->fs;
	struct ext4_sblock *sb = &parent->fs->sb;

#if CONFIG_DIR_INDEX_ENABLE
	/* Index adding (if allowed) */
	if ((ext4_sb_feature_com(sb, EXT4_FCOM_DIR_INDEX)) &&
	    (ext4_inode_has_flag(parent->inode, EXT4_INODE_FLAG_INDEX))) {
		r = ext4_dir_dx_add_entry(parent, child, name, name_len);

		/* Check if index is not corrupted */
		if (r != EXT4_ERR_BAD_DX_DIR) {
			if (r != EOK)
				return r;

			return EOK;
		}

		/* Needed to clear dir index flag if corrupted */
		ext4_inode_clear_flag(parent->inode, EXT4_INODE_FLAG_INDEX);
		parent->dirty = true;
	}
#endif

	/* Linear algorithm */
	uint32_t iblock = 0;
	ext4_fsblk_t fblock = 0;
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint64_t inode_size = ext4_inode_get_size(sb, parent->inode);
	uint32_t total_blocks = (uint32_t)(inode_size / block_size);

	/* Find block, where is space for new entry and try to add */
	bool success = false;
	for (iblock = 0; iblock < total_blocks; ++iblock) {
		r = ext4_fs_get_inode_dblk_idx(parent, iblock, &fblock, false);
		if (r != EOK)
			return r;

		struct ext4_block block;
		r = ext4_trans_block_get(fs->bdev, &block, fblock);
		if (r != EOK)
			return r;

		if (!ext4_dir_csum_verify(parent, (void *)block.data)) {
			ext4_dbg(DEBUG_DIR,
				 DBG_WARN "Leaf block checksum failed."
				 "Inode: %" PRIu32", "
				 "Block: %" PRIu32"\n",
				 parent->index,
				 iblock);
		}

		/* If adding is successful, function can finish */
		r = ext4_dir_try_insert_entry(sb, parent, &block, child,
						name, name_len);
		if (r == EOK)
			success = true;

		r = ext4_block_set(fs->bdev, &block);
		if (r != EOK)
			return r;

		if (success)
			return EOK;
	}

	/* No free block found - needed to allocate next data block */

	iblock = 0;
	fblock = 0;
	r = ext4_fs_append_inode_dblk(parent, &fblock, &iblock);
	if (r != EOK)
		return r;

	/* Load new block */
	struct ext4_block b;

	r = ext4_trans_block_get_noread(fs->bdev, &b, fblock);
	if (r != EOK)
		return r;

	/* Fill block with zeroes */
	memset(b.data, 0, block_size);
	struct ext4_dir_en *blk_en = (void *)b.data;

	/* Save new block */
	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		uint16_t el = block_size - sizeof(struct ext4_dir_entry_tail);
		ext4_dir_write_entry(sb, blk_en, el, child, name, name_len);
		ext4_dir_init_entry_tail(EXT4_DIRENT_TAIL(b.data, block_size));
	} else {
		ext4_dir_write_entry(sb, blk_en, block_size, child, name,
				name_len);
	}

	ext4_dir_set_csum(parent, (void *)b.data);
	ext4_trans_set_block_dirty(b.buf);
	r = ext4_block_set(fs->bdev, &b);

	return r;
}

int ext4_dir_find_entry(struct ext4_dir_search_result *result,
			struct ext4_inode_ref *parent, const char *name,
			uint32_t name_len)
{
	int r;
	struct ext4_sblock *sb = &parent->fs->sb;

	/* Entry clear */
	result->block.lb_id = 0;
	result->dentry = NULL;

#if CONFIG_DIR_INDEX_ENABLE
	/* Index search */
	if ((ext4_sb_feature_com(sb, EXT4_FCOM_DIR_INDEX)) &&
	    (ext4_inode_has_flag(parent->inode, EXT4_INODE_FLAG_INDEX))) {
		r = ext4_dir_dx_find_entry(result, parent, name_len, name);
		/* Check if index is not corrupted */
		if (r != EXT4_ERR_BAD_DX_DIR) {
			if (r != EOK)
				return r;

			return EOK;
		}

		/* Needed to clear dir index flag if corrupted */
		ext4_inode_clear_flag(parent->inode, EXT4_INODE_FLAG_INDEX);
		parent->dirty = true;
	}
#endif

	/* Linear algorithm */

	uint32_t iblock;
	ext4_fsblk_t fblock;
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint64_t inode_size = ext4_inode_get_size(sb, parent->inode);
	uint32_t total_blocks = (uint32_t)(inode_size / block_size);

	/* Walk through all data blocks */
	for (iblock = 0; iblock < total_blocks; ++iblock) {
		/* Load block address */
		r = ext4_fs_get_inode_dblk_idx(parent, iblock, &fblock, false);
		if (r != EOK)
			return r;

		/* Load data block */
		struct ext4_block b;
		r = ext4_trans_block_get(parent->fs->bdev, &b, fblock);
		if (r != EOK)
			return r;

		if (!ext4_dir_csum_verify(parent, (void *)b.data)) {
			ext4_dbg(DEBUG_DIR,
				 DBG_WARN "Leaf block checksum failed."
				 "Inode: %" PRIu32", "
				 "Block: %" PRIu32"\n",
				 parent->index,
				 iblock);
		}

		/* Try to find entry in block */
		struct ext4_dir_en *res_entry;
		r = ext4_dir_find_in_block(&b, sb, name_len, name, &res_entry);
		if (r == EOK) {
			result->block = b;
			result->dentry = res_entry;
			return EOK;
		}

		/* Entry not found - put block and continue to the next block */

		r = ext4_block_set(parent->fs->bdev, &b);
		if (r != EOK)
			return r;
	}

	return ENOENT;
}

int ext4_dir_remove_entry(struct ext4_inode_ref *parent, const char *name,
			  uint32_t name_len)
{
	struct ext4_sblock *sb = &parent->fs->sb;
	/* Check if removing from directory */
	if (!ext4_inode_is_type(sb, parent->inode, EXT4_INODE_MODE_DIRECTORY))
		return ENOTDIR;

	/* Try to find entry */
	struct ext4_dir_search_result result;
	int rc = ext4_dir_find_entry(&result, parent, name, name_len);
	if (rc != EOK)
		return rc;

	/* Invalidate entry */
	ext4_dir_en_set_inode(result.dentry, 0);

	/* Store entry position in block */
	uint32_t pos = (uint8_t *)result.dentry - result.block.data;

	/*
	 * If entry is not the first in block, it must be merged
	 * with previous entry
	 */
	if (pos != 0) {
		uint32_t offset = 0;

		/* Start from the first entry in block */
		struct ext4_dir_en *tmp_de =(void *)result.block.data;
		uint16_t de_len = ext4_dir_en_get_entry_len(tmp_de);

		/* Find direct predecessor of removed entry */
		while ((offset + de_len) < pos) {
			offset += ext4_dir_en_get_entry_len(tmp_de);
			tmp_de = (void *)(result.block.data + offset);
			de_len = ext4_dir_en_get_entry_len(tmp_de);
		}

		ext4_assert(de_len + offset == pos);

		/* Add to removed entry length to predecessor's length */
		uint16_t del_len;
		del_len = ext4_dir_en_get_entry_len(result.dentry);
		ext4_dir_en_set_entry_len(tmp_de, de_len + del_len);
	}

	ext4_dir_set_csum(parent,
			(struct ext4_dir_en *)result.block.data);
	ext4_trans_set_block_dirty(result.block.buf);

	return ext4_dir_destroy_result(parent, &result);
}

int ext4_dir_try_insert_entry(struct ext4_sblock *sb,
			      struct ext4_inode_ref *inode_ref,
			      struct ext4_block *dst_blk,
			      struct ext4_inode_ref *child, const char *name,
			      uint32_t name_len)
{
	/* Compute required length entry and align it to 4 bytes */
	uint32_t block_size = ext4_sb_get_block_size(sb);
	uint16_t required_len = sizeof(struct ext4_fake_dir_entry) + name_len;

	if ((required_len % 4) != 0)
		required_len += 4 - (required_len % 4);

	/* Initialize pointers, stop means to upper bound */
	struct ext4_dir_en *start = (void *)dst_blk->data;
	struct ext4_dir_en *stop = (void *)(dst_blk->data + block_size);

	/*
	 * Walk through the block and check for invalid entries
	 * or entries with free space for new entry
	 */
	while (start < stop) {
		uint32_t inode = ext4_dir_en_get_inode(start);
		uint16_t rec_len = ext4_dir_en_get_entry_len(start);
		uint8_t itype = ext4_dir_en_get_inode_type(sb, start);

		/* If invalid and large enough entry, use it */
		if ((inode == 0) && (itype != EXT4_DIRENTRY_DIR_CSUM) &&
		    (rec_len >= required_len)) {
			ext4_dir_write_entry(sb, start, rec_len, child, name,
					     name_len);
			ext4_dir_set_csum(inode_ref, (void *)dst_blk->data);
			ext4_trans_set_block_dirty(dst_blk->buf);

			return EOK;
		}

		/* Valid entry, try to split it */
		if (inode != 0) {
			uint16_t used_len;
			used_len = ext4_dir_en_get_name_len(sb, start);

			uint16_t sz;
			sz = sizeof(struct ext4_fake_dir_entry) + used_len;

			if ((used_len % 4) != 0)
				sz += 4 - (used_len % 4);

			uint16_t free_space = rec_len - sz;

			/* There is free space for new entry */
			if (free_space >= required_len) {
				/* Cut tail of current entry */
				struct ext4_dir_en * new_entry;
				new_entry = (void *)((uint8_t *)start + sz);
				ext4_dir_en_set_entry_len(start, sz);
				ext4_dir_write_entry(sb, new_entry, free_space,
						     child, name, name_len);

				ext4_dir_set_csum(inode_ref,
						  (void *)dst_blk->data);
				ext4_trans_set_block_dirty(dst_blk->buf);
				return EOK;
			}
		}

		/* Jump to the next entry */
		start = (void *)((uint8_t *)start + rec_len);
	}

	/* No free space found for new entry */
	return ENOSPC;
}

int ext4_dir_find_in_block(struct ext4_block *block, struct ext4_sblock *sb,
			   size_t name_len, const char *name,
			   struct ext4_dir_en **res_entry)
{
	/* Start from the first entry in block */
	struct ext4_dir_en *de = (struct ext4_dir_en *)block->data;

	/* Set upper bound for cycling */
	uint8_t *addr_limit = block->data + ext4_sb_get_block_size(sb);

	/* Walk through the block and check entries */
	while ((uint8_t *)de < addr_limit) {
		/* Termination condition */
		if ((uint8_t *)de + name_len > addr_limit)
			break;

		/* Valid entry - check it */
		if (ext4_dir_en_get_inode(de) != 0) {
			/* For more efficient compare only lengths firstly*/
			uint16_t el = ext4_dir_en_get_name_len(sb, de);
			if (el == name_len) {
				/* Compare names */
				if (memcmp(name, de->name, name_len) == 0) {
					*res_entry = de;
					return EOK;
				}
			}
		}

		uint16_t de_len = ext4_dir_en_get_entry_len(de);

		/* Corrupted entry */
		if (de_len == 0)
			return EINVAL;

		/* Jump to next entry */
		de = (struct ext4_dir_en *)((uint8_t *)de + de_len);
	}

	/* Entry not found */
	return ENOENT;
}

int ext4_dir_destroy_result(struct ext4_inode_ref *parent,
			    struct ext4_dir_search_result *result)
{
	if (result->block.lb_id)
		return ext4_block_set(parent->fs->bdev, &result->block);

	return EOK;
}

/**
 * @}
 */
