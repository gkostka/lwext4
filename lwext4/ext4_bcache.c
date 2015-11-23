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

static int
ext4_bcache_lba_compare(struct ext4_buf *a,
			struct ext4_buf *b)
{
	return a->lba - b->lba;
}

static int
ext4_bcache_lru_compare(struct ext4_buf *a,
			struct ext4_buf *b)
{
	return a->lru_id - b->lru_id;
}

RB_GENERATE_INTERNAL(ext4_buf_lba, ext4_buf, lba_node,
		     ext4_bcache_lba_compare, static inline)
RB_GENERATE_INTERNAL(ext4_buf_lru, ext4_buf, lru_node,
		     ext4_bcache_lru_compare, static inline)

int ext4_bcache_init_dynamic(struct ext4_bcache *bc, uint32_t cnt,
			     uint32_t itemsize)
{
	ext4_assert(bc && cnt && itemsize);

	memset(bc, 0, sizeof(struct ext4_bcache));

	bc->cnt = cnt;
	bc->itemsize = itemsize;
	bc->ref_blocks = 0;
	bc->max_ref_blocks = 0;

	return EOK;
}

int ext4_bcache_fini_dynamic(struct ext4_bcache *bc)
{
	memset(bc, 0, sizeof(struct ext4_bcache));
	return EOK;
}

/**@brief:
 *
 *  This is ext4_bcache, the module handling basic buffer-cache stuff.
 *
 *  Buffers in a bcache are sorted by their LBA and stored in a
 *  RB-Tree(lba_root).
 *
 *  Bcache also maintains another RB-Tree(lru_root) right now, where
 *  buffers are sorted by their LRU id.
 *
 *  A singly-linked list is used to track those dirty buffers which are
 *  ready to be flushed. (Those buffers which are dirty but also referenced
 *  are not considered ready to be flushed.)
 *
 *  When a buffer is not referenced, it will be stored in both lba_root
 *  and lru_root, while it will only be stored in lba_root when it is
 *  referenced.
 */

static struct ext4_buf *
ext4_buf_alloc(struct ext4_bcache *bc, uint64_t lba)
{
	void *data;
	struct ext4_buf *buf;
	data = malloc(bc->itemsize);
	if (!data)
		return NULL;

	buf = malloc(sizeof(struct ext4_buf));
	if (!buf) {
		free(data);
		return NULL;
	}

	buf->flags = 0;
	buf->lba = lba;
	buf->data = data;
	buf->lru_id = 0;
	buf->refctr = 0;
	memset(&buf->lba_node, 0, sizeof(buf->lba_node));
	memset(&buf->lru_node, 0, sizeof(buf->lru_node));
	memset(&buf->dirty_node, 0, sizeof(buf->dirty_node));
	return buf;
}

static void ext4_buf_free(struct ext4_buf *buf)
{
	free(buf->data);
	free(buf);
}

static struct ext4_buf *
ext4_buf_lookup(struct ext4_bcache *bc, uint64_t lba)
{
	struct ext4_buf tmp = {
		.lba = lba
	};

	return RB_FIND(ext4_buf_lba, &bc->lba_root, &tmp);
}

struct ext4_buf *ext4_buf_lowest_lru(struct ext4_bcache *bc)
{
	return RB_MIN(ext4_buf_lru, &bc->lru_root);
}

void ext4_bcache_drop_buf(struct ext4_bcache *bc, struct ext4_buf *buf)
{
	/*Cannot drop any referenced buffers.*/
	ext4_assert(!buf->refctr);

	RB_REMOVE(ext4_buf_lba, &bc->lba_root, buf);
	RB_REMOVE(ext4_buf_lru, &bc->lru_root, buf);

	/*Forcibly drop dirty buffer.*/
	if (ext4_bcache_test_flag(buf, BC_DIRTY))
		SLIST_REMOVE(&bc->dirty_list,
			     buf,
			     ext4_buf,
			     dirty_node);

	ext4_buf_free(buf);
	bc->ref_blocks--;
}

int ext4_bcache_alloc(struct ext4_bcache *bc, struct ext4_block *b,
		      bool *is_new)
{
	/* Try to search the buffer with exaxt LBA. */
	struct ext4_buf *buf = ext4_buf_lookup(bc, b->lb_id);
	if (buf) {
		/* If buffer is not referenced. */
		if (!buf->refctr) {
			/* Assign new value to LRU id and increment LRU counter
			 * by 1*/
			buf->lru_id = ++bc->lru_ctr;
			RB_REMOVE(ext4_buf_lru, &bc->lru_root, buf);
			if (ext4_bcache_test_flag(buf, BC_DIRTY))
				SLIST_REMOVE(&bc->dirty_list,
					     buf,
					     ext4_buf,
					     dirty_node);

		}

		buf->refctr++;

		b->uptodate = ext4_bcache_test_flag(buf, BC_UPTODATE);
		/* Right now we don't propagate the dirty flag from ext4_buf to
		 * ext4_block. */
		b->dirty = false;
		b->buf = buf;
		b->data = buf->data;

		*is_new = false;
		return EOK;
	}

	/* We need to allocate one buffer.*/
	buf = ext4_buf_alloc(bc, b->lb_id);
	if (!buf)
		return ENOMEM;

	RB_INSERT(ext4_buf_lba, &bc->lba_root, buf);
	/* One more buffer in bcache now. :-) */
	bc->ref_blocks++;

	buf->refctr = 1;
	/* Assign new value to LRU id and increment LRU counter
	 * by 1*/
	buf->lru_id = ++bc->lru_ctr;

	b->uptodate = false;
	b->dirty = false;
	b->buf = buf;
	b->data = buf->data;

	*is_new = true;
	return EOK;
}

int ext4_bcache_free(struct ext4_bcache *bc, struct ext4_block *b,
		     uint8_t free_delay)
{
	struct ext4_buf *buf = b->buf;

	ext4_assert(bc && b);

	/*Check if valid.*/
	ext4_assert(b->lb_id);

	/*Block should have a valid pointer to ext4_buf.*/
	ext4_assert(buf);

	/*Check if someone don't try free unreferenced block cache.*/
	ext4_assert(buf->refctr);

	/*Just decrease reference counter*/
	buf->refctr--;

	/* TODO: it looks useless... */
	if (free_delay)
		bc->free_delay = free_delay;

	/* If buffer is modified, buf will be mark up-to-date and dirty. */
	if (b->dirty) {
		ext4_bcache_set_flag(buf, BC_DIRTY);
		ext4_bcache_set_flag(buf, BC_UPTODATE);
		b->uptodate = true;
	}
	/* Someone might want to drop this buffer from bcache. */
	if (!b->uptodate)
		ext4_bcache_clear_flag(buf, BC_UPTODATE);

	/* We are the last one touching this buffer, do the cleanups. */
	if (!buf->refctr) {
		RB_INSERT(ext4_buf_lru, &bc->lru_root, buf);
		/* This buffer is ready to be flushed. */
		if (ext4_bcache_test_flag(buf, BC_DIRTY))
			SLIST_INSERT_HEAD(&bc->dirty_list, buf, dirty_node);

		/* The buffer is invalidated...drop it. */
		if (!ext4_bcache_test_flag(buf, BC_UPTODATE))
			ext4_bcache_drop_buf(bc, buf);

	}

	b->lb_id = 0;
	b->data = 0;
	b->uptodate = false;
	b->dirty = false;

	return EOK;
}

bool ext4_bcache_is_full(struct ext4_bcache *bc)
{
	return (bc->cnt <= bc->ref_blocks);
}

/**
 * @}
 */
