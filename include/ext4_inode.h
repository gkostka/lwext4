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
 * @file  ext4_inode.h
 * @brief Inode handle functions
 */

#ifndef EXT4_INODE_H_
#define EXT4_INODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>

#include <stdint.h>

/**@brief Get mode of the i-node.
 * @param sb    Superblock
 * @param inode I-node to load mode from
 * @return Mode of the i-node
 */
uint32_t ext4_inode_get_mode(struct ext4_sblock *sb, struct ext4_inode *inode);

/**@brief Set mode of the i-node.
 * @param sb    Superblock
 * @param inode I-node to set mode to
 * @param mode  Mode to set to i-node
 */
void ext4_inode_set_mode(struct ext4_sblock *sb, struct ext4_inode *inode,
			 uint32_t mode);

/**@brief Get ID of the i-node owner (user id).
 * @param inode I-node to load uid from
 * @return User ID of the i-node owner
 */
uint32_t ext4_inode_get_uid(struct ext4_inode *inode);

/**@brief Set ID of the i-node owner.
 * @param inode I-node to set uid to
 * @param uid   ID of the i-node owner
 */
void ext4_inode_set_uid(struct ext4_inode *inode, uint32_t uid);

/**@brief Get real i-node size.
 * @param sb    Superblock
 * @param inode I-node to load size from
 * @return Real size of i-node
 */
uint64_t ext4_inode_get_size(struct ext4_sblock *sb, struct ext4_inode *inode);

/**@brief Set real i-node size.
 * @param inode I-node to set size to
 * @param size  Size of the i-node
 */
void ext4_inode_set_size(struct ext4_inode *inode, uint64_t size);

/**@brief Get time, when i-node was last accessed.
 * @param inode I-node
 * @return Time of the last access (POSIX)
 */
uint32_t ext4_inode_get_access_time(struct ext4_inode *inode);

/**@brief Set time, when i-node was last accessed.
 * @param inode I-node
 * @param time  Time of the last access (POSIX)
 */
void ext4_inode_set_access_time(struct ext4_inode *inode, uint32_t time);

/**@brief Get time, when i-node was last changed.
 * @param inode I-node
 * @return Time of the last change (POSIX)
 */
uint32_t ext4_inode_get_change_inode_time(struct ext4_inode *inode);

/**@brief Set time, when i-node was last changed.
 * @param inode I-node
 * @param time  Time of the last change (POSIX)
 */
void ext4_inode_set_change_inode_time(struct ext4_inode *inode, uint32_t time);

/**@brief Get time, when i-node content was last modified.
 * @param inode I-node
 * @return Time of the last content modification (POSIX)
 */
uint32_t ext4_inode_get_modif_time(struct ext4_inode *inode);

/**@brief Set time, when i-node content was last modified.
 * @param inode I-node
 * @param time  Time of the last content modification (POSIX)
 */
void ext4_inode_set_modif_time(struct ext4_inode *inode, uint32_t time);

/**@brief Get time, when i-node was deleted.
 * @param inode I-node
 * @return Time of the delete action (POSIX)
 */
uint32_t ext4_inode_get_del_time(struct ext4_inode *inode);

/**@brief Set time, when i-node was deleted.
 * @param inode I-node
 * @param time  Time of the delete action (POSIX)
 */
void ext4_inode_set_del_time(struct ext4_inode *inode, uint32_t time);

/**@brief Get ID of the i-node owner's group.
 * @param inode I-node to load gid from
 * @return Group ID of the i-node owner
 */
uint32_t ext4_inode_get_gid(struct ext4_inode *inode);

/**@brief Set ID to the i-node owner's group.
 * @param inode I-node to set gid to
 * @param gid   Group ID of the i-node owner
 */
void ext4_inode_set_gid(struct ext4_inode *inode, uint32_t gid);

/**@brief Get number of links to i-node.
 * @param inode I-node to load number of links from
 * @return Number of links to i-node
 */
uint16_t ext4_inode_get_links_cnt(struct ext4_inode *inode);

/**@brief Set number of links to i-node.
 * @param inode I-node to set number of links to
 * @param cnt Number of links to i-node
 */
void ext4_inode_set_links_cnt(struct ext4_inode *inode, uint16_t cnt);

/**@brief Get number of 512-bytes blocks used for i-node.
 * @param sb    Superblock
 * @param inode I-node
 * @return Number of 512-bytes blocks
 */
uint64_t ext4_inode_get_blocks_count(struct ext4_sblock *sb,
				     struct ext4_inode *inode);

/**@brief Set number of 512-bytes blocks used for i-node.
 * @param sb    Superblock
 * @param inode I-node
 * @param cnt Number of 512-bytes blocks
 * @return Error code
 */
int ext4_inode_set_blocks_count(struct ext4_sblock *sb,
				struct ext4_inode *inode, uint64_t cnt);

/**@brief Get flags (features) of i-node.
 * @param inode I-node to get flags from
 * @return Flags (bitmap)
 */
uint32_t ext4_inode_get_flags(struct ext4_inode *inode);

/**@brief Set flags (features) of i-node.
 * @param inode I-node to set flags to
 * @param flags Flags to set to i-node
 */
void ext4_inode_set_flags(struct ext4_inode *inode, uint32_t flags);

/**@brief Get file generation (used by NFS).
 * @param inode I-node
 * @return File generation
 */
uint32_t ext4_inode_get_generation(struct ext4_inode *inode);

/**@brief Set file generation (used by NFS).
 * @param inode      I-node
 * @param gen File generation
 */
void ext4_inode_set_generation(struct ext4_inode *inode, uint32_t gen);

/**@brief Get extra I-node size field.
 * @param sb         Superblock
 * @param inode      I-node
 * @return extra I-node size
 */
uint16_t ext4_inode_get_extra_isize(struct ext4_sblock *sb,
				    struct ext4_inode *inode);

/**@brief Set extra I-node size field.
 * @param sb         Superblock
 * @param inode      I-node
 * @param size       extra I-node size
 */
void ext4_inode_set_extra_isize(struct ext4_sblock *sb,
				struct ext4_inode *inode,
				uint16_t size);

/**@brief Get address of block, where are extended attributes located.
 * @param inode I-node
 * @param sb    Superblock
 * @return Block address
 */
uint64_t ext4_inode_get_file_acl(struct ext4_inode *inode,
				 struct ext4_sblock *sb);

/**@brief Set address of block, where are extended attributes located.
 * @param inode    I-node
 * @param sb       Superblock
 * @param acl Block address
 */
void ext4_inode_set_file_acl(struct ext4_inode *inode, struct ext4_sblock *sb,
			     uint64_t acl);

/**@brief Get block address of specified direct block.
 * @param inode I-node to load block from
 * @param idx   Index of logical block
 * @return Physical block address
 */
uint32_t ext4_inode_get_direct_block(struct ext4_inode *inode, uint32_t idx);

/**@brief Set block address of specified direct block.
 * @param inode  I-node to set block address to
 * @param idx    Index of logical block
 * @param block Physical block address
 */
void ext4_inode_set_direct_block(struct ext4_inode *inode, uint32_t idx,
				 uint32_t block);

/**@brief Get block address of specified indirect block.
 * @param inode I-node to get block address from
 * @param idx   Index of indirect block
 * @return Physical block address
 */
uint32_t ext4_inode_get_indirect_block(struct ext4_inode *inode, uint32_t idx);

/**@brief Set block address of specified indirect block.
 * @param inode  I-node to set block address to
 * @param idx    Index of indirect block
 * @param block Physical block address
 */
void ext4_inode_set_indirect_block(struct ext4_inode *inode, uint32_t idx,
				   uint32_t block);

/**@brief Get device number
 * @param inode  I-node to get device number from
 * @return Device number
 */
uint32_t ext4_inode_get_dev(struct ext4_inode *inode);

/**@brief Set device number
 * @param inode  I-node to set device number to
 * @param dev    Device number
 */
void ext4_inode_set_dev(struct ext4_inode *inode, uint32_t dev);

/**@brief return the type of i-node
 * @param sb    Superblock
 * @param inode I-node to return the type of
 * @return Result of check operation
 */
uint32_t ext4_inode_type(struct ext4_sblock *sb, struct ext4_inode *inode);

/**@brief Check if i-node has specified type.
 * @param sb    Superblock
 * @param inode I-node to check type of
 * @param type  Type to check
 * @return Result of check operation
 */
bool ext4_inode_is_type(struct ext4_sblock *sb, struct ext4_inode *inode,
			uint32_t type);

/**@brief Check if i-node has specified flag.
 * @param inode I-node to check flags of
 * @param f  Flag to check
 * @return Result of check operation
 */
bool ext4_inode_has_flag(struct ext4_inode *inode, uint32_t f);

/**@brief Remove specified flag from i-node.
 * @param inode      I-node to clear flag on
 * @param f Flag to be cleared
 */
void ext4_inode_clear_flag(struct ext4_inode *inode, uint32_t f);

/**@brief Set specified flag to i-node.
 * @param inode    I-node to set flag on
 * @param f Flag to be set
 */
void ext4_inode_set_flag(struct ext4_inode *inode, uint32_t f);

/**@brief Get inode checksum(crc32)
 * @param sb    Superblock
 * @param inode I-node to get checksum value from
 */
uint32_t
ext4_inode_get_csum(struct ext4_sblock *sb, struct ext4_inode *inode);

/**@brief Get inode checksum(crc32)
 * @param sb    Superblock
 * @param inode I-node to get checksum value from
 */
void
ext4_inode_set_csum(struct ext4_sblock *sb, struct ext4_inode *inode,
			uint32_t checksum);

/**@brief Check if i-node can be truncated.
 * @param sb    Superblock
 * @param inode I-node to check
 * @return Result of the check operation
 */
bool ext4_inode_can_truncate(struct ext4_sblock *sb, struct ext4_inode *inode);

/**@brief Get extent header from the root of the extent tree.
 * @param inode I-node to get extent header from
 * @return Pointer to extent header of the root node
 */
struct ext4_extent_header *
ext4_inode_get_extent_header(struct ext4_inode *inode);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_INODE_H_ */

/**
 * @}
 */
