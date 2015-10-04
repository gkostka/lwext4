/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * Copyright (c) 2015 Kaho Ng (ngkaho1234@gmail.com)
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
 * @file  ext4_xattr.c
 * @brief Extended Attribute manipulation.
 */

#include "ext4_config.h"
#include "ext4_types.h"
#include "ext4_fs.h"
#include "ext4_errno.h"
#include "ext4_blockdev.h"
#include "ext4_super.h"
#include "ext4_debug.h"
#include "ext4_block_group.h"
#include "ext4_balloc.h"
#include "ext4_inode.h"
#include "ext4_extent.h"

#include <string.h>
#include <stdlib.h>

/**
 * @file  ext4_xattr.c
 * @brief Extended Attribute Manipulation
 */
static int ext4_xattr_item_cmp(struct ext4_xattr_item *a,
				struct ext4_xattr_item *b)
{
	int result;
	result = a->name_index - b->name_index;
	if (result)
		return result;

	result = a->name_len - b->name_len;
	if (result)
		return result;

	return memcmp(a->name, b->name, a->name_len);
}

RB_GENERATE_INTERNAL(ext4_xattr_tree,
		     ext4_xattr_item,
		     node,
		     ext4_xattr_item_cmp,
		     static inline)

static struct ext4_xattr_item *
ext4_xattr_item_alloc(uint8_t name_index,
		      char   *name,
		      size_t  name_len)
{
	struct ext4_xattr_item *item;
	item = malloc(sizeof(struct ext4_xattr_item) +
			name_len);
	if (!item)
		return NULL;

	item->name_index = name_index;
	item->name       = (char *)(item + 1);
	item->name_len   = name_len;
	item->data       = NULL;
	item->data_size  = 0;
	
	memset(&item->node, 0, sizeof(item->node));
	memcpy(item->name, name, name_len);

	return item;
}

static int
ext4_xattr_item_alloc_data(struct ext4_xattr_item *item,
			   void *orig_data,
			   size_t data_size)
{
	void *data = NULL;
	ext4_assert(!item->data);
	data = malloc(data_size);
	if (!data)
		return ENOMEM;

	if (orig_data)
		memcpy(data, orig_data, data_size);

	item->data = data;
	item->data_size = data_size;
	return EOK;
}

static void
ext4_xattr_item_free_data(struct ext4_xattr_item *item)
{
	ext4_assert(item->data);
	free(item->data);
	item->data = NULL;
	item->data_size = 0;
}

static int
ext4_xattr_item_resize_data(struct ext4_xattr_item *item,
			    size_t new_data_size)
{
	if (new_data_size != item->data_size) {
		void *new_data;
		new_data = realloc(item->data, new_data_size);
		if (!new_data)
			return ENOMEM;

		item->data = new_data;
		item->data_size = new_data_size;
	}
	return EOK;
}

static void
ext4_xattr_item_free(struct ext4_xattr_item *item)
{
	if (item->data)
		ext4_xattr_item_free_data(item);

	free(item);
}


static void *ext4_xattr_entry_data(struct ext4_xattr_ref *xattr_ref,
				   struct ext4_xattr_entry *entry,
				   bool in_inode)
{
	void *ret;
	if (in_inode) {
		struct ext4_xattr_ibody_header *header;
		struct ext4_xattr_entry *first_entry;
		uint16_t inode_size = ext4_get16(&xattr_ref->fs->sb,
						 inode_size);
		header = EXT4_XATTR_IHDR(xattr_ref->inode_ref->inode);
		first_entry = EXT4_XATTR_IFIRST(header);
 
		ret = (void *)((char *)first_entry + to_le16(entry->e_value_offs));
		if ((char *)ret + EXT4_XATTR_SIZE(to_le32(entry->e_value_size))
			- (char *)xattr_ref->inode_ref->inode >
		    inode_size)
			ret = NULL;

	} else {
		uint32_t block_size =
				ext4_sb_get_block_size(&xattr_ref->fs->sb);
		ret = (void *)((char *)xattr_ref->block.data + 
				to_le16(entry->e_value_offs));
		if ((char *)ret + EXT4_XATTR_SIZE(to_le32(entry->e_value_size))
			- (char *)xattr_ref->block.data >
		    block_size)
			ret = NULL;
	}
	return ret;
}

static int ext4_xattr_block_fetch(struct ext4_xattr_ref *xattr_ref)
{
	int ret = EOK;
	size_t size_rem;
	void *data;
	struct ext4_xattr_entry *entry = NULL;

	ext4_assert(xattr_ref->block.data);
	entry = EXT4_XATTR_BFIRST(&xattr_ref->block);

	size_rem = ext4_sb_get_block_size(&xattr_ref->fs->sb);
	for(;size_rem > 0 && !EXT4_XATTR_IS_LAST_ENTRY(entry);
	    entry = EXT4_XATTR_NEXT(entry),
	    size_rem -= EXT4_XATTR_LEN(entry->e_name_len)) {
		struct ext4_xattr_item *item;
		char *e_name = (char *)(entry + 1);

		data = ext4_xattr_entry_data(xattr_ref, entry,
					     false);
		if (!data) {
			ret = EIO;
			goto Finish;
		}

		item = ext4_xattr_item_alloc(entry->e_name_index,
					     e_name,
					     (size_t)entry->e_name_len);
		if (!item) {
			ret = ENOMEM;
			goto Finish;
		}
		if (ext4_xattr_item_alloc_data(item,
					       data,
					       to_le32(entry->e_value_size))
			!= EOK) {
			ext4_xattr_item_free(item);
			ret = ENOMEM;
			goto Finish;
		}
		RB_INSERT(ext4_xattr_tree, &xattr_ref->root, item);
		xattr_ref->ea_size += item->data_size;
	}

Finish:
	return ret;
}

static int ext4_xattr_inode_fetch(struct ext4_xattr_ref *xattr_ref)
{
	void *data;
	size_t size_rem;
	int ret = EOK;
	struct ext4_xattr_ibody_header *header = NULL;
	struct ext4_xattr_entry *entry = NULL;
	uint16_t inode_size = ext4_get16(&xattr_ref->fs->sb,
					 inode_size);

	header = EXT4_XATTR_IHDR(xattr_ref->inode_ref->inode);
	entry = EXT4_XATTR_IFIRST(header);

	size_rem = inode_size -
		EXT4_GOOD_OLD_INODE_SIZE -
		xattr_ref->inode_ref->inode->extra_isize;
	for(;size_rem > 0 && !EXT4_XATTR_IS_LAST_ENTRY(entry);
	    entry = EXT4_XATTR_NEXT(entry),
	    size_rem -= EXT4_XATTR_LEN(entry->e_name_len)) {
		struct ext4_xattr_item *item;
		char *e_name = (char *)(entry + 1);

		data = ext4_xattr_entry_data(xattr_ref, entry,
					     true);
		if (!data) {
			ret = EIO;
			goto Finish;
		}

		item = ext4_xattr_item_alloc(entry->e_name_index,
					     e_name,
					     (size_t)entry->e_name_len);
		if (!item) {
			ret = ENOMEM;
			goto Finish;
		}
		if (ext4_xattr_item_alloc_data(item,
					       data,
					       to_le32(entry->e_value_size))
			!= EOK) {
			ext4_xattr_item_free(item);
			ret = ENOMEM;
			goto Finish;
		}
		RB_INSERT(ext4_xattr_tree, &xattr_ref->root, item);
		xattr_ref->ea_size += item->data_size;
	}

Finish:
	return ret;
}

static int ext4_xattr_fetch(struct ext4_xattr_ref *xattr_ref)
{
	int ret = EOK;
	uint16_t inode_size = ext4_get16(&xattr_ref->fs->sb,
					 inode_size);
	if (inode_size > EXT4_GOOD_OLD_INODE_SIZE) {
		ret = ext4_xattr_inode_fetch(xattr_ref);
		if (ret != EOK)
			return ret;

	}

	if (xattr_ref->block_loaded)
		ret = ext4_xattr_block_fetch(xattr_ref);

	xattr_ref->dirty = false;
	return ret;
}

static void
ext4_xattr_lookup_items(struct ext4_xattr_ref *xattr_ref,
			uint8_t name_index,
			char   *name,
			size_t  name_len)
{

}

static void
ext4_xattr_purge_items(struct ext4_xattr_ref *xattr_ref)
{
	struct ext4_xattr_item *item, *save_item;
	RB_FOREACH_SAFE(item,
			ext4_xattr_tree,
			&xattr_ref->root,
			save_item) {
		RB_REMOVE(ext4_xattr_tree, &xattr_ref->root, item);
		ext4_xattr_item_free(item);
	}
	xattr_ref->ea_size = 0;
}

int ext4_fs_get_xattr_ref(struct ext4_fs *fs,
			  struct ext4_inode_ref *inode_ref,
			  struct ext4_xattr_ref *ref)
{
	int rc;
	uint64_t xattr_block;
	xattr_block = ext4_inode_get_file_acl(inode_ref->inode,
					      &fs->sb);
	RB_INIT(&ref->root);
	if (xattr_block) {
		rc = ext4_block_get(fs->bdev,
				    &inode_ref->block, xattr_block);
		if (rc != EOK)
			return EIO;
	
		ref->block_loaded = true;
	} else
		ref->block_loaded = false;

	ref->inode_ref = inode_ref;
	ref->fs = fs;

	rc = ext4_xattr_fetch(ref);
	if (rc != EOK) {
		ext4_xattr_purge_items(ref);
		if (xattr_block)
			ext4_block_set(fs->bdev, &inode_ref->block);

		ref->block_loaded = false;
		return rc;
	}
	return EOK;
}

void ext4_fs_put_xattr_ref(struct ext4_xattr_ref *ref)
{
	if (ref->block_loaded) {
		ext4_block_set(ref->fs->bdev, &ref->block);
		ref->block_loaded = false;
	}
	ext4_xattr_purge_items(ref);
	ref->inode_ref = NULL;
	ref->fs = NULL;
}

/**
 * @}
 */
