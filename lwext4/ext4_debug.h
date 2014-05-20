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

#include <ext4_config.h>
#include <ext4_errno.h>
#include <stdint.h>
#include <stdio.h>

/**@brief   Debug mask: ext4_blockdev.c*/
#define EXT4_DEBUG_BLOCKDEV         (1 << 0)

/**@brief   Debug mask: ext4_fs.c*/
#define EXT4_DEBUG_FS               (1 << 1)

/**@brief   Debug mask: ext4_balloc.c*/
#define EXT4_DEBUG_BALLOC           (1 << 2)

/**@brief   Debug mask: ext4_bitmap.c*/
#define EXT4_DEBUG_BITMAP           (1 << 3)

/**@brief   Debug mask: ext4_dir_idx.c*/
#define EXT4_DEBUG_DIR_IDX          (1 << 4)

/**@brief   Debug mask: ext4_dir.c*/
#define EXT4_DEBUG_DIR              (1 << 5)

/**@brief   Debug mask: ext4_ialloc.c*/
#define EXT4_DEBUG_IALLOC           (1 << 6)

/**@brief   Debug mask: ext4_inode.c*/
#define EXT4_DEBUG_INODE            (1 << 7)

/**@brief   Debug mask: ext4_super.c*/
#define EXT4_DEBUG_SUPER            (1 << 8)

/**@brief   Debug mask: ext4_bcache.c*/
#define EXT4_DEBUG_BCACHE           (1 << 9)

/**@brief   Debug mask: ext4_extents.c*/
#define EXT4_DEBUG_EXTENTS          (1 << 10)



/**@brief   All debug printf enabled.*/
#define EXT4_DEBUG_ALL              (0xFFFFFFFF)

/**@brief   Global mask debug settings.
 * @brief   m new debug mask.*/
void ext4_dmask_set(uint32_t m);

/**@brief   Global debug mask get.
 * @return  debug mask*/
uint32_t ext4_dmask_get(void);


#if CONFIG_DEBUG_PRINTF
/**@brief   Debug printf.*/
#define ext4_dprintf(m, ...)    do {                            \
        if(m & ext4_dmask_get())  printf(__VA_ARGS__); 			\
        fflush(stdout);                                         \
}while(0)
#else
#define ext4_dprintf(m, ...)
#endif



#if CONFIG_DEBUG_ASSERT
/**@brief   Debug asseration.*/
#define ext4_assert(_v) do {                                                \
        if(!(_v)){                                                          \
            printf("Assertion failed:\nmodule: %s\nfunc: %s\nline: %d\n",   \
                    __FILE__, __FUNCTION__, __LINE__);                      \
        }                                                                   \
}while(0)
#else
#define ext4_assert(_v)
#endif

#endif /* EXT4_DEBUG_H_ */

/**
 * @}
 */
