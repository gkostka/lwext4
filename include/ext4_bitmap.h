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
 * @file  ext4_bitmap.h
 * @brief Block/inode bitmap allocator.
 */

#ifndef EXT4_BITMAP_H_
#define EXT4_BITMAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief   Set bitmap bit.
 * @param   bmap bitmap
 * @param   bit bit to set*/
static inline void ext4_bmap_bit_set(uint8_t *bmap, uint32_t bit)
{
	*(bmap + (bit >> 3)) |= (1 << (bit & 7));
}

/**@brief   Clear bitmap bit.
 * @param   bmap bitmap buffer
 * @param   bit bit to clear*/
static inline void ext4_bmap_bit_clr(uint8_t *bmap, uint32_t bit)
{
	*(bmap + (bit >> 3)) &= ~(1 << (bit & 7));
}

/**@brief   Check if the bitmap bit is set.
 * @param   bmap bitmap buffer
 * @param   bit bit to check*/
static inline bool ext4_bmap_is_bit_set(uint8_t *bmap, uint32_t bit)
{
	return (*(bmap + (bit >> 3)) & (1 << (bit & 7)));
}

/**@brief   Check if the bitmap bit is clear.
 * @param   bmap bitmap buffer
 * @param   bit bit to check*/
static inline bool ext4_bmap_is_bit_clr(uint8_t *bmap, uint32_t bit)
{
	return !ext4_bmap_is_bit_set(bmap, bit);
}

/**@brief   Free range of bits in bitmap.
 * @param   bmap bitmap buffer
 * @param   sbit start bit
 * @param   bcnt bit count*/
void ext4_bmap_bits_free(uint8_t *bmap, uint32_t sbit, uint32_t bcnt);

/**@brief   Find first clear bit in bitmap.
 * @param   sbit start bit of search
 * @param   ebit end bit of search
 * @param   bit_id output parameter (first free bit)
 * @return  standard error code*/
int ext4_bmap_bit_find_clr(uint8_t *bmap, uint32_t sbit, uint32_t ebit,
			   uint32_t *bit_id);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_BITMAP_H_ */

/**
 * @}
 */
