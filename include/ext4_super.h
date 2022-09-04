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
 * @file  ext4_super.c
 * @brief Superblock operations.
 */

#ifndef EXT4_SUPER_H_
#define EXT4_SUPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>

/**@brief   Blocks count get stored in superblock.
 * @param   s superblock descriptor
 * @return  count of blocks*/
static inline uint64_t ext4_sb_get_blocks_cnt(struct ext4_sblock *s)
{
	return ((uint64_t)to_le32(s->blocks_count_hi) << 32) |
	       to_le32(s->blocks_count_lo);
}

/**@brief   Blocks count set  in superblock.
 * @param   s superblock descriptor
 * @param   cnt count of blocks*/
static inline void ext4_sb_set_blocks_cnt(struct ext4_sblock *s, uint64_t cnt)
{
	s->blocks_count_lo = to_le32((cnt << 32) >> 32);
	s->blocks_count_hi = to_le32(cnt >> 32);
}

/**@brief   Free blocks count get stored in superblock.
 * @param   s superblock descriptor
 * @return  free blocks*/
static inline uint64_t ext4_sb_get_free_blocks_cnt(struct ext4_sblock *s)
{
	return ((uint64_t)to_le32(s->free_blocks_count_hi) << 32) |
	       to_le32(s->free_blocks_count_lo);
}

/**@brief   Free blocks count set.
 * @param   s superblock descriptor
 * @param   cnt new value of free blocks*/
static inline void ext4_sb_set_free_blocks_cnt(struct ext4_sblock *s,
					       uint64_t cnt)
{
	s->free_blocks_count_lo = to_le32((cnt << 32) >> 32);
	s->free_blocks_count_hi = to_le32(cnt >> 32);
}

/**@brief   Block size get from superblock.
 * @param   s superblock descriptor
 * @return  block size in bytes*/
static inline uint32_t ext4_sb_get_block_size(struct ext4_sblock *s)
{
	return 1024 << to_le32(s->log_block_size);
}

/**@brief   Block group descriptor size.
 * @param   s superblock descriptor
 * @return  block group descriptor size in bytes*/
static inline uint16_t ext4_sb_get_desc_size(struct ext4_sblock *s)
{
	uint16_t size = to_le16(s->desc_size);

	return size < EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE
		   ? EXT4_MIN_BLOCK_GROUP_DESCRIPTOR_SIZE
		   : size;
}

/*************************Flags and features*********************************/

/**@brief   Support check of flag.
 * @param   s superblock descriptor
 * @param   v flag to check
 * @return  true if flag is supported*/
static inline bool ext4_sb_check_flag(struct ext4_sblock *s, uint32_t v)
{
	return to_le32(s->flags) & v;
}

/**@brief   Support check of feature compatible.
 * @param   s superblock descriptor
 * @param   v feature to check
 * @return  true if feature is supported*/
static inline bool ext4_sb_feature_com(struct ext4_sblock *s, uint32_t v)
{
	return to_le32(s->features_compatible) & v;
}

/**@brief   Support check of feature incompatible.
 * @param   s superblock descriptor
 * @param   v feature to check
 * @return  true if feature is supported*/
static inline bool ext4_sb_feature_incom(struct ext4_sblock *s, uint32_t v)
{
	return to_le32(s->features_incompatible) & v;
}

/**@brief   Support check of read only flag.
 * @param   s superblock descriptor
 * @param   v flag to check
 * @return  true if flag is supported*/
static inline bool ext4_sb_feature_ro_com(struct ext4_sblock *s, uint32_t v)
{
	return to_le32(s->features_read_only) & v;
}

/**@brief   Block group to flex group.
 * @param   s superblock descriptor
 * @param   block_group block group
 * @return  flex group id*/
static inline uint32_t ext4_sb_bg_to_flex(struct ext4_sblock *s,
					  uint32_t block_group)
{
	return block_group >> to_le32(s->log_groups_per_flex);
}

/**@brief   Flex block group size.
 * @param   s superblock descriptor
 * @return  flex bg size*/
static inline uint32_t ext4_sb_flex_bg_size(struct ext4_sblock *s)
{
	return 1 << to_le32(s->log_groups_per_flex);
}

/**@brief   Return first meta block group id.
 * @param   s superblock descriptor
 * @return  first meta_bg id */
static inline uint32_t ext4_sb_first_meta_bg(struct ext4_sblock *s)
{
	return to_le32(s->first_meta_bg);
}

/**************************More complex functions****************************/

/**@brief   Returns a block group count.
 * @param   s superblock descriptor
 * @return  count of block groups*/
uint32_t ext4_block_group_cnt(struct ext4_sblock *s);

/**@brief   Returns block count in block group
 *          (last block group may have less blocks)
 * @param   s superblock descriptor
 * @param   bgid block group id
 * @return  blocks count*/
uint32_t ext4_blocks_in_group_cnt(struct ext4_sblock *s, uint32_t bgid);

/**@brief   Returns inodes count in block group
 *          (last block group may have less inodes)
 * @param   s superblock descriptor
 * @param   bgid block group id
 * @return  inodes count*/
uint32_t ext4_inodes_in_group_cnt(struct ext4_sblock *s, uint32_t bgid);

/***************************Read/write/check superblock**********************/

/**@brief   Superblock write.
 * @param   bdev block device descriptor.
 * @param   s superblock descriptor
 * @return  Standard error code */
int ext4_sb_write(struct ext4_blockdev *bdev, struct ext4_sblock *s);

/**@brief   Superblock read.
 * @param   bdev block device descriptor.
 * @param   s superblock descriptor
 * @return  Standard error code */
int ext4_sb_read(struct ext4_blockdev *bdev, struct ext4_sblock *s);

/**@brief   Superblock simple validation.
 * @param   s superblock descriptor
 * @return  true if OK*/
bool ext4_sb_check(struct ext4_sblock *s);

/**@brief   Superblock presence in block group.
 * @param   s superblock descriptor
 * @param   block_group block group id
 * @return  true if block group has superblock*/
bool ext4_sb_is_super_in_bg(struct ext4_sblock *s, uint32_t block_group);

/**@brief   TODO:*/
bool ext4_sb_sparse(uint32_t group);

/**@brief   TODO:*/
uint32_t ext4_bg_num_gdb(struct ext4_sblock *s, uint32_t group);

/**@brief   TODO:*/
uint32_t ext4_num_base_meta_clusters(struct ext4_sblock *s,
				     uint32_t block_group);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_SUPER_H_ */

/**
 * @}
 */
