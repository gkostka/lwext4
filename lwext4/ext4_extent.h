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
 * @file  ext4_extent.h
 * @brief More complex filesystem functions.
 */
#ifndef EXT4_EXTENT_H_
#define EXT4_EXTENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ext4_config.h"
#include "ext4_types.h"
#include "ext4_inode.h"


/**@brief Get logical number of the block covered by extent.
 * @param extent Extent to load number from
 * @return Logical number of the first block covered by extent */
static inline uint32_t ext4_extent_get_first_block(struct ext4_extent *extent)
{
	return to_le32(extent->first_block);
}

/**@brief Set logical number of the first block covered by extent.
 * @param extent Extent to set number to
 * @param iblock Logical number of the first block covered by extent */
static inline void ext4_extent_set_first_block(struct ext4_extent *extent,
		uint32_t iblock)
{
	extent->first_block = to_le32(iblock);
}

/**@brief Get number of blocks covered by extent.
 * @param extent Extent to load count from
 * @return Number of blocks covered by extent */
static inline uint16_t ext4_extent_get_block_count(struct ext4_extent *extent)
{
	if (EXT4_EXT_IS_UNWRITTEN(extent))
		return EXT4_EXT_GET_LEN_UNWRITTEN(extent);
	else
		return EXT4_EXT_GET_LEN(extent);
}
/**@brief Set number of blocks covered by extent.
 * @param extent Extent to load count from
 * @param count  Number of blocks covered by extent
 * @param unwritten Whether the extent is unwritten or not */
static inline void ext4_extent_set_block_count(struct ext4_extent *extent,
					       uint16_t count, bool unwritten)
{
	EXT4_EXT_SET_LEN(extent, count);
	if (unwritten)
		EXT4_EXT_SET_UNWRITTEN(extent);
}

/**@brief Get physical number of the first block covered by extent.
 * @param extent Extent to load number
 * @return Physical number of the first block covered by extent */
static inline uint64_t ext4_extent_get_start(struct ext4_extent *extent)
{
	return ((uint64_t)to_le16(extent->start_hi)) << 32 |
	       ((uint64_t)to_le32(extent->start_lo));
}


/**@brief Set physical number of the first block covered by extent.
 * @param extent Extent to load number
 * @param fblock Physical number of the first block covered by extent */
static inline void ext4_extent_set_start(struct ext4_extent *extent, uint64_t fblock)
{
	extent->start_lo = to_le32((fblock << 32) >> 32);
	extent->start_hi = to_le16((uint16_t)(fblock >> 32));
}


/**@brief Get logical number of the block covered by extent index.
 * @param index Extent index to load number from
 * @return Logical number of the first block covered by extent index */
static inline uint32_t
ext4_extent_index_get_first_block(struct ext4_extent_index *index)
{
	return to_le32(index->first_block);
}

/**@brief Set logical number of the block covered by extent index.
 * @param index  Extent index to set number to
 * @param iblock Logical number of the first block covered by extent index */
static inline void
ext4_extent_index_set_first_block(struct ext4_extent_index *index,
				  uint32_t iblock)
{
	index->first_block = to_le32(iblock);
}

/**@brief Get physical number of block where the child node is located.
 * @param index Extent index to load number from
 * @return Physical number of the block with child node */
static inline uint64_t
ext4_extent_index_get_leaf(struct ext4_extent_index *index)
{
	return ((uint64_t)to_le16(index->leaf_hi)) << 32 |
	       ((uint64_t)to_le32(index->leaf_lo));
}

/**@brief Set physical number of block where the child node is located.
 * @param index  Extent index to set number to
 * @param fblock Ohysical number of the block with child node */
static inline void ext4_extent_index_set_leaf(struct ext4_extent_index *index,
					      uint64_t fblock)
{
	index->leaf_lo = to_le32((fblock << 32) >> 32);
	index->leaf_hi = to_le16((uint16_t)(fblock >> 32));
}

/**@brief Get magic value from extent header.
 * @param header Extent header to load value from
 * @return Magic value of extent header */
static inline uint16_t
ext4_extent_header_get_magic(struct ext4_extent_header *header)
{
	return to_le16(header->magic);
}

/**@brief Set magic value to extent header.
 * @param header Extent header to set value to
 * @param magic  Magic value of extent header */
static inline void ext4_extent_header_set_magic(struct ext4_extent_header *header,
						uint16_t magic)
{
	header->magic = to_le16(magic);
}

/**@brief Get number of entries from extent header
 * @param header Extent header to get value from
 * @return Number of entries covered by extent header */
static inline uint16_t
ext4_extent_header_get_entries_count(struct ext4_extent_header *header)
{
	return to_le16(header->entries_count);
}

/**@brief Set number of entries to extent header
 * @param header Extent header to set value to
 * @param count  Number of entries covered by extent header */
static inline void
ext4_extent_header_set_entries_count(struct ext4_extent_header *header,
				     uint16_t count)
{
	header->entries_count = to_le16(count);
}

/**@brief Get maximum number of entries from extent header
 * @param header Extent header to get value from
 * @return Maximum number of entries covered by extent header */
static inline uint16_t
ext4_extent_header_get_max_entries_count(struct ext4_extent_header *header)
{
	return to_le16(header->max_entries_count);
}

/**@brief Set maximum number of entries to extent header
 * @param header    Extent header to set value to
 * @param max_count Maximum number of entries covered by extent header */
static inline void
ext4_extent_header_set_max_entries_count(struct ext4_extent_header *header,
					      uint16_t max_count)
{
	header->max_entries_count = to_le16(max_count);
}

/**@brief Get depth of extent subtree.
 * @param header Extent header to get value from
 * @return Depth of extent subtree */
static inline uint16_t
ext4_extent_header_get_depth(struct ext4_extent_header *header)
{
	return to_le16(header->depth);
}

/**@brief Set depth of extent subtree.
 * @param header Extent header to set value to
 * @param depth  Depth of extent subtree */
static inline void
ext4_extent_header_set_depth(struct ext4_extent_header *header, uint16_t depth)
{
	header->depth = to_le16(depth);
}

/**@brief Get generation from extent header
 * @param header Extent header to get value from
 * @return Generation */
static inline uint32_t
ext4_extent_header_get_generation(struct ext4_extent_header *header)
{
	return to_le32(header->generation);
}

/**@brief Set generation to extent header
 * @param header     Extent header to set value to
 * @param generation Generation */
static inline void
ext4_extent_header_set_generation(struct ext4_extent_header *header,
				       uint32_t generation)
{
	header->generation = to_le32(generation);
}

/******************************************************************************/

/**TODO:  */
static inline void ext4_extent_tree_init(struct ext4_inode_ref *inode_ref)
{
	/* Initialize extent root header */
	struct ext4_extent_header *header =
			ext4_inode_get_extent_header(inode_ref->inode);
	ext4_extent_header_set_depth(header, 0);
	ext4_extent_header_set_entries_count(header, 0);
	ext4_extent_header_set_generation(header, 0);
	ext4_extent_header_set_magic(header, EXT4_EXTENT_MAGIC);

	uint16_t max_entries = (EXT4_INODE_BLOCKS * sizeof(uint32_t) -
			sizeof(struct ext4_extent_header)) /
					sizeof(struct ext4_extent);

	ext4_extent_header_set_max_entries_count(header, max_entries);
	inode_ref->dirty  = true;
}



/**TODO:  */
int ext4_extent_get_blocks(struct ext4_inode_ref *inode_ref, ext4_fsblk_t iblock,
			   uint32_t max_blocks, ext4_fsblk_t *result, bool create,
			   uint32_t *blocks_count);


/**@brief Release all data blocks starting from specified logical block.
 * @param inode_ref   I-node to release blocks from
 * @param iblock_from First logical block to release
 * @return Error code */
int ext4_extent_remove_space(struct ext4_inode_ref *inode_ref, ext4_lblk_t from,
			     ext4_lblk_t to);


#ifdef __cplusplus
}
#endif

#endif /* EXT4_EXTENT_H_ */
/**
* @}
*/
