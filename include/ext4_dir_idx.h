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

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>

#include <ext4_fs.h>
#include <ext4_dir.h>

#include <stdint.h>
#include <stdbool.h>

struct ext4_dir_idx_block {
	struct ext4_block b;
	struct ext4_dir_idx_entry *entries;
	struct ext4_dir_idx_entry *position;
};

#define EXT4_DIR_DX_INIT_BCNT 2


/**@brief Initialize index structure of new directory.
 * @param dir Pointer to directory i-node
 * @param parent Pointer to parent directory i-node
 * @return Error code
 */
int ext4_dir_dx_init(struct ext4_inode_ref *dir,
		     struct ext4_inode_ref *parent);

/**@brief Try to find directory entry using directory index.
 * @param result    Output value - if entry will be found,
 *                  than will be passed through this parameter
 * @param inode_ref Directory i-node
 * @param name_len  Length of name to be found
 * @param name      Name to be found
 * @return Error code
 */
int ext4_dir_dx_find_entry(struct ext4_dir_search_result *result,
			   struct ext4_inode_ref *inode_ref, size_t name_len,
			   const char *name);

/**@brief Add new entry to indexed directory
 * @param parent Directory i-node
 * @param child  I-node to be referenced from directory entry
 * @param name   Name of new directory entry
 * @return Error code
 */
int ext4_dir_dx_add_entry(struct ext4_inode_ref *parent,
			  struct ext4_inode_ref *child, const char *name, uint32_t name_len);

/**@brief Add new entry to indexed directory
 * @param dir           Directory i-node
 * @param parent_inode  parent inode index
 * @return Error code
 */
int ext4_dir_dx_reset_parent_inode(struct ext4_inode_ref *dir,
                                   uint32_t parent_inode);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_DIR_IDX_H_ */

/**
 * @}
 */
