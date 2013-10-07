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
 * @file  ext4_dir.h
 * @brief Directory handle procedures.
 */

#ifndef EXT4_DIR_H_
#define EXT4_DIR_H_

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_blockdev.h>
#include <ext4_super.h>


#include <stdint.h>

uint32_t ext4_dir_entry_ll_get_inode(struct ext4_directory_entry_ll *de);

void 	 ext4_dir_entry_ll_set_inode(struct ext4_directory_entry_ll *de,
		uint32_t inode);


uint16_t ext4_dir_entry_ll_get_entry_length(struct ext4_directory_entry_ll *de);
void 	 ext4_dir_entry_ll_set_entry_length(struct ext4_directory_entry_ll *de,
		uint16_t len);


uint16_t ext4_dir_entry_ll_get_name_length(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de);
void 	ext4_dir_entry_ll_set_name_length(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de, uint16_t len);



uint8_t ext4_dir_entry_ll_get_inode_type(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de);
void 	ext4_dir_entry_ll_set_inode_type(struct ext4_sblock *sb,
		struct ext4_directory_entry_ll *de, uint8_t type);


int ext4_dir_iterator_init(struct ext4_directory_iterator *it,
    struct ext4_inode_ref *inode_ref, uint64_t pos);

int ext4_dir_iterator_next(struct ext4_directory_iterator *it);
int ext4_dir_iterator_fini(struct ext4_directory_iterator *it);

void ext4_dir_write_entry(struct ext4_sblock *sb, struct ext4_directory_entry_ll *entry,
		uint16_t entry_len, struct ext4_inode_ref *child,  const char *name, size_t name_len);

int  ext4_dir_add_entry(struct ext4_inode_ref *parent, const char *name,
		struct ext4_inode_ref *child);

int ext4_dir_find_entry(struct ext4_directory_search_result *result,
    struct ext4_inode_ref *parent, const char *name);

int ext4_dir_remove_entry(struct ext4_inode_ref *parent, const char *name);

int ext4_dir_try_insert_entry(struct ext4_sblock *sb, struct ext4_block *target_block,
    struct ext4_inode_ref *child, const char *name, uint32_t name_len);

int ext4_dir_find_in_block(struct ext4_block *block, struct ext4_sblock *sb,
		size_t name_len, const char *name, struct ext4_directory_entry_ll **res_entry);

int ext4_dir_destroy_result(struct ext4_inode_ref *parent, struct ext4_directory_search_result *result);


#endif /* EXT4_DIR_H_ */

/**
 * @}
 */


