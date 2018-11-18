/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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
 * @file  ext4_mkfs.h
 * @brief
 */

#ifndef EXT4_MKFS_H_
#define EXT4_MKFS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>

#include <ext4_blockdev.h>
#include <ext4_fs.h>

#include <stdbool.h>
#include <stdint.h>

struct ext4_mkfs_info {
	uint64_t len;
	uint32_t block_size;
	uint32_t blocks_per_group;
	uint32_t inodes_per_group;
	uint32_t inode_size;
	uint32_t inodes;
	uint32_t journal_blocks;
	uint32_t feat_ro_compat;
	uint32_t feat_compat;
	uint32_t feat_incompat;
	uint32_t bg_desc_reserve_blocks;
	uint16_t dsc_size;
	uint8_t uuid[UUID_SIZE];
	bool journal;
	const char *label;
};


int ext4_mkfs_read_info(struct ext4_blockdev *bd, struct ext4_mkfs_info *info);

int ext4_mkfs(struct ext4_fs *fs, struct ext4_blockdev *bd,
	      struct ext4_mkfs_info *info, int fs_type);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_MKFS_H_ */

/**
 * @}
 */
