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

/**@brief   TODO: ...*/
uint8_t ext4_dir_dx_root_info_get_hash_version(
    struct ext4_directory_dx_root_info *root_info);

/**@brief   TODO: ...*/
void ext4_dir_dx_root_info_set_hash_version(
    struct ext4_directory_dx_root_info  *root_info, uint8_t v);

/**@brief   TODO: ...*/
uint8_t ext4_dir_dx_root_info_get_info_length(
    struct ext4_directory_dx_root_info *root_info);

/**@brief   TODO: ...*/
void ext4_dir_dx_root_info_set_info_length(
    struct ext4_directory_dx_root_info  *root_info, uint8_t len);

/**@brief   TODO: ...*/
uint8_t ext4_dir_dx_root_info_get_indirect_levels(
    struct ext4_directory_dx_root_info *root_info);

/**@brief   TODO: ...*/
void ext4_dir_dx_root_info_set_indirect_levels(
    struct ext4_directory_dx_root_info *root_info, uint8_t lvl);

/**@brief   TODO: ...*/
uint16_t ext4_dir_dx_countlimit_get_limit(
    struct ext4_directory_dx_countlimit *climit);

/**@brief   TODO: ...*/
void ext4_dir_dx_countlimit_set_limit(
    struct ext4_directory_dx_countlimit *climit, uint16_t limit);

/**@brief   TODO: ...*/
uint16_t ext4_dir_dx_countlimit_get_count(
    struct ext4_directory_dx_countlimit *climit);

/**@brief   TODO: ...*/
void ext4_dir_dx_countlimit_set_count(
    struct ext4_directory_dx_countlimit *climit, uint16_t count);

/**@brief   TODO: ...*/
uint32_t ext4_dir_dx_entry_get_hash(
    struct ext4_directory_dx_entry *entry);

/**@brief   TODO: ...*/
void ext4_dir_dx_entry_set_hash(
    struct ext4_directory_dx_entry *entry, uint32_t hash);

/**@brief   TODO: ...*/
uint32_t ext4_dir_dx_entry_get_block(
    struct ext4_directory_dx_entry *entry);

/**@brief   TODO: ...*/
void ext4_dir_dx_entry_set_block(
    struct ext4_directory_dx_entry *entry, uint32_t block);

/**@brief   TODO: ...*/
int ext4_dir_dx_init(struct ext4_inode_ref *dir);

/**@brief   TODO: ...*/
int ext4_dir_dx_find_entry(struct ext4_directory_search_result * result,
    struct ext4_inode_ref *inode_ref, size_t name_len, const char *name);

/**@brief   TODO: ...*/
int ext4_dir_dx_add_entry(struct ext4_inode_ref *parent,
    struct ext4_inode_ref *child, const char *name);

#endif /* EXT4_DIR_IDX_H_ */

/**
 * @}
 */

