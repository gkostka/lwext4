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
 * @file  ext4_dir_idx.h
 * @brief Directory indexing procedures.
 */

#ifndef EXT4_DIR_IDX_H_
#define EXT4_DIR_IDX_H_

#include <ext4_config.h>
#include <ext4_types.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief Get hash version used in directory index.
 * @param root_info Pointer to root info structure of index
 * @return Hash algorithm version
 */
uint8_t ext4_dir_dx_root_info_get_hash_version(
    struct ext4_directory_dx_root_info *root_info);

/**@brief Set hash version, that will be used in directory index.
 * @param root_info Pointer to root info structure of index
 * @param v Hash algorithm version
 */
void ext4_dir_dx_root_info_set_hash_version(
    struct ext4_directory_dx_root_info  *root_info, uint8_t v);

/**@brief Get length of root_info structure in bytes.
 * @param root_info Pointer to root info structure of index
 * @return Length of the structure
 */
uint8_t ext4_dir_dx_root_info_get_info_length(
    struct ext4_directory_dx_root_info *root_info);

/**@brief Set length of root_info structure in bytes.
 * @param root_info   Pointer to root info structure of index
 * @param info_length Length of the structure
 */
void ext4_dir_dx_root_info_set_info_length(
    struct ext4_directory_dx_root_info  *root_info, uint8_t len);

/**@brief Get number of indirect levels of HTree.
 * @param root_info Pointer to root info structure of index
 * @return Height of HTree (actually only 0 or 1)
 */
uint8_t ext4_dir_dx_root_info_get_indirect_levels(
    struct ext4_directory_dx_root_info *root_info);

/**@brief Set number of indirect levels of HTree.
 * @param root_info Pointer to root info structure of index
 * @param lvl Height of HTree (actually only 0 or 1)
 */
void ext4_dir_dx_root_info_set_indirect_levels(
    struct ext4_directory_dx_root_info *root_info, uint8_t lvl);

/**@brief Get maximum number of index node entries.
 * @param climit Pointer to counlimit structure
 * @return Maximum of entries in node
 */
uint16_t ext4_dir_dx_countlimit_get_limit(
    struct ext4_directory_dx_countlimit *climit);

/**@brief Set maximum number of index node entries.
 * @param climit Pointer to counlimit structure
 * @param limit Maximum of entries in node
 */
void ext4_dir_dx_countlimit_set_limit(
    struct ext4_directory_dx_countlimit *climit, uint16_t limit);

/**@brief Get current number of index node entries.
 * @param climit Pointer to counlimit structure
 * @return Number of entries in node
 */
uint16_t ext4_dir_dx_countlimit_get_count(
    struct ext4_directory_dx_countlimit *climit);

/**@brief Set current number of index node entries.
 * @param climit Pointer to counlimit structure
 * @param count Number of entries in node
 */
void ext4_dir_dx_countlimit_set_count(
    struct ext4_directory_dx_countlimit *climit, uint16_t count);

/**@brief Get hash value of index entry.
 * @param entry Pointer to index entry
 * @return Hash value
 */
uint32_t ext4_dir_dx_entry_get_hash(
    struct ext4_directory_dx_entry *entry);

/**@brief Set hash value of index entry.
 * @param entry Pointer to index entry
 * @param hash  Hash value
 */
void ext4_dir_dx_entry_set_hash(
    struct ext4_directory_dx_entry *entry, uint32_t hash);

/**@brief Get block address where child node is located.
 * @param entry Pointer to index entry
 * @return Block address of child node
 */
uint32_t ext4_dir_dx_entry_get_block(
    struct ext4_directory_dx_entry *entry);

/**@brief Set block address where child node is located.
 * @param entry Pointer to index entry
 * @param block Block address of child node
 */
void ext4_dir_dx_entry_set_block(
    struct ext4_directory_dx_entry *entry, uint32_t block);

/**@brief Initialize index structure of new directory.
 * @param dir Pointer to directory i-node
 * @return Error code
 */
int ext4_dir_dx_init(struct ext4_inode_ref *dir);

/**@brief Try to find directory entry using directory index.
 * @param result    Output value - if entry will be found,
 *                  than will be passed through this parameter
 * @param inode_ref Directory i-node
 * @param name_len  Length of name to be found
 * @param name      Name to be found
 * @return Error code
 */
int ext4_dir_dx_find_entry(struct ext4_directory_search_result * result,
    struct ext4_inode_ref *inode_ref, size_t name_len, const char *name);

/**@brief Add new entry to indexed directory
 * @param parent Directory i-node
 * @param child  I-node to be referenced from directory entry
 * @param name   Name of new directory entry
 * @return Error code
 */
int ext4_dir_dx_add_entry(struct ext4_inode_ref *parent,
    struct ext4_inode_ref *child, const char *name);

#endif /* EXT4_DIR_IDX_H_ */

/**
 * @}
 */

