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
 * @file  ext4_bcache.h
 * @brief Block cache allocator.
 */

#ifndef EXT4_BCACHE_H_
#define EXT4_BCACHE_H_

#include <ext4_config.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief   Single block descriptor.*/
struct ext4_block {
    /**@brief   Dirty flag.*/
    bool dirty;

    /**@brief   Logical block ID*/
    uint64_t lb_id;

    /**@brief   Cache id*/
    uint32_t cache_id;

    /**@brief   Data buffer.*/
    uint8_t *data;
};

/**@brief   Block cache descriptor.*/
struct ext4_bcache {

    /**@brief   Item count in block cache*/
    uint32_t cnt;

    /**@brief   Item size in block cache*/
    uint32_t itemsize;

    /**@brief   Last recently used counter.*/
    uint32_t lru_ctr;

    /**@brief   Reference count table (cnt).*/
    uint32_t refctr[CONFIG_BLOCK_DEV_CACHE_SIZE];

    /**@brief   Last recently used ID table (cnt)*/
    uint32_t lru_id[CONFIG_BLOCK_DEV_CACHE_SIZE];

    /**@brief   Writeback free delay mode table (cnt)*/
    uint8_t free_delay[CONFIG_BLOCK_DEV_CACHE_SIZE];

    /**@brief   Logical block table (cnt).*/
    uint64_t lba[CONFIG_BLOCK_DEV_CACHE_SIZE];

    /**@brief   Dirty mark (cnt).*/
    bool dirty[CONFIG_BLOCK_DEV_CACHE_SIZE];

    /**@brief   Cache data buffers (cnt * itemsize)*/
    uint8_t *data;

    /**@brief   Currently referenced datablocks*/
    uint32_t ref_blocks;

    /**@brief   Maximum referenced datablocks*/
    uint32_t max_ref_blocks;
};

/**@brief   Static initializer of block cache structure.*/
#define EXT4_BCACHE_STATIC_INSTANCE(__name, __cnt, __itemsize)                 \
    static uint8_t __name##_data[(__cnt) * (__itemsize)];                      \
    static struct ext4_bcache __name = {                                       \
        .cnt = __cnt,                                                          \
        .itemsize = __itemsize,                                                \
        .lru_ctr = 0,                                                          \
        .data = __name##_data,                                                 \
    }

/**@brief   Dynamic initialization of block cache.
 * @param   bc block cache descriptor
 * @param   cnt items count in block cache
 * @param   itemsize single item size (in bytes)
 * @return  standard error code*/
int ext4_bcache_init_dynamic(struct ext4_bcache *bc, uint32_t cnt,
                             uint32_t itemsize);

/**@brief   Dynamic de-initialization of block cache.
 * @param   bc block cache descriptor
 * @return  standard error code*/
int ext4_bcache_fini_dynamic(struct ext4_bcache *bc);

/**@brief   Allocate block from block cache memory.
 *          Unreferenced block allocation is based on LRU
 *          (Last Recently Used) algorithm.
 * @param   bc block cache descriptor
 * @param   b block to alloc
 * @param   is_new block is new (needs to be read)
 * @return  standard error code*/
int ext4_bcache_alloc(struct ext4_bcache *bc, struct ext4_block *b,
                      bool *is_new);

/**@brief   Free block from cache memory (decrement reference counter).
 * @param   bc block cache descriptor
 * @param   b block to free
 * @param   cache writeback mode
 * @return  standard error code*/
int ext4_bcache_free(struct ext4_bcache *bc, struct ext4_block *b,
                     uint8_t free_delay);

/**@brief   Return a full status of block cache.
 * @param   bc block cache descriptor
 * @return  full status*/
bool ext4_bcache_is_full(struct ext4_bcache *bc);

#endif /* EXT4_BCACHE_H_ */

/**
 * @}
 */
