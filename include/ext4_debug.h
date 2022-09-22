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
 * @file  ext4_debug.c
 * @brief Debug printf and assert macros.
 */

#ifndef EXT4_DEBUG_H_
#define EXT4_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_errno.h>

#if !CONFIG_HAVE_OWN_ASSERT
#include <assert.h>
#endif

#include <stdint.h>
#include <inttypes.h>

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif


#define DEBUG_BALLOC (1ul << 0)
#define DEBUG_BCACHE (1ul << 1)
#define DEBUG_BITMAP (1ul << 2)
#define DEBUG_BLOCK_GROUP (1ul << 3)
#define DEBUG_BLOCKDEV (1ul << 4)
#define DEBUG_DIR_IDX (1ul << 5)
#define DEBUG_DIR (1ul << 6)
#define DEBUG_EXTENT (1ul << 7)
#define DEBUG_FS (1ul << 8)
#define DEBUG_HASH (1ul << 9)
#define DEBUG_IALLOC (1ul << 10)
#define DEBUG_INODE (1ul << 11)
#define DEBUG_SUPER (1ul << 12)
#define DEBUG_XATTR (1ul << 13)
#define DEBUG_MKFS (1ul << 14)
#define DEBUG_EXT4 (1ul << 15)
#define DEBUG_JBD (1ul << 16)
#define DEBUG_MBR (1ul << 17)

#define DEBUG_NOPREFIX (1ul << 31)
#define DEBUG_ALL (0xFFFFFFFF)

static inline const char *ext4_dmask_id2str(uint32_t m)
{
	switch(m) {
	case DEBUG_BALLOC:
		return "ext4_balloc: ";
	case DEBUG_BCACHE:
		return "ext4_bcache: ";
	case DEBUG_BITMAP:
		return "ext4_bitmap: ";
	case DEBUG_BLOCK_GROUP:
		return "ext4_block_group: ";
	case DEBUG_BLOCKDEV:
		return "ext4_blockdev: ";
	case DEBUG_DIR_IDX:
		return "ext4_dir_idx: ";
	case DEBUG_DIR:
		return "ext4_dir: ";
	case DEBUG_EXTENT:
		return "ext4_extent: ";
	case DEBUG_FS:
		return "ext4_fs: ";
	case DEBUG_HASH:
		return "ext4_hash: ";
	case DEBUG_IALLOC:
		return "ext4_ialloc: ";
	case DEBUG_INODE:
		return "ext4_inode: ";
	case DEBUG_SUPER:
		return "ext4_super: ";
	case DEBUG_XATTR:
		return "ext4_xattr: ";
	case DEBUG_MKFS:
		return "ext4_mkfs: ";
	case DEBUG_JBD:
		return "ext4_jbd: ";
	case DEBUG_MBR:
		return "ext4_mbr: ";
	case DEBUG_EXT4:
		return "ext4: ";
	}
	return "";
}
#define DBG_NONE  ""
#define DBG_INFO  "[info]  "
#define DBG_WARN  "[warn]  "
#define DBG_ERROR "[error] "

/**@brief   Global mask debug set.
 * @param   m new debug mask.*/
void ext4_dmask_set(uint32_t m);

/**@brief   Global mask debug clear.
 * @param   m new debug mask.*/
void ext4_dmask_clr(uint32_t m);

/**@brief   Global debug mask get.
 * @return  debug mask*/
uint32_t ext4_dmask_get(void);

#if CONFIG_DEBUG_PRINTF
#include <stdio.h>

/**@brief   Debug printf.*/
#define ext4_dbg(m, ...)                                                       \
	do {                                                                   \
		if ((m) & ext4_dmask_get()) {                                  \
			if (!((m) & DEBUG_NOPREFIX)) {                         \
				printf("%s", ext4_dmask_id2str(m));            \
				printf("l: %d   ", __LINE__);                  \
			}                                                      \
			printf(__VA_ARGS__);                                   \
			fflush(stdout);                                        \
		}                                                              \
	} while (0)
#else
#define ext4_dbg(m, ...) do { } while (0)
#endif

#if CONFIG_DEBUG_ASSERT
/**@brief   Debug assertion.*/
#if CONFIG_HAVE_OWN_ASSERT
#include <stdio.h>

#define ext4_assert(_v)                                                        \
	do {                                                                   \
		if (!(_v)) {                                                   \
			printf("assertion failed:\nfile: %s\nline: %d\n",      \
			       __FILE__, __LINE__);                            \
			       while (1)				       \
				       ;				       \
		}                                                              \
	} while (0)
#else
#define ext4_assert(_v) assert(_v)
#endif
#else
#define ext4_assert(_v) ((void)(_v))
#endif

#ifdef __cplusplus
}
#endif

#endif /* EXT4_DEBUG_H_ */

/**
 * @}
 */
