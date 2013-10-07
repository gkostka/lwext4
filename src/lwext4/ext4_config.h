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
 * @file  ext4_config.h
 * @brief Configuration file.
 */

#ifndef EXT4_CONFIG_H_
#define EXT4_CONFIG_H_

#ifdef CONFIG_HAVE_OWN_CFG
#include <config.h>
#endif


/**@brief	Enable directory indexing feature (EXT3 feature)*/
#ifndef CONFIG_DIR_INDEX_ENABLE
#define CONFIG_DIR_INDEX_ENABLE				0
#endif

/**@brief	Enable extents feature (EXT4 feature)*/
#ifndef CONFIG_EXTENT_ENABLE
#define CONFIG_EXTENT_ENABLE				0
#endif



/**@brief	Include error codes from ext4_errno or sandard library.*/
#ifndef CONFIG_HAVE_OWN_ERRNO
#define CONFIG_HAVE_OWN_ERRNO				1
#endif


/**@brief	Debug printf enable (stdout)*/
#ifndef CONFIG_DEBUG_PRINTF
#define CONFIG_DEBUG_PRINTF					1
#endif

/**@brief	Assert printf enable (stdout)*/
#ifndef CONFIG_DEBUG_ASSERT
#define CONFIG_DEBUG_ASSERT					1
#endif

/**@brief	Statistics of block device*/
#ifndef CONFIG_BLOCK_DEV_ENABLE_STATS
#define CONFIG_BLOCK_DEV_ENABLE_STATS		1
#endif

/**@brief	Cache size of block device.*/
#ifndef CONFIG_BLOCK_DEV_CACHE_SIZE
#define CONFIG_BLOCK_DEV_CACHE_SIZE			8
#endif


/**@brief	Ilosc urzadzen blokowych.*/
#ifndef CONFIG_EXT4_BLOCKDEVS_COUNT
#define CONFIG_EXT4_BLOCKDEVS_COUNT			2
#endif

/**@brief	Ilosc punktow montowania systemu plikow*/
#ifndef CONFIG_EXT4_MOUNTPOINTS_COUNT
#define CONFIG_EXT4_MOUNTPOINTS_COUNT		2
#endif


#endif /* EXT4_CONFIG_H_ */

/**
 * @}
 */
