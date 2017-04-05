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
 * @file  ext4_ialloc.c
 * @brief Inode allocation procedures.
 */

#ifndef EXT4_IALLOC_H_
#define EXT4_IALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>

/**@brief Calculate and set checksum of inode bitmap.
 * @param sb superblock pointer.
 * @param bg block group
 * @param bitmap bitmap buffer
 */
void ext4_ialloc_set_bitmap_csum(struct ext4_sblock *sb, struct ext4_bgroup *bg,
				 void *bitmap);

/**@brief Free i-node number and modify filesystem data structers.
 * @param fs     Filesystem, where the i-node is located
 * @param index  Index of i-node to be release
 * @param is_dir Flag us for information whether i-node is directory or not
 */
int ext4_ialloc_free_inode(struct ext4_fs *fs, uint32_t index, bool is_dir);

/**@brief I-node allocation algorithm.
 * This is more simple algorithm, than Orlov allocator used
 * in the Linux kernel.
 * @param fs     Filesystem to allocate i-node on
 * @param index  Output value - allocated i-node number
 * @param is_dir Flag if allocated i-node will be file or directory
 * @return Error code
 */
int ext4_ialloc_alloc_inode(struct ext4_fs *fs, uint32_t *index, bool is_dir);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_IALLOC_H_ */

/**
 * @}
 */
