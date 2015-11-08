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
 * @file  ext4_bcache.c
 * @brief Block cache allocator.
 */

#include "ext4_config.h"
#include "ext4_bcache.h"
#include "ext4_debug.h"
#include "ext4_errno.h"

#include <string.h>
#include <stdlib.h>

int ext4_bcache_init_dynamic(struct ext4_bcache *bc, uint32_t cnt,
			     uint32_t itemsize)
{
	ext4_assert(bc && cnt && itemsize);

	memset(bc, 0, sizeof(struct ext4_bcache));

	bc->data = malloc(cnt * itemsize);
	if (!bc->data)
		goto error;

	bc->cnt = cnt;
	bc->itemsize = itemsize;
	bc->ref_blocks = 0;
	bc->max_ref_blocks = 0;

	return EOK;

error:

	if (bc->data)
		free(bc->data);

	memset(bc, 0, sizeof(struct ext4_bcache));

	return ENOMEM;
}

int ext4_bcache_fini_dynamic(struct ext4_bcache *bc)
{
	if (bc->data)
		free(bc->data);

	memset(bc, 0, sizeof(struct ext4_bcache));

	return EOK;
}

int ext4_bcache_alloc(struct ext4_bcache *bc, struct ext4_block *b,
		      bool *is_new)
{
	uint32_t i;
	ext4_assert(bc && b && is_new);

	/*Check if valid.*/
	ext4_assert(b->lb_id);
	if (!b->lb_id) {
		ext4_assert(b->lb_id);
	}

	uint32_t cache_id = bc->cnt;
	uint32_t alloc_id = 0;

	*is_new = false;

	/*Find in free blocks (Last Recently Used).*/
	for (i = 0; i < bc->cnt; ++i) {

		/*Check if block is already in cache*/
		if (b->lb_id == bc->lba[i]) {

			if (!bc->refctr[i] && !bc->free_delay[i])
				bc->ref_blocks++;

			/*Update reference counter*/
			bc->refctr[i]++;

			/*Update usage marker*/
			bc->lru_id[i] = ++bc->lru_ctr;

			/*Set valid cache data and id*/
			b->data = bc->data + i * bc->itemsize;
			b->cache_id = i;

			/* If data in the caxhe is up-to-date */
			b->uptodate = ext4_bcache_test_flag(bc, i, BC_UPTODATE);

			return EOK;
		}

		/*Best fit calculations.*/
		if (bc->refctr[i])
			continue;

		if (bc->free_delay[i])
			continue;

		/*Block is unreferenced, but it may exist block with
		 * lower usage marker*/

		/*First find.*/
		if (cache_id == bc->cnt) {
			cache_id = i;
			alloc_id = bc->lru_id[i];
			continue;
		}

		/*Next find*/
		if (alloc_id <= bc->lru_id[i])
			continue;

		/*This block has lower alloc id marker*/
		cache_id = i;
		alloc_id = bc->lru_id[i];
	}

	if (cache_id != bc->cnt) {
		/*There was unreferenced block*/
		bc->lba[cache_id] = b->lb_id;
		bc->refctr[cache_id] = 1;
		bc->lru_id[cache_id] = ++bc->lru_ctr;

		/*Set valid cache data and id*/
		b->data = bc->data + cache_id * bc->itemsize;
		b->cache_id = cache_id;

		/* Data in the cache is not up-to-date anymore. */
		ext4_bcache_clear_flag(bc, cache_id, BC_UPTODATE);
		b->uptodate = false;

		/*Statistics*/
		bc->ref_blocks++;
		if (bc->ref_blocks > bc->max_ref_blocks)
			bc->max_ref_blocks = bc->ref_blocks;

		/*Block needs to be read.*/
		*is_new = true;

		return EOK;
	}

	ext4_dbg(DEBUG_BCACHE, DBG_ERROR
		     "unable to alloc block cache!\n");
	return ENOMEM;
}

int ext4_bcache_free(struct ext4_bcache *bc, struct ext4_block *b,
		     uint8_t free_delay)
{
	ext4_assert(bc && b);

	/*Check if valid.*/
	ext4_assert(b->lb_id);

	/*Block should be in cache.*/
	ext4_assert(b->cache_id < bc->cnt);

	/*Check if someone don't try free unreferenced block cache.*/
	ext4_assert(bc->refctr[b->cache_id]);

	/*Just decrease reference counter*/
	if (bc->refctr[b->cache_id])
		bc->refctr[b->cache_id]--;

	if (free_delay)
		bc->free_delay[b->cache_id] = free_delay;

	/*Update statistics*/
	if (!bc->refctr[b->cache_id] && !bc->free_delay[b->cache_id])
		bc->ref_blocks--;

	b->lb_id = 0;
	b->data = 0;
	b->cache_id = 0;
	b->uptodate = false;

	return EOK;
}

bool ext4_bcache_is_full(struct ext4_bcache *bc)
{
	return (bc->cnt == bc->ref_blocks);
}

/**
 * @}
 */
