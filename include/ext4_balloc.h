/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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
 * @file  ext4_balloc.h
 * @brief Physical block allocator.
 */

#ifndef EXT4_BALLOC_H_
#define EXT4_BALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>

#include <ext4_fs.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief Compute number of block group from block address.
 * @param s superblock pointer.
 * @param baddr Absolute address of block.
 * @return Block group index
 */
uint32_t ext4_balloc_get_bgid_of_block(struct ext4_sblock *s,
				       ext4_fsblk_t baddr);

/**@brief Compute the starting block address of a block group
 * @param s   superblock pointer.
 * @param bgid block group index
 * @return Block address
 */
ext4_fsblk_t ext4_balloc_get_block_of_bgid(struct ext4_sblock *s,
					   uint32_t bgid);

/**@brief Calculate and set checksum of block bitmap.
 * @param sb superblock pointer.
 * @param bg block group
 * @param bitmap bitmap buffer
 */
void ext4_balloc_set_bitmap_csum(struct ext4_sblock *sb,
				 struct ext4_bgroup *bg,
				 void *bitmap);

/**@brief   Free block from inode.
 * @param   inode_ref inode reference
 * @param   baddr block address
 * @return  standard error code*/
int ext4_balloc_free_block(struct ext4_inode_ref *inode_ref,
			   ext4_fsblk_t baddr);

/**@brief   Free blocks from inode.
 * @param   inode_ref inode reference
 * @param   first block address
 * @param   count block count
 * @return  standard error code*/
int ext4_balloc_free_blocks(struct ext4_inode_ref *inode_ref,
			    ext4_fsblk_t first, uint32_t count);

/**@brief   Allocate block procedure.
 * @param   inode_ref inode reference
 * @param   baddr allocated block address
 * @return  standard error code*/
int ext4_balloc_alloc_block(struct ext4_inode_ref *inode_ref,
			    ext4_fsblk_t goal,
			    ext4_fsblk_t *baddr);

/**@brief   Try allocate selected block.
 * @param   inode_ref inode reference
 * @param   baddr block address to allocate
 * @param   free if baddr is not allocated
 * @return  standard error code*/
int ext4_balloc_try_alloc_block(struct ext4_inode_ref *inode_ref,
				ext4_fsblk_t baddr, bool *free);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_BALLOC_H_ */

/**
 * @}
 */
