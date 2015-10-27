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
 * @file  ext4_extent.c
 * @brief More complex filesystem functions.
 */

#include "ext4_config.h"
#include "ext4_extent.h"
#include "ext4_inode.h"
#include "ext4_super.h"
#include "ext4_crc32c.h"
#include "ext4_blockdev.h"
#include "ext4_balloc.h"
#include "ext4_fs.h"

#include <string.h>
#include <stdlib.h>

#if !CONFIG_EXTENT_FULL

/**@brief Binary search in extent index node.
 * @param header Extent header of index node
 * @param index  Output value - found index will be set here
 * @param iblock Logical block number to find in index node */
static void ext4_extent_binsearch_idx(struct ext4_extent_header *header,
				      struct ext4_extent_index **index,
				      uint32_t iblock)
{
	struct ext4_extent_index *r;
	struct ext4_extent_index *l;
	struct ext4_extent_index *m;

	uint16_t entries_count = ext4_extent_header_get_entries_count(header);

	/* Initialize bounds */
	l = EXT4_EXTENT_FIRST_INDEX(header) + 1;
	r = EXT4_EXTENT_FIRST_INDEX(header) + entries_count - 1;

	/* Do binary search */
	while (l <= r) {
		m = l + (r - l) / 2;
		uint32_t first_block = ext4_extent_index_get_first_block(m);

		if (iblock < first_block)
			r = m - 1;
		else
			l = m + 1;
	}

	/* Set output value */
	*index = l - 1;
}

/**@brief Binary search in extent leaf node.
 * @param header Extent header of leaf node
 * @param extent Output value - found extent will be set here,
 *               or NULL if node is empty
 * @param iblock Logical block number to find in leaf node */
static void ext4_extent_binsearch(struct ext4_extent_header *header,
				  struct ext4_extent **extent, uint32_t iblock)
{
	struct ext4_extent *r;
	struct ext4_extent *l;
	struct ext4_extent *m;

	uint16_t entries_count = ext4_extent_header_get_entries_count(header);

	if (entries_count == 0) {
		/* this leaf is empty */
		*extent = NULL;
		return;
	}

	/* Initialize bounds */
	l = EXT4_EXTENT_FIRST(header) + 1;
	r = EXT4_EXTENT_FIRST(header) + entries_count - 1;

	/* Do binary search */
	while (l <= r) {
		m = l + (r - l) / 2;
		uint32_t first_block = ext4_extent_get_first_block(m);

		if (iblock < first_block)
			r = m - 1;
		else
			l = m + 1;
	}

	/* Set output value */
	*extent = l - 1;
}

static uint32_t ext4_ext_block_csum(struct ext4_inode_ref *inode_ref,
				    struct ext4_extent_header *eh)
{
	uint32_t checksum = 0;
	struct ext4_sblock *sb = &inode_ref->fs->sb;

	if (ext4_sb_has_feature_read_only(sb,
				EXT4_FEATURE_RO_COMPAT_METADATA_CSUM)) {
		uint32_t ino_index = to_le32(inode_ref->index);
		uint32_t ino_gen =
			to_le32(ext4_inode_get_generation(inode_ref->inode));
		/* First calculate crc32 checksum against fs uuid */
		checksum = ext4_crc32c(~0, sb->uuid, sizeof(sb->uuid));
		/* Then calculate crc32 checksum against inode number
		 * and inode generation */
		checksum = ext4_crc32c(checksum, &ino_index,
				     sizeof(ino_index));
		checksum = ext4_crc32c(checksum, &ino_gen,
				     sizeof(ino_gen));
		/* Finally calculate crc32 checksum against 
		 * the entire extent block up to the checksum field */
		checksum = ext4_crc32c(checksum, eh,
				EXT4_EXTENT_TAIL_OFFSET(eh));
	}
	return checksum;
}

static struct ext4_extent_header *ext_block_hdr(struct ext4_block *block)
{
	return (struct ext4_extent_header *)block->data;
}

static struct ext4_extent_tail *
find_ext4_extent_tail(struct ext4_extent_header *eh)
{
	return (struct ext4_extent_tail *)(((char *)eh) +
					   EXT4_EXTENT_TAIL_OFFSET(eh));
}

/*
 * BIG FAT NOTES:
 *       Currently we do not verify the checksum of extent
 *       (only the case in ext4_extent.c)
 */

static void ext4_extent_block_csum_set(struct ext4_inode_ref *inode_ref,
				       struct ext4_extent_header *eh)
{
	struct ext4_extent_tail *tail;

	tail = find_ext4_extent_tail(eh);
	tail->et_checksum = to_le32(ext4_ext_block_csum(inode_ref, eh));
}

/**@brief Get physical block in the extent tree by logical block number.
 * There is no need to save path in the tree during this algorithm.
 * @param inode_ref I-node to load block from
 * @param iblock    Logical block number to find
 * @param fblock    Output value for physical block number
 * @return Error code*/
static int
ext4_extent_find_block(struct ext4_inode_ref *inode_ref, uint32_t iblock,
			   ext4_fsblk_t *fblock)
{
	int rc;
	/* Compute bound defined by i-node size */
	uint64_t inode_size =
	    ext4_inode_get_size(&inode_ref->fs->sb, inode_ref->inode);

	uint32_t block_size = ext4_sb_get_block_size(&inode_ref->fs->sb);

	uint32_t last_idx = (inode_size - 1) / block_size;

	/* Check if requested iblock is not over size of i-node */
	if (iblock > last_idx) {
		*fblock = 0;
		return EOK;
	}

	struct ext4_block block;
	block.lb_id = 0;

	/* Walk through extent tree */
	struct ext4_extent_header *header =
	    ext4_inode_get_extent_header(inode_ref->inode);

	while (ext4_extent_header_get_depth(header) != 0) {
		/* Search index in node */
		struct ext4_extent_index *index;
		ext4_extent_binsearch_idx(header, &index, iblock);

		/* Load child node and set values for the next iteration */
		uint64_t child = ext4_extent_index_get_leaf(index);

		if (block.lb_id) {
			rc = ext4_block_set(inode_ref->fs->bdev, &block);
			if (rc != EOK)
				return rc;
		}

		int rc = ext4_block_get(inode_ref->fs->bdev, &block, child);
		if (rc != EOK)
			return rc;

		header = (struct ext4_extent_header *)block.data;
	}

	/* Search extent in the leaf block */
	struct ext4_extent *extent = NULL;
	ext4_extent_binsearch(header, &extent, iblock);

	/* Prevent empty leaf */
	if (extent == NULL) {
		*fblock = 0;
	} else {
		/* Compute requested physical block address */
		ext4_fsblk_t phys_block;
		uint32_t first = ext4_extent_get_first_block(extent);
		phys_block = ext4_extent_get_start(extent) + iblock - first;

		*fblock = phys_block;
	}

	/* Cleanup */
	if (block.lb_id) {
		rc = ext4_block_set(inode_ref->fs->bdev, &block);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/**@brief Find extent for specified iblock.
 * This function is used for finding block in the extent tree with
 * saving the path through the tree for possible future modifications.
 * @param inode_ref I-node to read extent tree from
 * @param iblock    Iblock to find extent for
 * @param ret_path  Output value for loaded path from extent tree
 * @return Error code */
static int ext4_extent_find_extent(struct ext4_inode_ref *inode_ref,
				   uint32_t iblock,
				   struct ext4_extent_path **ret_path)
{
	struct ext4_extent_header *eh =
	    ext4_inode_get_extent_header(inode_ref->inode);

	uint16_t depth = ext4_extent_header_get_depth(eh);
	uint16_t i;
	struct ext4_extent_path *tmp_path;

	/* Added 2 for possible tree growing */
	tmp_path = malloc(sizeof(struct ext4_extent_path) * (depth + 2));
	if (tmp_path == NULL)
		return ENOMEM;

	/* Initialize structure for algorithm start */
	tmp_path[0].block = inode_ref->block;
	tmp_path[0].header = eh;

	/* Walk through the extent tree */
	uint16_t pos = 0;
	int rc;
	while (ext4_extent_header_get_depth(eh) != 0) {
		/* Search index in index node by iblock */
		ext4_extent_binsearch_idx(tmp_path[pos].header,
					  &tmp_path[pos].index, iblock);

		tmp_path[pos].depth = depth;
		tmp_path[pos].extent = NULL;

		ext4_assert(tmp_path[pos].index != 0);

		/* Load information for the next iteration */
		uint64_t fblock =
		    ext4_extent_index_get_leaf(tmp_path[pos].index);

		struct ext4_block block;
		rc = ext4_block_get(inode_ref->fs->bdev, &block, fblock);
		if (rc != EOK)
			goto cleanup;

		pos++;

		eh = (struct ext4_extent_header *)block.data;
		tmp_path[pos].block = block;
		tmp_path[pos].header = eh;
	}

	tmp_path[pos].depth = 0;
	tmp_path[pos].extent = NULL;
	tmp_path[pos].index = NULL;

	/* Find extent in the leaf node */
	ext4_extent_binsearch(tmp_path[pos].header, &tmp_path[pos].extent,
			      iblock);
	*ret_path = tmp_path;

	return EOK;

cleanup:
	/*
	 * Put loaded blocks
	 * From 1: 0 is a block with inode data
	 */
	for (i = 1; i < tmp_path->depth; ++i) {
		if (tmp_path[i].block.lb_id) {
			int r = ext4_block_set(inode_ref->fs->bdev,
					       &tmp_path[i].block);
			if (r != EOK)
				rc = r;
		}
	}

	/* Destroy temporary data structure */
	free(tmp_path);

	return rc;
}

/**@brief Release extent and all data blocks covered by the extent.
 * @param inode_ref I-node to release extent and block from
 * @param extent    Extent to release
 * @return Error code */
static int ext4_extent_release(struct ext4_inode_ref *inode_ref,
			       struct ext4_extent *extent)
{
	/* Compute number of the first physical block to release */
	uint64_t start = ext4_extent_get_start(extent);
	uint16_t block_count = ext4_extent_get_block_count(extent);

	return ext4_balloc_free_blocks(inode_ref, start, block_count);
}

/** Recursively release the whole branch of the extent tree.
 * For each entry of the node release the subbranch and finally release
 * the node. In the leaf node all extents will be released.
 * @param inode_ref I-node where the branch is released
 * @param index     Index in the non-leaf node to be released
 *                  with the whole subtree
 * @return Error code */
static int ext4_extent_release_branch(struct ext4_inode_ref *inode_ref,
				      struct ext4_extent_index *index)
{
	ext4_fsblk_t fblock = ext4_extent_index_get_leaf(index);
	uint32_t i;
	struct ext4_block block;
	int rc = ext4_block_get(inode_ref->fs->bdev, &block, fblock);
	if (rc != EOK)
		return rc;

	struct ext4_extent_header *header = (void *)block.data;

	if (ext4_extent_header_get_depth(header)) {
		/* The node is non-leaf, do recursion */
		struct ext4_extent_index *idx = EXT4_EXTENT_FIRST_INDEX(header);

		/* Release all subbranches */
		for (i = 0; i < ext4_extent_header_get_entries_count(header);
		     ++i, ++idx) {
			rc = ext4_extent_release_branch(inode_ref, idx);
			if (rc != EOK)
				return rc;
		}
	} else {
		/* Leaf node reached */
		struct ext4_extent *ext = EXT4_EXTENT_FIRST(header);

		/* Release all extents and stop recursion */
		for (i = 0; i < ext4_extent_header_get_entries_count(header);
		     ++i, ++ext) {
			rc = ext4_extent_release(inode_ref, ext);
			if (rc != EOK)
				return rc;
		}
	}

	/* Release data block where the node was stored */

	rc = ext4_block_set(inode_ref->fs->bdev, &block);
	if (rc != EOK)
		return rc;

	return ext4_balloc_free_block(inode_ref, fblock);
}

int ext4_extent_remove_space(struct ext4_inode_ref *inode_ref, ext4_lblk_t from,
			     ext4_lblk_t to)
{
	if (to != EXT_MAX_BLOCKS)
		return ENOTSUP;

	/* Find the first extent to modify */
	struct ext4_extent_path *path;
	uint16_t i;
	int rc = ext4_extent_find_extent(inode_ref, from, &path);
	if (rc != EOK)
		return rc;

	/* Jump to last item of the path (extent) */
	struct ext4_extent_path *path_ptr = path;
	while (path_ptr->depth != 0)
		path_ptr++;

	ext4_assert(path_ptr->extent != NULL);

	/* First extent maybe released partially */
	uint32_t first_iblock = ext4_extent_get_first_block(path_ptr->extent);
	ext4_fsblk_t first_fblock = ext4_extent_get_start(path_ptr->extent) +
				from - first_iblock;

	uint16_t block_count = ext4_extent_get_block_count(path_ptr->extent);

	uint16_t delete_count =
	    block_count -
	    (ext4_extent_get_start(path_ptr->extent) - first_fblock);

	/* Release all blocks */
	rc = ext4_balloc_free_blocks(inode_ref, first_fblock, delete_count);
	if (rc != EOK)
		goto cleanup;

	/* Correct counter */
	block_count -= delete_count;
	ext4_extent_set_block_count(path_ptr->extent, block_count,
				EXT4_EXT_IS_UNWRITTEN(path_ptr->extent));

	/* Initialize the following loop */
	uint16_t entries =
	    ext4_extent_header_get_entries_count(path_ptr->header);
	struct ext4_extent *tmp_ext = path_ptr->extent + 1;
	struct ext4_extent *stop_ext =
	    EXT4_EXTENT_FIRST(path_ptr->header) + entries;

	/* If first extent empty, release it */
	if (block_count == 0)
		entries--;

	/* Release all successors of the first extent in the same node */
	while (tmp_ext < stop_ext) {
		first_fblock = ext4_extent_get_start(tmp_ext);
		delete_count = ext4_extent_get_block_count(tmp_ext);

		rc = ext4_balloc_free_blocks(inode_ref, first_fblock,
					     delete_count);
		if (rc != EOK)
			goto cleanup;

		entries--;
		tmp_ext++;
	}

	ext4_extent_header_set_entries_count(path_ptr->header, entries);
	ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&path_ptr->block));
	path_ptr->block.dirty = true;

	/* If leaf node is empty, parent entry must be modified */
	bool remove_parent_record = false;

	/* Don't release root block (including inode data) !!! */
	if ((path_ptr != path) && (entries == 0)) {
		rc = ext4_balloc_free_block(inode_ref, path_ptr->block.lb_id);
		if (rc != EOK)
			goto cleanup;

		remove_parent_record = true;
	}

	/* Jump to the parent */
	--path_ptr;

	/* Release all successors in all tree levels */
	while (path_ptr >= path) {
		entries =
		    ext4_extent_header_get_entries_count(path_ptr->header);
		struct ext4_extent_index *index = path_ptr->index + 1;
		struct ext4_extent_index *stop =
		    EXT4_EXTENT_FIRST_INDEX(path_ptr->header) + entries;

		/* Correct entries count because of changes in the previous
		 * iteration */
		if (remove_parent_record)
			entries--;

		/* Iterate over all entries and release the whole subtrees */
		while (index < stop) {
			rc = ext4_extent_release_branch(inode_ref, index);
			if (rc != EOK)
				goto cleanup;

			++index;
			--entries;
		}

		ext4_extent_header_set_entries_count(path_ptr->header, entries);
		ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&path_ptr->block));
		path_ptr->block.dirty = true;

		/* Free the node if it is empty */
		if ((entries == 0) && (path_ptr != path)) {
			rc = ext4_balloc_free_block(inode_ref,
						    path_ptr->block.lb_id);
			if (rc != EOK)
				goto cleanup;

			/* Mark parent to be checked */
			remove_parent_record = true;
		} else
			remove_parent_record = false;

		--path_ptr;
	}

	if (!entries)
		ext4_extent_header_set_depth(path->header, 0);

cleanup:
	/*
	 * Put loaded blocks
	 * starting from 1: 0 is a block with inode data
	 */
	for (i = 1; i <= path->depth; ++i) {
		if (path[i].block.lb_id) {
			int r =
			    ext4_block_set(inode_ref->fs->bdev, &path[i].block);
			if (r != EOK)
				rc = r;
		}
	}

	/* Destroy temporary data structure */
	free(path);

	return rc;
}

/**@brief Append new extent to the i-node and do some splitting if necessary.
 * @param inode_ref      I-node to append extent to
 * @param path           Path in the extent tree for possible splitting
 * @param last_path_item Input/output parameter for pointer to the last
 *                       valid item in the extent tree path
 * @param iblock         Logical index of block to append extent for
 * @return Error code */
static int ext4_extent_append_extent(struct ext4_inode_ref *inode_ref,
				     struct ext4_extent_path *path,
				     uint32_t iblock)
{
	struct ext4_extent_path *path_ptr = path + path->depth;

	uint32_t block_size = ext4_sb_get_block_size(&inode_ref->fs->sb);

	/* Start splitting */
	while (path_ptr > path) {
		uint16_t entries =
		    ext4_extent_header_get_entries_count(path_ptr->header);
		uint16_t limit =
		    ext4_extent_header_get_max_entries_count(path_ptr->header);

		if (entries == limit) {
			/* Full node - allocate block for new one */
			ext4_fsblk_t goal, fblock;
			int rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
			if (rc != EOK)
				return rc;

			rc = ext4_balloc_alloc_block(inode_ref, goal, &fblock);
			if (rc != EOK)
				return rc;

			struct ext4_block block;
			rc =
			    ext4_block_get(inode_ref->fs->bdev, &block, fblock);
			if (rc != EOK) {
				ext4_balloc_free_block(inode_ref, fblock);
				return rc;
			}

			/* Put back not modified old block */
			rc = ext4_block_set(inode_ref->fs->bdev,
					    &path_ptr->block);
			if (rc != EOK) {
				ext4_balloc_free_block(inode_ref, fblock);
				return rc;
			}

			/* Initialize newly allocated block and remember it */
			memset(block.data, 0, block_size);
			path_ptr->block = block;

			/* Update pointers in extent path structure */
			path_ptr->header = (void *)block.data;
			if (path_ptr->depth) {
				path_ptr->index =
				    EXT4_EXTENT_FIRST_INDEX(path_ptr->header);
				ext4_extent_index_set_first_block(
				    path_ptr->index, iblock);
				ext4_extent_index_set_leaf(
				    path_ptr->index,
				    (path_ptr + 1)->block.lb_id);
				limit = (block_size -
					 sizeof(struct ext4_extent_header)) /
					sizeof(struct ext4_extent_index);
			} else {
				path_ptr->extent =
				    EXT4_EXTENT_FIRST(path_ptr->header);
				ext4_extent_set_first_block(path_ptr->extent,
							    iblock);
				limit = (block_size -
					 sizeof(struct ext4_extent_header)) /
					sizeof(struct ext4_extent);
			}

			/* Initialize on-disk structure (header) */
			ext4_extent_header_set_entries_count(path_ptr->header,
							     1);
			ext4_extent_header_set_max_entries_count(
			    path_ptr->header, limit);
			ext4_extent_header_set_magic(path_ptr->header,
						     EXT4_EXTENT_MAGIC);
			ext4_extent_header_set_depth(path_ptr->header,
						     path_ptr->depth);
			ext4_extent_header_set_generation(path_ptr->header, 0);

			ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&path_ptr->block));
			path_ptr->block.dirty = true;

			/* Jump to the preceding item */
			path_ptr--;
		} else {
			/* Node with free space */
			if (path_ptr->depth) {
				path_ptr->index =
				    EXT4_EXTENT_FIRST_INDEX(path_ptr->header) +
				    entries;
				ext4_extent_index_set_first_block(
				    path_ptr->index, iblock);
				ext4_extent_index_set_leaf(
				    path_ptr->index,
				    (path_ptr + 1)->block.lb_id);
			} else {
				path_ptr->extent =
				    EXT4_EXTENT_FIRST(path_ptr->header) +
				    entries;
				ext4_extent_set_first_block(path_ptr->extent,
							    iblock);
			}

			ext4_extent_header_set_entries_count(path_ptr->header,
							     entries + 1);
			ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&path_ptr->block));
			path_ptr->block.dirty = true;

			/* No more splitting needed */
			return EOK;
		}
	}

	ext4_assert(path_ptr == path);

	/* Should be the root split too? */

	uint16_t entries = ext4_extent_header_get_entries_count(path->header);
	uint16_t limit = ext4_extent_header_get_max_entries_count(path->header);

	if (entries == limit) {
		ext4_fsblk_t goal, new_fblock;
		int rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
		if (rc != EOK)
			return rc;

		rc = ext4_balloc_alloc_block(inode_ref, goal, &new_fblock);
		if (rc != EOK)
			return rc;

		struct ext4_block block;
		rc = ext4_block_get(inode_ref->fs->bdev, &block, new_fblock);
		if (rc != EOK)
			return rc;

		/* Initialize newly allocated block */
		memset(block.data, 0, block_size);

		/* Move data from root to the new block */
		memcpy(block.data, inode_ref->inode->blocks,
		       EXT4_INODE_BLOCKS * sizeof(uint32_t));

		/* Data block is initialized */

		struct ext4_block *root_block = &path->block;
		uint16_t root_depth = path->depth;
		struct ext4_extent_header *root_header = path->header;

		/* Make space for tree growing */
		struct ext4_extent_path *new_root = path;
		struct ext4_extent_path *old_root = path + 1;

		size_t nbytes =
		    sizeof(struct ext4_extent_path) * (path->depth + 1);
		memmove(old_root, new_root, nbytes);
		memset(new_root, 0, sizeof(struct ext4_extent_path));

		/* Update old root structure */
		old_root->block = block;
		old_root->header = (struct ext4_extent_header *)block.data;

		/* Add new entry and update limit for entries */
		if (old_root->depth) {
			limit =
			    (block_size - sizeof(struct ext4_extent_header)) /
			    sizeof(struct ext4_extent_index);
			old_root->index =
			    EXT4_EXTENT_FIRST_INDEX(old_root->header) + entries;
			ext4_extent_index_set_first_block(old_root->index,
							  iblock);
			ext4_extent_index_set_leaf(old_root->index,
						   (old_root + 1)->block.lb_id);
			old_root->extent = NULL;
		} else {
			limit =
			    (block_size - sizeof(struct ext4_extent_header)) /
			    sizeof(struct ext4_extent);
			old_root->extent =
			    EXT4_EXTENT_FIRST(old_root->header) + entries;
			ext4_extent_set_first_block(old_root->extent, iblock);
			old_root->index = NULL;
		}

		ext4_extent_header_set_entries_count(old_root->header,
						     entries + 1);
		ext4_extent_header_set_max_entries_count(old_root->header,
							 limit);

		ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&old_root->block));
		old_root->block.dirty = true;

		/* Re-initialize new root metadata */
		new_root->depth = root_depth + 1;
		new_root->block = *root_block;
		new_root->header = root_header;
		new_root->extent = NULL;
		new_root->index = EXT4_EXTENT_FIRST_INDEX(new_root->header);

		ext4_extent_header_set_depth(new_root->header, new_root->depth);

		/* Create new entry in root */
		ext4_extent_header_set_entries_count(new_root->header, 1);
		ext4_extent_index_set_first_block(new_root->index, 0);
		ext4_extent_index_set_leaf(new_root->index, new_fblock);

		/* Since new_root belongs to on-disk inode,
		 * we don't do checksum here */
		new_root->block.dirty = true;
	} else {
		if (path->depth) {
			path->index =
			    EXT4_EXTENT_FIRST_INDEX(path->header) + entries;
			ext4_extent_index_set_first_block(path->index, iblock);
			ext4_extent_index_set_leaf(path->index,
						   (path + 1)->block.lb_id);
		} else {
			path->extent =
			    EXT4_EXTENT_FIRST(path->header) + entries;
			ext4_extent_set_first_block(path->extent, iblock);
		}

		ext4_extent_header_set_entries_count(path->header, entries + 1);
		/* Since new_root belongs to on-disk inode,
		 * we don't do checksum here */
		path->block.dirty = true;
	}

	return EOK;
}

/**@brief Append data block to the i-node.
 * This function allocates data block, tries to append it
 * to some existing extent or creates new extents.
 * It includes possible extent tree modifications (splitting).
 * @param inode_ref I-node to append block to
 * @param iblock    Output logical number of newly allocated block
 * @param fblock    Output physical block address of newly allocated block
 *
 * @return Error code*/
static int
ext4_extent_append_block(struct ext4_inode_ref *inode_ref, uint32_t *iblock,
			     ext4_fsblk_t *fblock, bool update_size)
{
	uint16_t i;
	ext4_fsblk_t goal;
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	uint64_t inode_size = ext4_inode_get_size(sb, inode_ref->inode);
	uint32_t block_size = ext4_sb_get_block_size(sb);

	/* Calculate number of new logical block */
	uint32_t new_block_idx = 0;
	if (inode_size > 0) {
		if ((inode_size % block_size) != 0)
			inode_size += block_size - (inode_size % block_size);

		new_block_idx = inode_size / block_size;
	}

	/* Load the nearest leaf (with extent) */
	struct ext4_extent_path *path;
	int rc = ext4_extent_find_extent(inode_ref, new_block_idx, &path);
	if (rc != EOK)
		return rc;

	/* Jump to last item of the path (extent) */
	struct ext4_extent_path *path_ptr = path;
	while (path_ptr->depth != 0)
		path_ptr++;

	/* Add new extent to the node if not present */
	if (path_ptr->extent == NULL)
		goto append_extent;

	uint16_t block_count = ext4_extent_get_block_count(path_ptr->extent);
	uint16_t block_limit = (1 << 15);

	ext4_fsblk_t phys_block = 0;
	if (block_count < block_limit) {
		/* There is space for new block in the extent */
		if (block_count == 0) {
			int rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
			if (rc != EOK)
				goto finish;

			/* Existing extent is empty */
			rc = ext4_balloc_alloc_block(inode_ref, goal, &phys_block);
			if (rc != EOK)
				goto finish;

			/* Initialize extent */
			ext4_extent_set_first_block(path_ptr->extent,
						    new_block_idx);
			ext4_extent_set_start(path_ptr->extent, phys_block);
			ext4_extent_set_block_count(path_ptr->extent, 1,
						false);

			/* Update i-node */
			if (update_size) {
				ext4_inode_set_size(inode_ref->inode,
						    inode_size + block_size);
				inode_ref->dirty = true;
			}

			ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&path_ptr->block));
			path_ptr->block.dirty = true;

			goto finish;
		} else {
			ext4_fsblk_t goal;
			int rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
			if (rc != EOK)
				goto finish;

			/* Existing extent contains some blocks */
			phys_block = ext4_extent_get_start(path_ptr->extent);
			phys_block +=
			    ext4_extent_get_block_count(path_ptr->extent);

			/* Check if the following block is free for allocation
			 */
			bool free;
			rc = ext4_balloc_try_alloc_block(inode_ref, phys_block,
							 &free);
			if (rc != EOK)
				goto finish;

			if (!free) {
				/* Target is not free, new block must be
				 * appended to new extent
				 */
				goto append_extent;
			}

			/* Update extent */
			ext4_extent_set_block_count(path_ptr->extent,
						    block_count + 1,
						    false);

			/* Update i-node */
			if (update_size) {
				ext4_inode_set_size(inode_ref->inode,
						    inode_size + block_size);
				inode_ref->dirty = true;
			}

			ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&path_ptr->block));
			path_ptr->block.dirty = true;

			goto finish;
		}
	}

append_extent:
	/* Append new extent to the tree */
	phys_block = 0;

	rc = ext4_fs_indirect_find_goal(inode_ref, &goal);
	if (rc != EOK)
		goto finish;

	/* Allocate new data block */
	rc = ext4_balloc_alloc_block(inode_ref, goal, &phys_block);
	if (rc != EOK)
		goto finish;

	/* Append extent for new block (includes tree splitting if needed) */
	rc = ext4_extent_append_extent(inode_ref, path, new_block_idx);
	if (rc != EOK) {
		ext4_balloc_free_block(inode_ref, phys_block);
		goto finish;
	}

	uint32_t tree_depth = ext4_extent_header_get_depth(path->header);
	path_ptr = path + tree_depth;

	/* Initialize newly created extent */
	ext4_extent_set_block_count(path_ptr->extent, 1, false);
	ext4_extent_set_first_block(path_ptr->extent, new_block_idx);
	ext4_extent_set_start(path_ptr->extent, phys_block);

	/* Update i-node */
	if (update_size) {
		ext4_inode_set_size(inode_ref->inode, inode_size + block_size);
		inode_ref->dirty = true;
	}

	ext4_extent_block_csum_set(inode_ref, ext_block_hdr(&path_ptr->block));
	path_ptr->block.dirty = true;

finish:
	/* Set return values */
	*iblock = new_block_idx;
	*fblock = phys_block;

	/*
	 * Put loaded blocks
	 * starting from 1: 0 is a block with inode data
	 */
	for (i = 1; i <= path->depth; ++i) {
		if (path[i].block.lb_id) {
			int r =
			    ext4_block_set(inode_ref->fs->bdev, &path[i].block);
			if (r != EOK)
				rc = r;
		}
	}

	/* Destroy temporary data structure */
	free(path);

	return rc;
}

int ext4_extent_get_blocks(struct ext4_inode_ref *inode_ref, ext4_fsblk_t iblock,
			   ext4_lblk_t max_blocks, ext4_fsblk_t *result, bool create,
			   ext4_lblk_t *blocks_count)
{
	uint32_t iblk = iblock;
	ext4_fsblk_t fblk = 0;
	int r;

	if (blocks_count)
		return ENOTSUP;
	if (max_blocks != 1)
		return ENOTSUP;

	if (!create)
		r = ext4_extent_find_block(inode_ref, iblk, &fblk);
	else
		r = ext4_extent_append_block(inode_ref, &iblk, &fblk, false);

	*result = fblk;
	return r;
}

#endif
/**
 * @}
 */
