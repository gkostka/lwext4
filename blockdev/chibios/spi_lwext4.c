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

#include "spi_lwext4.h"

#include <config.h>
#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#include <hal.h>
#include <mmc_spi.h>


#if HAL_USE_MMC_SPI
extern MMCDriver MMCD1;
#endif

/**@brief   Block size.*/
#define SPI_BLOCK_SIZE 512

/**@brief   MBR_block ID*/
#define MBR_BLOCK_ID 0
#define MBR_PART_TABLE_OFF 446

struct part_tab_entry {
	uint8_t status;
	uint8_t chs1[3];
	uint8_t type;
	uint8_t chs2[3];
	uint32_t first_lba;
	uint32_t sectors;
} __attribute__((packed));

/**@brief   Partition block offset*/
static uint32_t part_offset;

/**@brief IO timings*/
struct spi_io_timings {
	uint64_t acc_bread;
	uint64_t acc_bwrite;

	uint32_t cnt_bread;
	uint32_t cnt_bwrite;

	uint32_t av_bread;
	uint32_t av_bwrite;
};

static struct spi_io_timings io_timings;

void io_timings_clear(void)
{
	memset(&io_timings, 0, sizeof(struct spi_io_timings));
}

const struct ext4_io_stats *io_timings_get(uint32_t time_sum_ms)
{
	static struct ext4_io_stats s;

	s.io_read = (((float)io_timings.acc_bread * 100.0) / time_sum_ms);
	s.io_read /= 1000.0;

	s.io_write = (((float)io_timings.acc_bwrite * 100.0) / time_sum_ms);
	s.io_write /= 1000.0;

	s.cpu = 100.0 - s.io_read - s.io_write;

	return &s;
}

/**********************BLOCKDEV INTERFACE**************************************/
static int spi_open(struct ext4_blockdev *bdev);
static int spi_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		     uint32_t blk_cnt);
static int spi_bwrite(struct ext4_blockdev *bdev, const void *buf,
		      uint64_t blk_id, uint32_t blk_cnt);
static int spi_close(struct ext4_blockdev *bdev);

/******************************************************************************/
EXT4_BLOCKDEV_STATIC_INSTANCE(_spi, SPI_BLOCK_SIZE, 0, spi_open, spi_bread,
			      spi_bwrite, spi_close);

/******************************************************************************/
EXT4_BCACHE_STATIC_INSTANCE(_spi_cache, CONFIG_BLOCK_DEV_CACHE_SIZE,
			    EXT_LOGICAL_BLOCK_SIZE);

/******************************************************************************/

static int spi_open(struct ext4_blockdev *bdev)
{

	(void)bdev;

	static uint8_t mbr[512];
	struct part_tab_entry *part0;

	if (mmcConnect(&MMCD1) != HAL_SUCCESS)
		return EIO;

	if (mmcStartSequentialRead(&MMCD1, 0) != HAL_SUCCESS)
		return EIO;

	if (mmcSequentialRead(&MMCD1, mbr) != HAL_SUCCESS)
		return EIO;

	if (mmcStopSequentialRead(&MMCD1) != HAL_SUCCESS)
		return EIO;

	part0 = (struct part_tab_entry *)(mbr + MBR_PART_TABLE_OFF);

	part_offset = part0->first_lba;
	_spi.ph_bcnt = MMCD1.capacity * SPI_BLOCK_SIZE;

	return EOK;
}

static int spi_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		     uint32_t blk_cnt)
{
	(void)bdev;
	uint64_t v = tim_get_us();

	if (mmcStartSequentialRead(&MMCD1, blk_id) != HAL_SUCCESS)
		return EIO;

	while (blk_cnt) {
		if (mmcSequentialRead(&MMCD1, buf) != HAL_SUCCESS)
			return EIO;

		buf += SPI_BLOCK_SIZE;
		blk_cnt--;
	}

	if (mmcStopSequentialRead(&MMCD1) != HAL_SUCCESS)
		return EIO;

	io_timings.acc_bread += tim_get_us() - v;
	io_timings.cnt_bread++;
	io_timings.av_bread = io_timings.acc_bread / io_timings.cnt_bread;

	return EOK;
}

static int spi_bwrite(struct ext4_blockdev *bdev, const void *buf,
		      uint64_t blk_id, uint32_t blk_cnt)
{
	(void)bdev;
	uint64_t v = tim_get_us();

	if (mmcStartSequentialWrite(&MMCD1, blk_id) != HAL_SUCCESS)
		return EIO;

	while (blk_cnt) {
		if (mmcSequentialWrite(&MMCD1, buf) != HAL_SUCCESS)
			return EIO;

		buf += SPI_BLOCK_SIZE;
		blk_cnt--;
	}

	if (mmcStopSequentialWrite(&MMCD1) != HAL_SUCCESS)
		return EIO;

	io_timings.acc_bwrite += tim_get_us() - v;
	io_timings.cnt_bwrite++;
	io_timings.av_bwrite = io_timings.acc_bwrite / io_timings.cnt_bwrite;

	return EOK;
}

static int spi_close(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

/******************************************************************************/

struct ext4_bcache *spi_cache_get(void) { return &_spi_cache; }

struct ext4_blockdev *spi_bdev_get(void) { return &_spi; }
