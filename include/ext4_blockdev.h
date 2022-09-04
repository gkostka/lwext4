/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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
 * @file  ext4_blockdev.h
 * @brief Block device module.
 */

#ifndef EXT4_BLOCKDEV_H_
#define EXT4_BLOCKDEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_bcache.h>

#include <stdbool.h>
#include <stdint.h>

struct ext4_blockdev_iface {
	/**@brief   Open device function
	 * @param   bdev block device.*/
	int (*open)(struct ext4_blockdev *bdev);

	/**@brief   Block read function.
	 * @param   bdev block device
	 * @param   buf output buffer
	 * @param   blk_id block id
	 * @param   blk_cnt block count*/
	int (*bread)(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		     uint32_t blk_cnt);

	/**@brief   Block write function.
	 * @param   buf input buffer
	 * @param   blk_id block id
	 * @param   blk_cnt block count*/
	int (*bwrite)(struct ext4_blockdev *bdev, const void *buf,
		      uint64_t blk_id, uint32_t blk_cnt);

	/**@brief   Close device function.
	 * @param   bdev block device.*/
	int (*close)(struct ext4_blockdev *bdev);

	/**@brief   Lock block device. Required in multi partition mode
	 *          operations. Not mandatory field.
	 * @param   bdev block device.*/
	int (*lock)(struct ext4_blockdev *bdev);

	/**@brief   Unlock block device. Required in multi partition mode
	 *          operations. Not mandatory field.
	 * @param   bdev block device.*/
	int (*unlock)(struct ext4_blockdev *bdev);

	/**@brief   Block size (bytes): physical*/
	uint32_t ph_bsize;

	/**@brief   Block count: physical*/
	uint64_t ph_bcnt;

	/**@brief   Block size buffer: physical*/
	uint8_t *ph_bbuf;

	/**@brief   Reference counter to block device interface*/
	uint32_t ph_refctr;

	/**@brief   Physical read counter*/
	uint32_t bread_ctr;

	/**@brief   Physical write counter*/
	uint32_t bwrite_ctr;

	/**@brief   User data pointer*/
	void* p_user;
};

/**@brief   Definition of the simple block device.*/
struct ext4_blockdev {
	/**@brief Block device interface*/
	struct ext4_blockdev_iface *bdif;

	/**@brief Offset in bdif. For multi partition mode.*/
	uint64_t part_offset;

	/**@brief Part size in bdif. For multi partition mode.*/
	uint64_t part_size;

	/**@brief   Block cache.*/
	struct ext4_bcache *bc;

	/**@brief   Block size (bytes) logical*/
	uint32_t lg_bsize;

	/**@brief   Block count: logical*/
	uint64_t lg_bcnt;

	/**@brief   Cache write back mode reference counter*/
	uint32_t cache_write_back;

	/**@brief   The filesystem this block device belongs to. */
	struct ext4_fs *fs;

	void *journal;
};

/**@brief   Static initialization of the block device.*/
#define EXT4_BLOCKDEV_STATIC_INSTANCE(__name, __bsize, __bcnt, __open, __bread,\
				      __bwrite, __close, __lock, __unlock)     \
	static uint8_t __name##_ph_bbuf[(__bsize)];                            \
	static struct ext4_blockdev_iface __name##_iface = {                   \
		.open = __open,                                                \
		.bread = __bread,                                              \
		.bwrite = __bwrite,                                            \
		.close = __close,                                              \
		.lock = __lock,                                                \
		.unlock = __unlock,                                            \
		.ph_bsize = __bsize,                                           \
		.ph_bcnt = __bcnt,                                             \
		.ph_bbuf = __name##_ph_bbuf,                                   \
	};								       \
	static struct ext4_blockdev __name = {                                 \
		.bdif = &__name##_iface,                                       \
		.part_offset = 0,                                              \
		.part_size =  (__bcnt) * (__bsize),                            \
	}

/**@brief   Block device initialization.
 * @param   bdev block device descriptor
 * @return  standard error code*/
int ext4_block_init(struct ext4_blockdev *bdev);

/**@brief   Binds a bcache to block device.
 * @param   bdev block device descriptor
 * @param   bc block cache descriptor
 * @return  standard error code*/
int ext4_block_bind_bcache(struct ext4_blockdev *bdev, struct ext4_bcache *bc);

/**@brief   Close block device
 * @param   bdev block device descriptor
 * @return  standard error code*/
int ext4_block_fini(struct ext4_blockdev *bdev);

/**@brief   Flush data in given buffer to disk.
 * @param   bdev block device descriptor
 * @param   buf buffer
 * @return  standard error code*/
int ext4_block_flush_buf(struct ext4_blockdev *bdev, struct ext4_buf *buf);

/**@brief   Flush data in buffer of given lba to disk,
 *          if that buffer exists in block cache.
 * @param   bdev block device descriptor
 * @param   lba logical block address
 * @return  standard error code*/
int ext4_block_flush_lba(struct ext4_blockdev *bdev, uint64_t lba);

/**@brief   Set logical block size in block device.
 * @param   bdev block device descriptor
 * @param   lb_bsize logical block size (in bytes)*/
void ext4_block_set_lb_size(struct ext4_blockdev *bdev, uint32_t lb_bsize);

/**@brief   Block get function (through cache, don't read).
 * @param   bdev block device descriptor
 * @param   b block descriptor
 * @param   lba logical block address
 * @return  standard error code*/
int ext4_block_get_noread(struct ext4_blockdev *bdev, struct ext4_block *b,
			  uint64_t lba);

/**@brief   Block get function (through cache).
 * @param   bdev block device descriptor
 * @param   b block descriptor
 * @param   lba logical block address
 * @return  standard error code*/
int ext4_block_get(struct ext4_blockdev *bdev, struct ext4_block *b,
		   uint64_t lba);

/**@brief   Block set procedure (through cache).
 * @param   bdev block device descriptor
 * @param   b block descriptor
 * @return  standard error code*/
int ext4_block_set(struct ext4_blockdev *bdev, struct ext4_block *b);

/**@brief   Block read procedure (without cache)
 * @param   bdev block device descriptor
 * @param   buf output buffer
 * @param   lba logical block address
 * @return  standard error code*/
int ext4_blocks_get_direct(struct ext4_blockdev *bdev, void *buf, uint64_t lba,
			   uint32_t cnt);

/**@brief   Block write procedure (without cache)
 * @param   bdev block device descriptor
 * @param   buf output buffer
 * @param   lba logical block address
 * @return  standard error code*/
int ext4_blocks_set_direct(struct ext4_blockdev *bdev, const void *buf,
			   uint64_t lba, uint32_t cnt);

/**@brief   Write to block device (by direct address).
 * @param   bdev block device descriptor
 * @param   off byte offset in block device
 * @param   buf input buffer
 * @param   len length of the write buffer
 * @return  standard error code*/
int ext4_block_writebytes(struct ext4_blockdev *bdev, uint64_t off,
			  const void *buf, uint32_t len);

/**@brief   Read freom block device (by direct address).
 * @param   bdev block device descriptor
 * @param   off byte offset in block device
 * @param   buf input buffer
 * @param   len length of the write buffer
 * @return  standard error code*/
int ext4_block_readbytes(struct ext4_blockdev *bdev, uint64_t off, void *buf,
			 uint32_t len);

/**@brief   Flush all dirty buffers to disk
 * @param   bdev block device descriptor
 * @return  standard error code*/
int ext4_block_cache_flush(struct ext4_blockdev *bdev);

/**@brief   Enable/disable write back cache mode
 * @param   bdev block device descriptor
 * @param   on_off
 *              !0 - ENABLE
 *               0 - DISABLE (all delayed cache buffers will be flushed)
 * @return  standard error code*/
int ext4_block_cache_write_back(struct ext4_blockdev *bdev, uint8_t on_off);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_BLOCKDEV_H_ */

/**
 * @}
 */
