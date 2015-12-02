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
 * @file  ext4_blockdev.c
 * @brief Block device module.
 */

#include "ext4_config.h"
#include "ext4_blockdev.h"
#include "ext4_errno.h"
#include "ext4_debug.h"

#include <string.h>
#include <stdlib.h>

int ext4_block_init(struct ext4_blockdev *bdev)
{
	int rc;
	ext4_assert(bdev);

	ext4_assert(bdev->open && bdev->close && bdev->bread && bdev->bwrite);

	/*Low level block init*/
	rc = bdev->open(bdev);
	if (rc != EOK)
		return rc;

	bdev->flags |= EXT4_BDEV_INITIALIZED;

	return EOK;
}

int ext4_block_bind_bcache(struct ext4_blockdev *bdev, struct ext4_bcache *bc)
{
	ext4_assert(bdev && bc);
	bdev->bc = bc;
	bc->bdev = bdev;
	return EOK;
}

void ext4_block_set_lb_size(struct ext4_blockdev *bdev, uint64_t lb_bsize)
{
	/*Logical block size has to be multiply of physical */
	ext4_assert(!(lb_bsize % bdev->ph_bsize));

	bdev->lg_bsize = lb_bsize;
	bdev->lg_bcnt = (bdev->ph_bcnt * bdev->ph_bsize) / lb_bsize;
}

int ext4_block_fini(struct ext4_blockdev *bdev)
{
	ext4_assert(bdev);

	bdev->flags &= ~(EXT4_BDEV_INITIALIZED);

	/*Low level block fini*/
	return bdev->close(bdev);
}

int ext4_block_flush_buf(struct ext4_blockdev *bdev, struct ext4_buf *buf)
{
	int r;
	struct ext4_bcache *bc = bdev->bc;
	/*Only flushing unreferenced buffer is allowed.*/
	ext4_assert(!buf->refctr);

	if (ext4_bcache_test_flag(buf, BC_DIRTY)) {
		r = ext4_blocks_set_direct(bdev, buf->data, buf->lba, 1);
		if (r)
			return r;

		ext4_bcache_remove_dirty_node(bc, buf);
		ext4_bcache_clear_flag(buf, BC_DIRTY);
	}
	return EOK;
}

int ext4_block_cache_shake(struct ext4_blockdev *bdev)
{
	struct ext4_buf *buf;
	while (!RB_EMPTY(&bdev->bc->lru_root) &&
		ext4_bcache_is_full(bdev->bc)) {
		
		buf = ext4_buf_lowest_lru(bdev->bc);
		ext4_assert(buf);
		if (ext4_bcache_test_flag(buf, BC_DIRTY)) {
			int r = ext4_block_flush_buf(bdev, buf);
			if (r != EOK)
				return r;

		}

		ext4_bcache_drop_buf(bdev->bc, buf);
	}
	return EOK;
}

int ext4_block_get_noread(struct ext4_blockdev *bdev, struct ext4_block *b,
			  uint64_t lba)
{
	bool is_new;
	int r;

	ext4_assert(bdev && b);

	if (!(bdev->flags & EXT4_BDEV_INITIALIZED))
		return EIO;

	if (!(lba < bdev->lg_bcnt))
		return ERANGE;

	b->dirty = 0;
	b->lb_id = lba;

	/*If cache is full we have to (flush and) drop it anyway :(*/
	r = ext4_block_cache_shake(bdev);
	if (r != EOK)
		return r;

	r = ext4_bcache_alloc(bdev->bc, b, &is_new);
	if (r != EOK)
		return r;

	if (!b->data)
		return ENOMEM;

	return EOK;
}

int ext4_block_get(struct ext4_blockdev *bdev, struct ext4_block *b,
		   uint64_t lba)
{
	uint64_t pba;
	uint32_t pb_cnt;
	int r = ext4_block_get_noread(bdev, b, lba);
	if (r != EOK)
		return r;

	if (b->uptodate) {
		/* Data in the cache is up-to-date.
		 * Reading from physical device is not required */
		return EOK;
	}

	pba = (lba * bdev->lg_bsize) / bdev->ph_bsize;
	pb_cnt = bdev->lg_bsize / bdev->ph_bsize;

	r = bdev->bread(bdev, b->data, pba, pb_cnt);

	if (r != EOK) {
		ext4_bcache_free(bdev->bc, b);
		b->lb_id = 0;
		return r;
	}

	/* Mark buffer up-to-date, since
	 * fresh data is read from physical device just now. */
	ext4_bcache_set_flag(b->buf, BC_UPTODATE);
	b->uptodate = true;
	bdev->bread_ctr++;
	return EOK;
}

int ext4_block_set(struct ext4_blockdev *bdev, struct ext4_block *b)
{
	ext4_assert(bdev && b);
	ext4_assert(b->buf);

	if (!(bdev->flags & EXT4_BDEV_INITIALIZED))
		return EIO;

	return ext4_bcache_free(bdev->bc, b);
}

int ext4_blocks_get_direct(struct ext4_blockdev *bdev, void *buf, uint64_t lba,
			   uint32_t cnt)
{
	uint64_t pba;
	uint32_t pb_cnt;

	ext4_assert(bdev && buf);

	pba = (lba * bdev->lg_bsize) / bdev->ph_bsize;
	pb_cnt = bdev->lg_bsize / bdev->ph_bsize;

	bdev->bread_ctr++;
	return bdev->bread(bdev, buf, pba, pb_cnt * cnt);
}

int ext4_blocks_set_direct(struct ext4_blockdev *bdev, const void *buf,
			   uint64_t lba, uint32_t cnt)
{
	uint64_t pba;
	uint32_t pb_cnt;

	ext4_assert(bdev && buf);

	pba = (lba * bdev->lg_bsize) / bdev->ph_bsize;
	pb_cnt = bdev->lg_bsize / bdev->ph_bsize;

	bdev->bwrite_ctr++;

	return bdev->bwrite(bdev, buf, pba, pb_cnt * cnt);
}

int ext4_block_writebytes(struct ext4_blockdev *bdev, uint64_t off,
			  const void *buf, uint32_t len)
{
	uint64_t block_idx;
	uint64_t block_end;
	uint32_t blen;
	uint32_t unalg;
	int r = EOK;

	const uint8_t *p = (void *)buf;

	ext4_assert(bdev && buf);

	if (!(bdev->flags & EXT4_BDEV_INITIALIZED))
		return EIO;

	block_idx = off / bdev->ph_bsize;
	block_end = block_idx + len / bdev->ph_bsize;

	if (!(block_end < bdev->ph_bcnt))
		return EINVAL; /*Ups. Out of range operation*/

	/*OK lets deal with the first possible unaligned block*/
	unalg = (off & (bdev->ph_bsize - 1));
	if (unalg) {

		uint32_t wlen = (bdev->ph_bsize - unalg) > len
				    ? len
				    : (bdev->ph_bsize - unalg);

		r = bdev->bread(bdev, bdev->ph_bbuf, block_idx, 1);
		if (r != EOK)
			return r;

		memcpy(bdev->ph_bbuf + unalg, p, wlen);

		r = bdev->bwrite(bdev, bdev->ph_bbuf, block_idx, 1);
		if (r != EOK)
			return r;

		p += wlen;
		len -= wlen;
		block_idx++;
	}

	/*Aligned data*/
	blen = len / bdev->ph_bsize;
	r = bdev->bwrite(bdev, p, block_idx, blen);
	if (r != EOK)
		return r;

	p += bdev->ph_bsize * blen;
	len -= bdev->ph_bsize * blen;

	block_idx += blen;

	/*Rest of the data*/
	if (len) {
		r = bdev->bread(bdev, bdev->ph_bbuf, block_idx, 1);
		if (r != EOK)
			return r;

		memcpy(bdev->ph_bbuf, p, len);

		r = bdev->bwrite(bdev, bdev->ph_bbuf, block_idx, 1);
		if (r != EOK)
			return r;
	}

	return r;
}

int ext4_block_readbytes(struct ext4_blockdev *bdev, uint64_t off, void *buf,
			 uint32_t len)
{
	uint64_t block_idx;
	uint64_t block_end;
	uint32_t blen;
	uint32_t unalg;
	int r = EOK;

	uint8_t *p = (void *)buf;

	ext4_assert(bdev && buf);

	if (!(bdev->flags & EXT4_BDEV_INITIALIZED))
		return EIO;

	block_idx = off / bdev->ph_bsize;
	block_end = block_idx + len / bdev->ph_bsize;

	if (!(block_end < bdev->ph_bcnt))
		return EINVAL; /*Ups. Out of range operation*/

	/*OK lets deal with the first possible unaligned block*/
	unalg = (off & (bdev->ph_bsize - 1));
	if (unalg) {

		uint32_t rlen = (bdev->ph_bsize - unalg) > len
				    ? len
				    : (bdev->ph_bsize - unalg);

		r = bdev->bread(bdev, bdev->ph_bbuf, block_idx, 1);
		if (r != EOK)
			return r;

		memcpy(p, bdev->ph_bbuf + unalg, rlen);

		p += rlen;
		len -= rlen;
		block_idx++;
	}

	/*Aligned data*/
	blen = len / bdev->ph_bsize;

	r = bdev->bread(bdev, p, block_idx, blen);
	if (r != EOK)
		return r;

	p += bdev->ph_bsize * blen;
	len -= bdev->ph_bsize * blen;

	block_idx += blen;

	/*Rest of the data*/
	if (len) {
		r = bdev->bread(bdev, bdev->ph_bbuf, block_idx, 1);
		if (r != EOK)
			return r;

		memcpy(p, bdev->ph_bbuf, len);
	}

	return r;
}

int ext4_block_cache_write_back(struct ext4_blockdev *bdev, uint8_t on_off)
{
	int r;
	struct ext4_buf *buf;

	if (on_off)
		bdev->cache_write_back++;

	if (!on_off && bdev->cache_write_back)
		bdev->cache_write_back--;

	if (bdev->cache_write_back)
		return EOK;

	/*Flush all delayed cache blocks*/
	while (!SLIST_EMPTY(&bdev->bc->dirty_list)) {
		
		buf = SLIST_FIRST(&bdev->bc->dirty_list);
		ext4_assert(buf);
		r = ext4_block_flush_buf(bdev, buf);
		if (r != EOK)
			return r;

	}
	return EOK;
}

/**
 * @}
 */
