/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * Copyright (c) 2015 Kaho Ng (ngkaho1234@gmail.com)
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
 * @file  ext4_journal.h
 * @brief Journal handle functions
 */

#ifndef EXT4_JOURNAL_H_
#define EXT4_JOURNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>
#include <misc/queue.h>
#include <misc/tree.h>

struct jbd_fs {
	struct ext4_blockdev *bdev;
	struct ext4_inode_ref inode_ref;
	struct jbd_sb sb;

	bool dirty;
};

struct jbd_buf {
	uint32_t jbd_lba;
	struct ext4_block block;
	struct jbd_trans *trans;
	struct jbd_block_rec *block_rec;
	TAILQ_ENTRY(jbd_buf) buf_node;
	TAILQ_ENTRY(jbd_buf) dirty_buf_node;
};

struct jbd_revoke_rec {
	ext4_fsblk_t lba;
	RB_ENTRY(jbd_revoke_rec) revoke_node;
};

struct jbd_block_rec {
	ext4_fsblk_t lba;
	struct jbd_trans *trans;
	RB_ENTRY(jbd_block_rec) block_rec_node;
	LIST_ENTRY(jbd_block_rec) tbrec_node;
	TAILQ_HEAD(jbd_buf_dirty, jbd_buf) dirty_buf_queue;
};

struct jbd_trans {
	uint32_t trans_id;

	uint32_t start_iblock;
	int alloc_blocks;
	int data_cnt;
	uint32_t data_csum;
	int written_cnt;
	int error;

	struct jbd_journal *journal;

	TAILQ_HEAD(jbd_trans_buf, jbd_buf) buf_queue;
	RB_HEAD(jbd_revoke_tree, jbd_revoke_rec) revoke_root;
	LIST_HEAD(jbd_trans_block_rec, jbd_block_rec) tbrec_list;
	TAILQ_ENTRY(jbd_trans) trans_node;
};

struct jbd_journal {
	uint32_t first;
	uint32_t start;
	uint32_t last;
	uint32_t trans_id;
	uint32_t alloc_trans_id;

	uint32_t block_size;

	TAILQ_HEAD(jbd_cp_queue, jbd_trans) cp_queue;
	RB_HEAD(jbd_block, jbd_block_rec) block_rec_root;

	struct jbd_fs *jbd_fs;
};

int jbd_get_fs(struct ext4_fs *fs,
	       struct jbd_fs *jbd_fs);
int jbd_put_fs(struct jbd_fs *jbd_fs);
int jbd_inode_bmap(struct jbd_fs *jbd_fs,
		   ext4_lblk_t iblock,
		   ext4_fsblk_t *fblock);
int jbd_recover(struct jbd_fs *jbd_fs);
int jbd_journal_start(struct jbd_fs *jbd_fs,
		      struct jbd_journal *journal);
int jbd_journal_stop(struct jbd_journal *journal);
struct jbd_trans *
jbd_journal_new_trans(struct jbd_journal *journal);
int jbd_trans_set_block_dirty(struct jbd_trans *trans,
			      struct ext4_block *block);
int jbd_trans_revoke_block(struct jbd_trans *trans,
			   ext4_fsblk_t lba);
int jbd_trans_try_revoke_block(struct jbd_trans *trans,
			       ext4_fsblk_t lba);
void jbd_journal_free_trans(struct jbd_journal *journal,
			    struct jbd_trans *trans,
			    bool abort);
int jbd_journal_commit_trans(struct jbd_journal *journal,
			     struct jbd_trans *trans);
void
jbd_journal_purge_cp_trans(struct jbd_journal *journal,
			   bool flush,
			   bool once);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_JOURNAL_H_ */

/**
 * @}
 */
