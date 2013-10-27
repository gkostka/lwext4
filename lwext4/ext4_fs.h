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

/**@brief   TODO: ...*/
int ext4_fs_init(struct ext4_fs *fs, struct ext4_blockdev *bdev);

/**@brief   TODO: ...*/
int ext4_fs_fini(struct ext4_fs *fs);

/**@brief   TODO: ...*/
int ext4_fs_check_features(struct ext4_fs *fs, bool *read_only);

/**@brief   TODO: ...*/
uint32_t ext4_fs_baddr2_index_in_group(struct ext4_sblock *s, uint32_t baddr);

/**@brief   TODO: ...*/
uint32_t ext4_fs_index_in_group2_baddr(struct ext4_sblock *s, uint32_t index,
    uint32_t bgid);

/**@brief   TODO: ...*/
int ext4_fs_get_block_group_ref(struct ext4_fs *fs, uint32_t bgid,
    struct ext4_block_group_ref *ref);

/**@brief   TODO: ...*/
int ext4_fs_put_block_group_ref(struct ext4_block_group_ref *ref);

/**@brief   TODO: ...*/
int ext4_fs_get_inode_ref(struct ext4_fs *fs, uint32_t index,
    struct ext4_inode_ref *ref);

/**@brief   TODO: ...*/
int ext4_fs_put_inode_ref(struct ext4_inode_ref *ref);

/**@brief   TODO: ...*/
int ext4_fs_alloc_inode(struct ext4_fs *fs, struct ext4_inode_ref *inode_ref,
    bool is_directory);

/**@brief   TODO: ...*/
int ext4_fs_free_inode(struct ext4_inode_ref *inode_ref);

/**@brief   TODO: ...*/
int ext4_fs_truncate_inode(struct ext4_inode_ref *inode_ref,
    uint64_t new_size);

/**@brief   TODO: ...*/
int ext4_fs_get_inode_data_block_index(struct ext4_inode_ref *inode_ref,
    uint64_t iblock, uint32_t *fblock);

/**@brief   TODO: ...*/
int ext4_fs_set_inode_data_block_index(struct ext4_inode_ref *inode_ref,
    uint64_t iblock, uint32_t fblock);

/**@brief   TODO: ...*/
int ext4_fs_release_inode_block(struct ext4_inode_ref *inode_ref,
    uint32_t iblock);

/**@brief   TODO: ...*/
int ext4_fs_append_inode_block(struct ext4_inode_ref *inode_ref,
    uint32_t *fblock, uint32_t *iblock);

#endif /* EXT4_FS_H_ */

/**
 * @}
 */
