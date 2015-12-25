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

#include "ext4_config.h"
#include "ext4_types.h"

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
struct jbd_trans *jbd_journal_new_trans(struct jbd_journal *journal);
int jbd_trans_get_access(struct jbd_journal *journal,
			 struct jbd_trans *trans,
			 struct ext4_block *block);
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
void jbd_journal_submit_trans(struct jbd_journal *journal,
			      struct jbd_trans *trans);
void jbd_journal_commit_one(struct jbd_journal *journal);
void jbd_journal_commit_all(struct jbd_journal *journal);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_JOURNAL_H_ */

/**
 * @}
 */
