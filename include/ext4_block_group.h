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
 * @file  ext4_block_group.h
 * @brief Block group function set.
 */

#ifndef EXT4_BLOCK_GROUP_H_
#define EXT4_BLOCK_GROUP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_super.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief Get address of block with data block bitmap.
 * @param bg pointer to block group
 * @param s pointer to superblock
 * @return Address of block with block bitmap
 */
static inline uint64_t ext4_bg_get_block_bitmap(struct ext4_bgroup *bg,
						struct ext4_sblock *s)
{
	uint64_t v = to_le32(bg->block_bitmap_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint64_t)to_le32(bg->block_bitmap_hi) << 32;

	return v;
}

/**@brief Set address of block with data block bitmap.
 * @param bg pointer to block group
 * @param s pointer to superblock
 * @param blk block to set
 */
static inline void ext4_bg_set_block_bitmap(struct ext4_bgroup *bg,
					    struct ext4_sblock *s, uint64_t blk)
{

	bg->block_bitmap_lo = to_le32((uint32_t)blk);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->block_bitmap_hi = to_le32(blk >> 32);

}

/**@brief Get address of block with i-node bitmap.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @return Address of block with i-node bitmap
 */
static inline uint64_t ext4_bg_get_inode_bitmap(struct ext4_bgroup *bg,
						struct ext4_sblock *s)
{

	uint64_t v = to_le32(bg->inode_bitmap_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint64_t)to_le32(bg->inode_bitmap_hi) << 32;

	return v;
}

/**@brief Set address of block with i-node bitmap.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @param blk block to set
 */
static inline void ext4_bg_set_inode_bitmap(struct ext4_bgroup *bg,
					    struct ext4_sblock *s, uint64_t blk)
{
	bg->inode_bitmap_lo = to_le32((uint32_t)blk);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->inode_bitmap_hi = to_le32(blk >> 32);

}


/**@brief Get address of the first block of the i-node table.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @return Address of first block of i-node table
 */
static inline uint64_t
ext4_bg_get_inode_table_first_block(struct ext4_bgroup *bg,
				    struct ext4_sblock *s)
{
	uint64_t v = to_le32(bg->inode_table_first_block_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint64_t)to_le32(bg->inode_table_first_block_hi) << 32;

	return v;
}

/**@brief Set address of the first block of the i-node table.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @param blk block to set
 */
static inline void
ext4_bg_set_inode_table_first_block(struct ext4_bgroup *bg,
				    struct ext4_sblock *s, uint64_t blk)
{
	bg->inode_table_first_block_lo = to_le32((uint32_t)blk);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->inode_table_first_block_hi = to_le32(blk >> 32);
}

/**@brief Get number of free blocks in block group.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @return Number of free blocks in block group
 */
static inline uint32_t ext4_bg_get_free_blocks_count(struct ext4_bgroup *bg,
						     struct ext4_sblock *s)
{
	uint32_t v = to_le16(bg->free_blocks_count_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t)to_le16(bg->free_blocks_count_hi) << 16;

	return v;
}

/**@brief Set number of free blocks in block group.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @param cnt Number of free blocks in block group
 */
static inline void ext4_bg_set_free_blocks_count(struct ext4_bgroup *bg,
						 struct ext4_sblock *s,
						 uint32_t cnt)
{
	bg->free_blocks_count_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->free_blocks_count_hi = to_le16(cnt >> 16);
}

/**@brief Get number of free i-nodes in block group.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @return Number of free i-nodes in block group
 */
static inline uint32_t ext4_bg_get_free_inodes_count(struct ext4_bgroup *bg,
						     struct ext4_sblock *s)
{
	uint32_t v = to_le16(bg->free_inodes_count_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t)to_le16(bg->free_inodes_count_hi) << 16;

	return v;
}

/**@brief Set number of free i-nodes in block group.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @param cnt Number of free i-nodes in block group
 */
static inline void ext4_bg_set_free_inodes_count(struct ext4_bgroup *bg,
						 struct ext4_sblock *s,
						 uint32_t cnt)
{
	bg->free_inodes_count_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->free_inodes_count_hi = to_le16(cnt >> 16);
}

/**@brief Get number of used directories in block group.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @return Number of used directories in block group
 */
static inline uint32_t ext4_bg_get_used_dirs_count(struct ext4_bgroup *bg,
						   struct ext4_sblock *s)
{
	uint32_t v = to_le16(bg->used_dirs_count_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t)to_le16(bg->used_dirs_count_hi) << 16;

	return v;
}

/**@brief Set number of used directories in block group.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @param cnt Number of used directories in block group
 */
static inline void ext4_bg_set_used_dirs_count(struct ext4_bgroup *bg,
					       struct ext4_sblock *s,
					       uint32_t cnt)
{
	bg->used_dirs_count_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->used_dirs_count_hi = to_le16(cnt >> 16);
}

/**@brief Get number of unused i-nodes.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @return Number of unused i-nodes
 */
static inline uint32_t ext4_bg_get_itable_unused(struct ext4_bgroup *bg,
						 struct ext4_sblock *s)
{

	uint32_t v = to_le16(bg->itable_unused_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t)to_le16(bg->itable_unused_hi) << 16;

	return v;
}

/**@brief Set number of unused i-nodes.
 * @param bg Pointer to block group
 * @param s Pointer to superblock
 * @param cnt Number of unused i-nodes
 */
static inline void ext4_bg_set_itable_unused(struct ext4_bgroup *bg,
					     struct ext4_sblock *s,
					     uint32_t cnt)
{
	bg->itable_unused_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->itable_unused_hi = to_le16(cnt >> 16);
}

/**@brief  Set checksum of block group.
 * @param bg Pointer to block group
 * @param crc Cheksum of block group
 */
static inline void ext4_bg_set_checksum(struct ext4_bgroup *bg, uint16_t crc)
{
	bg->checksum = to_le16(crc);
}

/**@brief Check if block group has a flag.
 * @param bg Pointer to block group
 * @param f Flag to be checked
 * @return True if flag is set to 1
 */
static inline bool ext4_bg_has_flag(struct ext4_bgroup *bg, uint32_t f)
{
	return to_le16(bg->flags) & f;
}

/**@brief Set flag of block group.
 * @param bg Pointer to block group
 * @param f Flag to be set
 */
static inline void ext4_bg_set_flag(struct ext4_bgroup *bg, uint32_t f)
{
	uint16_t flags = to_le16(bg->flags);
	flags |= f;
	bg->flags = to_le16(flags);
}

/**@brief Clear flag of block group.
 * @param bg Pointer to block group
 * @param f Flag to be cleared
 */
static inline void ext4_bg_clear_flag(struct ext4_bgroup *bg, uint32_t f)
{
	uint16_t flags = to_le16(bg->flags);
	flags &= ~f;
	bg->flags = to_le16(flags);
}

/**@brief Calculate CRC16 of the block group.
 * @param crc Init value
 * @param buffer Input buffer
 * @param len Sizeof input buffer
 * @return Computed CRC16*/
uint16_t ext4_bg_crc16(uint16_t crc, const uint8_t *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_BLOCK_GROUP_H_ */

/**
 * @}
 */
