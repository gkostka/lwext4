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
 * @file  ext4_fs.c
 * @brief More complex filesystem functions.
 */

#ifndef EXT4_FS_H_
#define EXT4_FS_H_

#include <ext4_config.h>
#include <ext4_types.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief Convert block address to relative index in block group.
 * @param sb Superblock pointer
 * @param baddr Block number to convert
 * @return Relative number of block
 */
static inline uint32_t ext4_fs_baddr2_index_in_group(struct ext4_sblock *s,
                                                     uint32_t baddr)
{
    ext4_assert(baddr);
    if (ext4_get32(s, first_data_block))
        baddr--;

    return baddr % ext4_get32(s, blocks_per_group);
}

/**@brief Convert relative block address in group to absolute address.
 * @param s Superblock pointer
 * @param index Relative block address
 * @param bgid Block group
 * @return Absolute block address
 */
static inline uint32_t ext4_fs_index_in_group2_baddr(struct ext4_sblock *s,
                                                     uint32_t index,
                                                     uint32_t bgid)
{
    if (ext4_get32(s, first_data_block))
        index++;

    return ext4_get32(s, blocks_per_group) * bgid + index;
}

/**@brief TODO: */
static inline uint64_t ext4_fs_first_bg_block_no(struct ext4_sblock *s,
                                                 uint32_t bgid)
{
    return (uint64_t)bgid * ext4_get32(s, blocks_per_group) +
           ext4_get32(s, first_data_block);
}

/**@brief Initialize filesystem and read all needed data.
 * @param fs Filesystem instance to be initialized
 * @param bdev Identifier if device with the filesystem
 * @return Error code
 */
int ext4_fs_init(struct ext4_fs *fs, struct ext4_blockdev *bdev);

/**@brief Destroy filesystem instance (used by unmount operation).
 * @param fs Filesystem to be destroyed
 * @return Error code
 */
int ext4_fs_fini(struct ext4_fs *fs);

/**@brief Check filesystem's features, if supported by this driver
 * Function can return EOK and set read_only flag. It mean's that
 * there are some not-supported features, that can cause problems
 * during some write operations.
 * @param fs        Filesystem to be checked
 * @param read_only Flag if filesystem should be mounted only for reading
 * @return Error code
 */
int ext4_fs_check_features(struct ext4_fs *fs, bool *read_only);

/**@brief Get reference to block group specified by index.
 * @param fs   Filesystem to find block group on
 * @param bgid Index of block group to load
 * @param ref  Output pointer for reference
 * @return Error code
 */
int ext4_fs_get_block_group_ref(struct ext4_fs *fs, uint32_t bgid,
                                struct ext4_block_group_ref *ref);

/**@brief Put reference to block group.
 * @param ref Pointer for reference to be put back
 * @return Error code
 */
int ext4_fs_put_block_group_ref(struct ext4_block_group_ref *ref);

/**@brief Get reference to i-node specified by index.
 * @param fs    Filesystem to find i-node on
 * @param index Index of i-node to load
 * @param ref   Output pointer for reference
 * @return Error code
 */
int ext4_fs_get_inode_ref(struct ext4_fs *fs, uint32_t index,
                          struct ext4_inode_ref *ref);

/**@brief Put reference to i-node.
 * @param ref Pointer for reference to be put back
 * @return Error code
 */
int ext4_fs_put_inode_ref(struct ext4_inode_ref *ref);

/**@brief Allocate new i-node in the filesystem.
 * @param fs        Filesystem to allocated i-node on
 * @param inode_ref Output pointer to return reference to allocated i-node
 * @param flags     Flags to be set for newly created i-node
 * @return Error code
 */
int ext4_fs_alloc_inode(struct ext4_fs *fs, struct ext4_inode_ref *inode_ref,
                        bool is_directory);

/**@brief Release i-node and mark it as free.
 * @param inode_ref I-node to be released
 * @return Error code
 */
int ext4_fs_free_inode(struct ext4_inode_ref *inode_ref);

/**@brief Truncate i-node data blocks.
 * @param inode_ref I-node to be truncated
 * @param new_size  New size of inode (must be < current size)
 * @return Error code
 */
int ext4_fs_truncate_inode(struct ext4_inode_ref *inode_ref, uint64_t new_size);

/**@brief Get physical block address by logical index of the block.
 * @param inode_ref I-node to read block address from
 * @param iblock    Logical index of block
 * @param fblock    Output pointer for return physical block address
 * @return Error code
 */
int ext4_fs_get_inode_data_block_index(struct ext4_inode_ref *inode_ref,
                                       uint64_t iblock, uint32_t *fblock);

/**@brief Set physical block address for the block logical address into the
 * i-node.
 * @param inode_ref I-node to set block address to
 * @param iblock    Logical index of block
 * @param fblock    Physical block address
 * @return Error code
 */
int ext4_fs_set_inode_data_block_index(struct ext4_inode_ref *inode_ref,
                                       uint64_t iblock, uint32_t fblock);

/**@brief Release data block from i-node
 * @param inode_ref I-node to release block from
 * @param iblock    Logical block to be released
 * @return Error code
 */
int ext4_fs_release_inode_block(struct ext4_inode_ref *inode_ref,
                                uint32_t iblock);

/**@brief Append following logical block to the i-node.
 * @param inode_ref I-node to append block to
 * @param fblock    Output physical block address of newly allocated block
 * @param iblock    Output logical number of newly allocated block
 * @return Error code
 */
int ext4_fs_append_inode_block(struct ext4_inode_ref *inode_ref,
                               uint32_t *fblock, uint32_t *iblock);

/**@brief   Increment inode link count.
 * @param   inode none handle
 */
void ext4_fs_inode_links_count_inc(struct ext4_inode_ref *inode_ref);

/**@brief   Decrement inode link count.
 * @param   inode none handle
 */
void ext4_fs_inode_links_count_dec(struct ext4_inode_ref *inode_ref);

#endif /* EXT4_FS_H_ */

/**
 * @}
 */
