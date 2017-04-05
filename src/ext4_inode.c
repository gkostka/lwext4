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
 * @file  ext4_inode.c
 * @brief Inode handle functions
 */

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_debug.h>

#include <ext4_inode.h>
#include <ext4_super.h>

/**@brief  Compute number of bits for block count.
 * @param block_size Filesystem block_size
 * @return Number of bits
 */
static uint32_t ext4_inode_block_bits_count(uint32_t block_size)
{
	uint32_t bits = 8;
	uint32_t size = block_size;

	do {
		bits++;
		size = size >> 1;
	} while (size > 256);

	return bits;
}

uint32_t ext4_inode_get_mode(struct ext4_sblock *sb, struct ext4_inode *inode)
{
	uint32_t v = to_le16(inode->mode);

	if (ext4_get32(sb, creator_os) == EXT4_SUPERBLOCK_OS_HURD) {
		v |= ((uint32_t)to_le16(inode->osd2.hurd2.mode_high)) << 16;
	}

	return v;
}

void ext4_inode_set_mode(struct ext4_sblock *sb, struct ext4_inode *inode,
			 uint32_t mode)
{
	inode->mode = to_le16((mode << 16) >> 16);

	if (ext4_get32(sb, creator_os) == EXT4_SUPERBLOCK_OS_HURD)
		inode->osd2.hurd2.mode_high = to_le16(mode >> 16);
}

uint32_t ext4_inode_get_uid(struct ext4_inode *inode)
{
	return to_le32(inode->uid);
}

void ext4_inode_set_uid(struct ext4_inode *inode, uint32_t uid)
{
	inode->uid = to_le32(uid);
}

uint64_t ext4_inode_get_size(struct ext4_sblock *sb, struct ext4_inode *inode)
{
	uint64_t v = to_le32(inode->size_lo);

	if ((ext4_get32(sb, rev_level) > 0) &&
	    (ext4_inode_is_type(sb, inode, EXT4_INODE_MODE_FILE)))
		v |= ((uint64_t)to_le32(inode->size_hi)) << 32;

	return v;
}

void ext4_inode_set_size(struct ext4_inode *inode, uint64_t size)
{
	inode->size_lo = to_le32((size << 32) >> 32);
	inode->size_hi = to_le32(size >> 32);
}

uint32_t ext4_inode_get_csum(struct ext4_sblock *sb, struct ext4_inode *inode)
{
	uint16_t inode_size = ext4_get16(sb, inode_size);
	uint32_t v = to_le16(inode->osd2.linux2.checksum_lo);

	if (inode_size > EXT4_GOOD_OLD_INODE_SIZE)
		v |= ((uint32_t)to_le16(inode->checksum_hi)) << 16;

	return v;
}

void ext4_inode_set_csum(struct ext4_sblock *sb, struct ext4_inode *inode,
			uint32_t checksum)
{
	uint16_t inode_size = ext4_get16(sb, inode_size);
	inode->osd2.linux2.checksum_lo =
		to_le16((checksum << 16) >> 16);

	if (inode_size > EXT4_GOOD_OLD_INODE_SIZE)
		inode->checksum_hi = to_le16(checksum >> 16);

}

uint32_t ext4_inode_get_access_time(struct ext4_inode *inode)
{
	return to_le32(inode->access_time);
}
void ext4_inode_set_access_time(struct ext4_inode *inode, uint32_t time)
{
	inode->access_time = to_le32(time);
}

uint32_t ext4_inode_get_change_inode_time(struct ext4_inode *inode)
{
	return to_le32(inode->change_inode_time);
}
void ext4_inode_set_change_inode_time(struct ext4_inode *inode, uint32_t time)
{
	inode->change_inode_time = to_le32(time);
}

uint32_t ext4_inode_get_modif_time(struct ext4_inode *inode)
{
	return to_le32(inode->modification_time);
}

void ext4_inode_set_modif_time(struct ext4_inode *inode, uint32_t time)
{
	inode->modification_time = to_le32(time);
}

uint32_t ext4_inode_get_del_time(struct ext4_inode *inode)
{
	return to_le32(inode->deletion_time);
}

void ext4_inode_set_del_time(struct ext4_inode *inode, uint32_t time)
{
	inode->deletion_time = to_le32(time);
}

uint32_t ext4_inode_get_gid(struct ext4_inode *inode)
{
	return to_le32(inode->gid);
}
void ext4_inode_set_gid(struct ext4_inode *inode, uint32_t gid)
{
	inode->gid = to_le32(gid);
}

uint16_t ext4_inode_get_links_cnt(struct ext4_inode *inode)
{
	return to_le16(inode->links_count);
}
void ext4_inode_set_links_cnt(struct ext4_inode *inode, uint16_t cnt)
{
	inode->links_count = to_le16(cnt);
}

uint64_t ext4_inode_get_blocks_count(struct ext4_sblock *sb,
				     struct ext4_inode *inode)
{
	uint64_t cnt = to_le32(inode->blocks_count_lo);

	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_HUGE_FILE)) {

		/* 48-bit field */
		cnt |= (uint64_t)to_le16(inode->osd2.linux2.blocks_high) << 32;

		if (ext4_inode_has_flag(inode, EXT4_INODE_FLAG_HUGE_FILE)) {

			uint32_t block_count = ext4_sb_get_block_size(sb);
			uint32_t b = ext4_inode_block_bits_count(block_count);
			return cnt << (b - 9);
		}
	}

	return cnt;
}

int ext4_inode_set_blocks_count(struct ext4_sblock *sb,
				struct ext4_inode *inode, uint64_t count)
{
	/* 32-bit maximum */
	uint64_t max = 0;
	max = ~max >> 32;

	if (count <= max) {
		inode->blocks_count_lo = to_le32((uint32_t)count);
		inode->osd2.linux2.blocks_high = 0;
		ext4_inode_clear_flag(inode, EXT4_INODE_FLAG_HUGE_FILE);

		return EOK;
	}

	/* Check if there can be used huge files (many blocks) */
	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_HUGE_FILE))
		return EINVAL;

	/* 48-bit maximum */
	max = 0;
	max = ~max >> 16;

	if (count <= max) {
		inode->blocks_count_lo = to_le32((uint32_t)count);
		inode->osd2.linux2.blocks_high = to_le16((uint16_t)(count >> 32));
		ext4_inode_clear_flag(inode, EXT4_INODE_FLAG_HUGE_FILE);
	} else {
		uint32_t block_count = ext4_sb_get_block_size(sb);
		uint32_t block_bits =ext4_inode_block_bits_count(block_count);

		ext4_inode_set_flag(inode, EXT4_INODE_FLAG_HUGE_FILE);
		count = count >> (block_bits - 9);
		inode->blocks_count_lo = to_le32((uint32_t)count);
		inode->osd2.linux2.blocks_high = to_le16((uint16_t)(count >> 32));
	}

	return EOK;
}

uint32_t ext4_inode_get_flags(struct ext4_inode *inode)
{
	return to_le32(inode->flags);
}
void ext4_inode_set_flags(struct ext4_inode *inode, uint32_t flags)
{
	inode->flags = to_le32(flags);
}

uint32_t ext4_inode_get_generation(struct ext4_inode *inode)
{
	return to_le32(inode->generation);
}
void ext4_inode_set_generation(struct ext4_inode *inode, uint32_t gen)
{
	inode->generation = to_le32(gen);
}

uint16_t ext4_inode_get_extra_isize(struct ext4_sblock *sb,
				    struct ext4_inode *inode)
{
	uint16_t inode_size = ext4_get16(sb, inode_size);
	if (inode_size > EXT4_GOOD_OLD_INODE_SIZE)
		return to_le16(inode->extra_isize);
	else
		return 0;
}

void ext4_inode_set_extra_isize(struct ext4_sblock *sb,
				struct ext4_inode *inode,
				uint16_t size)
{
	uint16_t inode_size = ext4_get16(sb, inode_size);
	if (inode_size > EXT4_GOOD_OLD_INODE_SIZE)
		inode->extra_isize = to_le16(size);
}

uint64_t ext4_inode_get_file_acl(struct ext4_inode *inode,
				 struct ext4_sblock *sb)
{
	uint64_t v = to_le32(inode->file_acl_lo);

	if (ext4_get32(sb, creator_os) == EXT4_SUPERBLOCK_OS_LINUX)
		v |= (uint32_t)to_le16(inode->osd2.linux2.file_acl_high) << 16;

	return v;
}

void ext4_inode_set_file_acl(struct ext4_inode *inode, struct ext4_sblock *sb,
			     uint64_t acl)
{
	inode->file_acl_lo = to_le32((acl << 32) >> 32);

	if (ext4_get32(sb, creator_os) == EXT4_SUPERBLOCK_OS_LINUX)
		inode->osd2.linux2.file_acl_high = to_le16((uint16_t)(acl >> 32));
}

uint32_t ext4_inode_get_direct_block(struct ext4_inode *inode, uint32_t idx)
{
	return to_le32(inode->blocks[idx]);
}
void ext4_inode_set_direct_block(struct ext4_inode *inode, uint32_t idx,
				 uint32_t block)
{
	inode->blocks[idx] = to_le32(block);
}

uint32_t ext4_inode_get_indirect_block(struct ext4_inode *inode, uint32_t idx)
{
	return to_le32(inode->blocks[idx + EXT4_INODE_INDIRECT_BLOCK]);
}

void ext4_inode_set_indirect_block(struct ext4_inode *inode, uint32_t idx,
				   uint32_t block)
{
	inode->blocks[idx + EXT4_INODE_INDIRECT_BLOCK] = to_le32(block);
}

uint32_t ext4_inode_get_dev(struct ext4_inode *inode)
{
	uint32_t dev_0, dev_1;
	dev_0 = ext4_inode_get_direct_block(inode, 0);
	dev_1 = ext4_inode_get_direct_block(inode, 1);

	if (dev_0)
		return dev_0;
	else
		return dev_1;
}

void ext4_inode_set_dev(struct ext4_inode *inode, uint32_t dev)
{
	if (dev & ~0xFFFF)
		ext4_inode_set_direct_block(inode, 1, dev);
	else
		ext4_inode_set_direct_block(inode, 0, dev);
}

uint32_t ext4_inode_type(struct ext4_sblock *sb, struct ext4_inode *inode)
{
	return (ext4_inode_get_mode(sb, inode) & EXT4_INODE_MODE_TYPE_MASK);
}

bool ext4_inode_is_type(struct ext4_sblock *sb, struct ext4_inode *inode,
			uint32_t type)
{
	return ext4_inode_type(sb, inode) == type;
}

bool ext4_inode_has_flag(struct ext4_inode *inode, uint32_t f)
{
	return ext4_inode_get_flags(inode) & f;
}

void ext4_inode_clear_flag(struct ext4_inode *inode, uint32_t f)
{
	uint32_t flags = ext4_inode_get_flags(inode);
	flags = flags & (~f);
	ext4_inode_set_flags(inode, flags);
}

void ext4_inode_set_flag(struct ext4_inode *inode, uint32_t f)
{
	uint32_t flags = ext4_inode_get_flags(inode);
	flags = flags | f;
	ext4_inode_set_flags(inode, flags);
}

bool ext4_inode_can_truncate(struct ext4_sblock *sb, struct ext4_inode *inode)
{
	if ((ext4_inode_has_flag(inode, EXT4_INODE_FLAG_APPEND)) ||
	    (ext4_inode_has_flag(inode, EXT4_INODE_FLAG_IMMUTABLE)))
		return false;

	if ((ext4_inode_is_type(sb, inode, EXT4_INODE_MODE_FILE)) ||
	    (ext4_inode_is_type(sb, inode, EXT4_INODE_MODE_DIRECTORY)) ||
	    (ext4_inode_is_type(sb, inode, EXT4_INODE_MODE_SOFTLINK)))
		return true;

	return false;
}

struct ext4_extent_header *
ext4_inode_get_extent_header(struct ext4_inode *inode)
{
	return (struct ext4_extent_header *)inode->blocks;
}

/**
 * @}
 */
