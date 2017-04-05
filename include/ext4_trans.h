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
 * @file  ext4_trans.h
 * @brief Transaction handle functions
 */

#ifndef EXT4_TRANS_H
#define EXT4_TRANS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>


/**@brief   Mark a buffer dirty and add it to the current transaction.
 * @param   buf buffer
 * @return  standard error code*/
int ext4_trans_set_block_dirty(struct ext4_buf *buf);

/**@brief   Block get function (through cache, don't read).
 *          jbd_trans_get_access would be called in order to
 *          get write access to the buffer.
 * @param   bdev block device descriptor
 * @param   b block descriptor
 * @param   lba logical block address
 * @return  standard error code*/
int ext4_trans_block_get_noread(struct ext4_blockdev *bdev,
			  struct ext4_block *b,
			  uint64_t lba);

/**@brief   Block get function (through cache).
 *          jbd_trans_get_access would be called in order to
 *          get write access to the buffer.
 * @param   bdev block device descriptor
 * @param   b block descriptor
 * @param   lba logical block address
 * @return  standard error code*/
int ext4_trans_block_get(struct ext4_blockdev *bdev,
		   struct ext4_block *b,
		   uint64_t lba);

/**@brief  Try to add block to be revoked to the current transaction.
 * @param  bdev block device descriptor
 * @param  lba logical block address
 * @return standard error code*/
int ext4_trans_try_revoke_block(struct ext4_blockdev *bdev,
			       uint64_t lba);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_TRANS_H */

/**
 * @}
 */
