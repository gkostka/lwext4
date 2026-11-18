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

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/**@brief   Default filename.*/
static const char *fname = "ext2";

/**@brief   Byte offset of the partition within the file.*/
static uint64_t part_offset;

/**@brief   Byte size of the partition. Zero means up to the end of file.*/
static uint64_t part_size;

/**@brief   Image block size.*/
#define EXT4_FILEDEV_BSIZE 512

/**@brief   Image file descriptor.*/
static FILE *dev_file;

#define DROP_LINUXCACHE_BUFFERS 0

/**********************BLOCKDEV INTERFACE**************************************/
static int file_dev_open(struct ext4_blockdev *bdev);
static int file_dev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			 uint32_t blk_cnt);
static int file_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt);
static int file_dev_close(struct ext4_blockdev *bdev);

/******************************************************************************/
EXT4_BLOCKDEV_STATIC_INSTANCE(file_dev, EXT4_FILEDEV_BSIZE, 0, file_dev_open,
		file_dev_bread, file_dev_bwrite, file_dev_close, 0, 0);

/******************************************************************************/
static int file_dev_open(struct ext4_blockdev *bdev)
{
	off_t file_size;
	int r = EFAULT;

	if (part_offset % EXT4_FILEDEV_BSIZE)
		return EINVAL;

	dev_file = fopen(fname, "r+b");

	if (!dev_file)
		return EIO;

	/*No buffering at file.*/
	setbuf(dev_file, 0);

	if (fseeko(dev_file, 0, SEEK_END))
		goto fail;

	file_size = ftello(dev_file);
	if (file_size < 0)
		goto fail;

	/*The partition window has to fit in the file.*/
	r = EINVAL;
	if (part_offset > (uint64_t)file_size)
		goto fail;
	if (part_size > (uint64_t)file_size - part_offset)
		goto fail;

	file_dev.part_offset = part_offset;
	file_dev.part_size = part_size ? part_size : file_size - part_offset;
	file_dev.bdif->ph_bcnt = file_size / file_dev.bdif->ph_bsize;

	return EOK;

fail:
	fclose(dev_file);
	dev_file = NULL;
	return r;
}

/******************************************************************************/
/**@brief   Check that physical blocks [blk_id, blk_id + blk_cnt) lie inside
 *          the partition window of the block device.*/
static bool file_dev_in_part(struct ext4_blockdev *bdev, uint64_t blk_id,
			     uint32_t blk_cnt)
{
	uint64_t bsize = bdev->bdif->ph_bsize;
	uint64_t first = bdev->part_offset / bsize;
	uint64_t end = (bdev->part_offset + bdev->part_size) / bsize;

	return blk_id >= first && blk_id <= end && blk_cnt <= end - blk_id;
}

/******************************************************************************/

static int file_dev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			 uint32_t blk_cnt)
{
	if (!file_dev_in_part(bdev, blk_id, blk_cnt))
		return EINVAL;
	if (fseeko(dev_file, blk_id * bdev->bdif->ph_bsize, SEEK_SET))
		return EIO;
	if (!blk_cnt)
		return EOK;
	if (!fread(buf, bdev->bdif->ph_bsize * blk_cnt, 1, dev_file))
		return EIO;

	return EOK;
}

static void drop_cache(void)
{
#if defined(__linux__) && DROP_LINUXCACHE_BUFFERS
	int fd;
	char *data = "3";

	sync();
	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
	write(fd, data, sizeof(char));
	close(fd);
#endif
}

/******************************************************************************/
static int file_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt)
{
	if (!file_dev_in_part(bdev, blk_id, blk_cnt))
		return EINVAL;
	if (fseeko(dev_file, blk_id * bdev->bdif->ph_bsize, SEEK_SET))
		return EIO;
	if (!blk_cnt)
		return EOK;
	if (!fwrite(buf, bdev->bdif->ph_bsize * blk_cnt, 1, dev_file))
		return EIO;

	drop_cache();
	return EOK;
}
/******************************************************************************/
static int file_dev_close(struct ext4_blockdev *bdev)
{
	fclose(dev_file);
	return EOK;
}

/******************************************************************************/
struct ext4_blockdev *file_dev_get(void)
{
	return &file_dev;
}
/******************************************************************************/
void file_dev_name_set(const char *n)
{
	fname = n;
}
/******************************************************************************/
void file_dev_part_offset_set(uint64_t offset)
{
	part_offset = offset;
}
/******************************************************************************/
void file_dev_part_size_set(uint64_t size)
{
	part_size = size;
}
/******************************************************************************/
