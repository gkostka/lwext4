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

int ext4_block_get_noread(struct ext4_blockdev *bdev, struct ext4_block *b,
			  uint64_t lba)
{
	uint32_t i;
	bool is_new;
	int r;

	ext4_assert(bdev && b);

	if (!(bdev->flags & EXT4_BDEV_INITIALIZED))
		return EIO;

	if (!(lba < bdev->lg_bcnt))
		return ERANGE;

	b->dirty = 0;
	b->lb_id = lba;

	/*If cache is full we have to flush it anyway :(*/
	if (ext4_bcache_is_full(bdev->bc) && bdev->cache_write_back) {

		uint32_t free_candidate = bdev->bc->cnt;
		uint32_t min_lru = 0xFFFFFFFF;

		for (i = 0; i < bdev->bc->cnt; ++i) {
			/*Check if buffer free was delayed.*/
			if (!bdev->bc->free_delay[i])
				continue;

			/*Check reference counter.*/
			if (bdev->bc->refctr[i])
				continue;

			if (bdev->bc->lru_id[i] < min_lru) {
				min_lru = bdev->bc->lru_id[i];
				free_candidate = i;
				continue;
			}
		}

		if (free_candidate < bdev->bc->cnt) {
			/*Buffer free was delayed and have no reference. Flush
			 * it.*/
			r = ext4_blocks_set_direct(
			    bdev, bdev->bc->data +
				      bdev->bc->itemsize * free_candidate,
			    bdev->bc->lba[free_candidate], 1);
			if (r != EOK)
				return r;

			/*No delayed anymore*/
			bdev->bc->free_delay[free_candidate] = 0;

			/*Reduce reference counter*/
			bdev->bc->ref_blocks--;
		}
	}

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
		ext4_bcache_free(bdev->bc, b, 0);
		b->lb_id = 0;
		return r;
	}

	/* Mark buffer up-to-date, since
	 * fresh data is read from physical device just now. */
	ext4_bcache_set_flag(bdev->bc, b->cache_id, BC_UPTODATE);
	b->uptodate = true;
	bdev->bread_ctr++;
	return EOK;
}

int ext4_block_set(struct ext4_blockdev *bdev, struct ext4_block *b)
{
	uint64_t pba;
	uint32_t pb_cnt;
	int r;

	ext4_assert(bdev && b);

	if (!(bdev->flags & EXT4_BDEV_INITIALIZED))
		return EIO;

	/*Buffer is not marked dirty and is stale*/
	if (!b->uptodate && !b->dirty)
		ext4_bcache_clear_flag(bdev->bc, b->cache_id, BC_UPTODATE);

	/*No need to write.*/
	if (!b->dirty &&
	    !ext4_bcache_test_flag(bdev->bc, b->cache_id, BC_DIRTY)) {
		ext4_bcache_free(bdev->bc, b, 0);
		return EOK;
	}
	/* Data is valid, so mark buffer up-to-date. */
	ext4_bcache_set_flag(bdev->bc, b->cache_id, BC_UPTODATE);

	/*Free cache delay mode*/
	if (bdev->cache_write_back) {

		/*Free cache block and mark as free delayed*/
		return ext4_bcache_free(bdev->bc, b, bdev->cache_write_back);
	}

	if (bdev->bc->refctr[b->cache_id] > 1) {
		ext4_bcache_set_flag(bdev->bc, b->cache_id, BC_DIRTY);
		return ext4_bcache_free(bdev->bc, b, 0);
	}

	pba = (b->lb_id * bdev->lg_bsize) / bdev->ph_bsize;
	pb_cnt = bdev->lg_bsize / bdev->ph_bsize;

	r = bdev->bwrite(bdev, b->data, pba, pb_cnt);
	ext4_bcache_clear_flag(bdev->bc, b->cache_id, BC_DIRTY);
	if (r != EOK) {
		b->dirty = false;
		ext4_bcache_clear_flag(bdev->bc, b->cache_id, BC_UPTODATE);
		ext4_bcache_free(bdev->bc, b, 0);
		return r;
	}

	bdev->bwrite_ctr++;
	b->dirty = false;
	ext4_bcache_free(bdev->bc, b, 0);
	return EOK;
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
	uint32_t i;

	if (on_off)
		bdev->cache_write_back++;

	if (!on_off && bdev->cache_write_back)
		bdev->cache_write_back--;


	if (bdev->cache_write_back)
		return EOK;

	/*Flush all delayed cache blocks*/
	for (i = 0; i < bdev->bc->cnt; ++i) {

		/*Check if buffer free was delayed.*/
		if (!bdev->bc->free_delay[i])
			continue;

		/*Check reference counter.*/
		if (bdev->bc->refctr[i])
			continue;

		/*Buffer free was delayed and have no reference. Flush
		 * it.*/
		r = ext4_blocks_set_direct(bdev, bdev->bc->data +
				bdev->bc->itemsize * i,	bdev->bc->lba[i], 1);
		if (r != EOK)
			return r;

		/*No delayed anymore*/
		bdev->bc->free_delay[i] = 0;

		/*Reduce reference counter*/
		bdev->bc->ref_blocks--;
	}

	return EOK;
}

/**
 * @}
 */
