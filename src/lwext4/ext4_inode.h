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

#include <ext4_config.h>
#include <stdint.h>

uint32_t ext4_inode_get_mode(struct ext4_sblock *sb, struct ext4_inode *inode);
void 	 ext4_inode_set_mode(struct ext4_sblock *sb, struct ext4_inode *inode, uint32_t mode);


uint32_t ext4_inode_get_uid(struct ext4_inode *inode);
void 	 ext4_inode_set_uid(struct ext4_inode *inode, uint32_t uid);


uint64_t ext4_inode_get_size(struct ext4_sblock *sb, struct ext4_inode *inode);
void 	 ext4_inode_set_size(struct ext4_inode *inode, uint64_t size);


uint32_t ext4_inode_get_access_time(struct ext4_inode *inode);
void 	 ext4_inode_set_access_time(struct ext4_inode *inode, uint32_t time);


uint32_t ext4_inode_get_change_inode_time(struct ext4_inode *inode);
void 	 ext4_inode_set_change_inode_time(struct ext4_inode *inode, uint32_t time);


uint32_t ext4_inode_get_modification_time(struct ext4_inode *inode);
void 	 ext4_inode_set_modification_time(struct ext4_inode *inode, uint32_t time);


uint32_t ext4_inode_get_deletion_time(struct ext4_inode *inode);
void 	 ext4_inode_set_deletion_time(struct ext4_inode *inode, uint32_t time);

uint32_t ext4_inode_get_gid(struct ext4_inode *inode);
void 	 ext4_inode_set_gid(struct ext4_inode *inode, uint32_t gid);

uint16_t ext4_inode_get_links_count(struct ext4_inode *inode);
void 	 ext4_inode_set_links_count(struct ext4_inode *inode, uint16_t cnt);


uint64_t ext4_inode_get_blocks_count(struct ext4_sblock *sb,  struct ext4_inode *inode);
int 	 ext4_inode_set_blocks_count(struct ext4_sblock *sb,  struct ext4_inode *inode, uint64_t cnt);


uint32_t ext4_inode_get_flags(struct ext4_inode *inode);
void 	 ext4_inode_set_flags(struct ext4_inode *inode, uint32_t flags);

uint32_t ext4_inode_get_generation(struct ext4_inode *inode);
void 	 ext4_inode_set_generation(struct ext4_inode *inode, uint32_t gen);

uint64_t ext4_inode_get_file_acl(struct ext4_inode *inode, struct ext4_sblock *sb);
void 	 ext4_inode_set_file_acl(struct ext4_inode *inode, struct ext4_sblock *sb, uint64_t acl);


uint32_t ext4_inode_get_direct_block(struct ext4_inode *inode, uint32_t idx);
void 	 ext4_inode_set_direct_block(struct ext4_inode *inode, uint32_t idx, uint32_t block);


uint32_t ext4_inode_get_indirect_block(struct ext4_inode *inode, uint32_t idx);
void	 ext4_inode_set_indirect_block(struct ext4_inode *inode, uint32_t idx, uint32_t block);

bool 	 ext4_inode_is_type(struct ext4_sblock *sb, struct ext4_inode *inode, uint32_t type);


bool 	 ext4_inode_has_flag(struct ext4_inode *inode, uint32_t f);
void 	 ext4_inode_clear_flag(struct ext4_inode *inode, uint32_t f);
void 	 ext4_inode_set_flag(struct ext4_inode *inode, uint32_t f);

bool 	 ext4_inode_can_truncate(struct ext4_sblock *sb, struct ext4_inode *inode);


struct ext4_extent_header * ext4_inode_get_extent_header(struct ext4_inode *inode);

#endif /* EXT4_INODE_H_ */

/**
 * @}
 */

