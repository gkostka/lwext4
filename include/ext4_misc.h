/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * Copyright (c) 2015 Kaho Ng (ngkaho1234@gmail.com)
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
 * @file  ext4_misc.h
 * @brief Miscellaneous helpers.
 */

#ifndef EXT4_MISC_H_
#define EXT4_MISC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**************************************************************/

#define EXT4_DIV_ROUND_UP(x, y) (((x) + (y) - 1)/(y))
#define EXT4_ALIGN(x, y) ((y) * EXT4_DIV_ROUND_UP((x), (y)))

/****************************Endian conversion*****************/

static inline uint64_t reorder64(uint64_t n)
{
	return  ((n & 0xff) << 56) |
		((n & 0xff00) << 40) |
		((n & 0xff0000) << 24) |
		((n & 0xff000000LL) << 8) |
		((n & 0xff00000000LL) >> 8) |
		((n & 0xff0000000000LL) >> 24) |
		((n & 0xff000000000000LL) >> 40) |
		((n & 0xff00000000000000LL) >> 56);
}

static inline uint32_t reorder32(uint32_t n)
{
	return  ((n & 0xff) << 24) |
		((n & 0xff00) << 8) |
		((n & 0xff0000) >> 8) |
		((n & 0xff000000) >> 24);
}

static inline uint16_t reorder16(uint16_t n)
{
	return  ((n & 0xff) << 8) |
		((n & 0xff00) >> 8);
}

#ifdef CONFIG_BIG_ENDIAN
#define to_le64(_n) reorder64(_n)
#define to_le32(_n) reorder32(_n)
#define to_le16(_n) reorder16(_n)

#define to_be64(_n) _n
#define to_be32(_n) _n
#define to_be16(_n) _n

#else
#define to_le64(_n) _n
#define to_le32(_n) _n
#define to_le16(_n) _n

#define to_be64(_n) reorder64(_n)
#define to_be32(_n) reorder32(_n)
#define to_be16(_n) reorder16(_n)
#endif

/****************************Access macros to ext4 structures*****************/

#define ext4_get32(s, f) to_le32((s)->f)
#define ext4_get16(s, f) to_le16((s)->f)
#define ext4_get8(s, f) (s)->f

#define ext4_set32(s, f, v)                                                    \
	do {                                                                   \
		(s)->f = to_le32(v);                                           \
	} while (0)
#define ext4_set16(s, f, v)                                                    \
	do {                                                                   \
		(s)->f = to_le16(v);                                           \
	} while (0)
#define ext4_set8                                                              \
	(s, f, v) do { (s)->f = (v); }                                         \
	while (0)

/****************************Access macros to jbd2 structures*****************/

#define jbd_get32(s, f) to_be32((s)->f)
#define jbd_get16(s, f) to_be16((s)->f)
#define jbd_get8(s, f) (s)->f

#define jbd_set32(s, f, v)                                                    \
	do {                                                                   \
		(s)->f = to_be32(v);                                           \
	} while (0)
#define jbd_set16(s, f, v)                                                    \
	do {                                                                   \
		(s)->f = to_be16(v);                                           \
	} while (0)
#define jbd_set8                                                              \
	(s, f, v) do { (s)->f = (v); }                                         \
	while (0)

#ifdef __GNUC__
 #ifndef __unused
 #define __unused __attribute__ ((__unused__))
 #endif
#else
 #define __unused
#endif

#ifndef offsetof
#define offsetof(type, field) 		\
	((size_t)(&(((type *)0)->field)))
#endif

#ifdef __cplusplus
}
#endif

#endif /* EXT4_MISC_H_ */

/**
 * @}
 */
