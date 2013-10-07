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


#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_super.h>

#include <stdint.h>
#include <stdbool.h>

static inline uint64_t ext4_bg_get_block_bitmap(struct ext4_bgroup *bg, struct ext4_sblock *s)
{
	uint64_t v = to_le32(bg->block_bitmap_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint64_t) to_le32(bg->block_bitmap_hi) << 32;

	return v;
}

static inline uint64_t ext4_bg_get_inode_bitmap(struct ext4_bgroup *bg, struct ext4_sblock *s)
{

	uint64_t v = to_le32(bg->inode_bitmap_lo);

	if (ext4_sb_get_desc_size(s)> EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint64_t) to_le32(bg->inode_bitmap_hi) << 32;

	return v;
}

static inline uint64_t ext4_bg_get_inode_table_first_block(struct ext4_bgroup *bg, struct ext4_sblock *s)
{
	uint64_t v = to_le32(bg->inode_table_first_block_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint64_t) to_le32(bg->inode_table_first_block_hi) << 32;

	return v;
}

static inline uint32_t ext4_bg_get_free_blocks_count(struct ext4_bgroup *bg, struct ext4_sblock *s)
{
	uint32_t v = to_le16(bg->free_blocks_count_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t) to_le16(bg->free_blocks_count_hi) << 16;

	return v;
}

static inline void 	 ext4_bg_set_free_blocks_count(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt)
{
	bg->free_blocks_count_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->free_blocks_count_hi = to_le16(cnt >> 16);
}

static inline uint32_t ext4_bg_get_free_inodes_count(struct ext4_bgroup *bg, struct ext4_sblock *s)
{
	uint32_t v = to_le16(bg->free_inodes_count_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t) to_le16(bg->free_inodes_count_hi) << 16;

	return v;
}

static inline void 	 ext4_bg_set_free_inodes_count(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt)
{
	bg->free_inodes_count_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->free_inodes_count_hi = to_le16(cnt >> 16);
}


static inline uint32_t ext4_bg_get_used_dirs_count(struct ext4_bgroup *bg, struct ext4_sblock *s)
{
	uint32_t v = to_le16(bg->used_dirs_count_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t) to_le16(bg->used_dirs_count_hi) << 16;

	return v;
}

static inline void 	 ext4_bg_set_used_dirs_count(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt)
{
	bg->used_dirs_count_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->used_dirs_count_hi = to_le16(cnt >> 16);
}


static inline uint32_t ext4_bg_get_itable_unused(struct ext4_bgroup *bg, struct ext4_sblock *s)
{

	uint32_t v = to_le16(bg->itable_unused_lo);

	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		v |= (uint32_t) to_le16(bg->itable_unused_hi) << 16;

	return v;
}

static inline void 	 ext4_bg_set_itable_unused(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt)
{
	bg->itable_unused_lo = to_le16((cnt << 16) >> 16);
	if (ext4_sb_get_desc_size(s) > EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE)
		bg->itable_unused_hi = to_le16(cnt >> 16);
}


static inline void 	 ext4_bg_set_checksum(struct ext4_bgroup *bg, uint16_t crc)
{
	bg->checksum = to_le16(crc);
}

static inline bool 	 ext4_bg_has_flag(struct ext4_bgroup *bg, uint32_t f)
{
	return to_le16(bg->flags) & f;
}

static inline void 	 ext4_bg_set_flag(struct ext4_bgroup *bg, uint32_t f)
{
	uint16_t flags = to_le16(bg->flags);
	flags |= f;
	bg->flags = to_le16(flags);
}

static inline void 	 ext4_bg_clear_flag(struct ext4_bgroup *bg, uint32_t f)
{
	uint16_t flags = to_le16(bg->flags);
	flags &= ~f;
	bg->flags = to_le16(flags);
}


uint16_t ext4_bg_crc16(uint16_t crc, const uint8_t *buffer, size_t len);

#endif /* EXT4_BLOCK_GROUP_H_ */

/**
 * @}
 */
