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

#include <ext4_config.h>
#include <ext4_types.h>

/**@brief Get logical number of the block covered by extent.
 * @param extent Extent to load number from
 * @return Logical number of the first block covered by extent */
uint32_t ext4_extent_get_first_block(struct ext4_extent *extent);

/**@brief Set logical number of the first block covered by extent.
 * @param extent Extent to set number to
 * @param iblock Logical number of the first block covered by extent */
void ext4_extent_set_first_block(struct ext4_extent *extent, uint32_t iblock);

/**@brief Get number of blocks covered by extent.
 * @param extent Extent to load count from
 * @return Number of blocks covered by extent */
uint16_t ext4_extent_get_block_count(struct ext4_extent *extent);

/**@brief Set number of blocks covered by extent.
 * @param extent Extent to load count from
 * @param count  Number of blocks covered by extent */
void ext4_extent_set_block_count(struct ext4_extent *extent, uint16_t count);

/**@brief Get physical number of the first block covered by extent.
 * @param extent Extent to load number
 * @return Physical number of the first block covered by extent */
uint64_t ext4_extent_get_start(struct ext4_extent *extent);

/**@brief Set physical number of the first block covered by extent.
 * @param extent Extent to load number
 * @param fblock Physical number of the first block covered by extent */
void ext4_extent_set_start(struct ext4_extent *extent, uint64_t fblock);


/**@brief Get logical number of the block covered by extent index.
 * @param index Extent index to load number from
 * @return Logical number of the first block covered by extent index */
uint32_t ext4_extent_index_get_first_block(struct ext4_extent_index *index);

/**@brief Set logical number of the block covered by extent index.
 * @param index  Extent index to set number to
 * @param iblock Logical number of the first block covered by extent index */
void ext4_extent_index_set_first_block(struct ext4_extent_index *index,
    uint32_t iblock);

/**@brief Get physical number of block where the child node is located.
 * @param index Extent index to load number from
 * @return Physical number of the block with child node */
uint64_t ext4_extent_index_get_leaf(struct ext4_extent_index *index);


/**@brief Set physical number of block where the child node is located.
 * @param index  Extent index to set number to
 * @param fblock Ohysical number of the block with child node */
void ext4_extent_index_set_leaf(struct ext4_extent_index *index,
    uint64_t fblock);


/**@brief Get magic value from extent header.
 * @param header Extent header to load value from
 * @return Magic value of extent header */
uint16_t ext4_extent_header_get_magic(struct ext4_extent_header *header);

/**@brief Set magic value to extent header.
 * @param header Extent header to set value to
 * @param magic  Magic value of extent header */
void ext4_extent_header_set_magic(struct ext4_extent_header *header,
    uint16_t magic);

/**@brief Get number of entries from extent header
 * @param header Extent header to get value from
 * @return Number of entries covered by extent header */
uint16_t ext4_extent_header_get_entries_count(struct ext4_extent_header *header);

/**@brief Set number of entries to extent header
 * @param header Extent header to set value to
 * @param count  Number of entries covered by extent header */
void ext4_extent_header_set_entries_count(struct ext4_extent_header *header,
    uint16_t count);

/**@brief Get maximum number of entries from extent header
 * @param header Extent header to get value from
 * @return Maximum number of entries covered by extent header */
uint16_t ext4_extent_header_get_max_entries_count(struct ext4_extent_header *header);

/**@brief Set maximum number of entries to extent header
 * @param header    Extent header to set value to
 * @param max_count Maximum number of entries covered by extent header */
void ext4_extent_header_set_max_entries_count(struct ext4_extent_header *header,
    uint16_t max_count);

/**@brief Get depth of extent subtree.
 * @param header Extent header to get value from
 * @return Depth of extent subtree */
uint16_t ext4_extent_header_get_depth(struct ext4_extent_header *header);

/**@brief Set depth of extent subtree.
 * @param header Extent header to set value to
 * @param depth  Depth of extent subtree */
void ext4_extent_header_set_depth(struct ext4_extent_header *header,
    uint16_t depth);

/**@brief Get generation from extent header
 * @param header Extent header to get value from
 * @return Generation */
uint32_t ext4_extent_header_get_generation(struct ext4_extent_header *header);

/**@brief Set generation to extent header
 * @param header     Extent header to set value to
 * @param generation Generation */
void ext4_extent_header_set_generation(struct ext4_extent_header *header,
    uint32_t generation);

/**@brief Find physical block in the extent tree by logical block number.
 * There is no need to save path in the tree during this algorithm.
 * @param inode_ref I-node to load block from
 * @param iblock    Logical block number to find
 * @param fblock    Output value for physical block number
 * @return Error code*/
int ext4_extent_find_block(struct ext4_inode_ref *inode_ref, uint32_t iblock,
    uint32_t *fblock);

/**@brief Release all data blocks starting from specified logical block.
 * @param inode_ref   I-node to release blocks from
 * @param iblock_from First logical block to release
 * @return Error code */
int ext4_extent_release_blocks_from(struct ext4_inode_ref *inode_ref,
    uint32_t iblock_from);

/**@brief Append data block to the i-node.
 * This function allocates data block, tries to append it
 * to some existing extent or creates new extents.
 * It includes possible extent tree modifications (splitting).
 * @param inode_ref I-node to append block to
 * @param iblock    Output logical number of newly allocated block
 * @param fblock    Output physical block address of newly allocated block
 *
 * @return Error code*/
int ext4_extent_append_block(struct ext4_inode_ref *inode_ref,
        uint32_t *iblock, uint32_t *fblock, bool update_size);


#endif /* EXT4_EXTENT_H_ */
/**
 * @}
 */
