/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Block devices over an image file with sector sizes other than the 512
 * bytes of blockdev/linux/file_dev: sector_dev_4k (4096 byte sectors, as on
 * "4Kn" disks) and sector_dev_256 (a sector size lwext4's partition code
 * does not support). Header only, include it in one test.
 */

#ifndef LWEXT4_TEST_SECTOR_DEV_H_
#define LWEXT4_TEST_SECTOR_DEV_H_

#include <ext4.h>

#include <stdio.h>

static const char *sector_dev_name;
static FILE *sector_dev_file;

static int sector_dev_open(struct ext4_blockdev *bdev);
static int sector_dev_bread(struct ext4_blockdev *bdev, void *buf,
			    uint64_t blk_id, uint32_t blk_cnt);
static int sector_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			     uint64_t blk_id, uint32_t blk_cnt);
static int sector_dev_close(struct ext4_blockdev *bdev);

EXT4_BLOCKDEV_STATIC_INSTANCE(sector_dev_4k, 4096, 0, sector_dev_open,
			      sector_dev_bread, sector_dev_bwrite,
			      sector_dev_close, 0, 0);
EXT4_BLOCKDEV_STATIC_INSTANCE(sector_dev_256, 256, 0, sector_dev_open,
			      sector_dev_bread, sector_dev_bwrite,
			      sector_dev_close, 0, 0);

/* The whole-device instance behind bdev (bdev may be a partition). */
static struct ext4_blockdev *sector_dev_parent(struct ext4_blockdev *bdev)
{
	return bdev->bdif == sector_dev_4k.bdif ? &sector_dev_4k
						: &sector_dev_256;
}

static int sector_dev_open(struct ext4_blockdev *bdev)
{
	struct ext4_blockdev *parent = sector_dev_parent(bdev);
	long size;

	sector_dev_file = fopen(sector_dev_name, "r+b");
	if (!sector_dev_file)
		return EIO;
	if (fseek(sector_dev_file, 0, SEEK_END) ||
	    (size = ftell(sector_dev_file)) < 0) {
		fclose(sector_dev_file);
		return EIO;
	}
	parent->part_offset = 0;
	parent->part_size = size;
	parent->bdif->ph_bcnt = size / parent->bdif->ph_bsize;
	return EOK;
}

static int sector_dev_bread(struct ext4_blockdev *bdev, void *buf,
			    uint64_t blk_id, uint32_t blk_cnt)
{
	uint32_t bsize = bdev->bdif->ph_bsize;

	if (fseek(sector_dev_file, (long)(blk_id * bsize), SEEK_SET))
		return EIO;
	if (blk_cnt && fread(buf, bsize * blk_cnt, 1, sector_dev_file) != 1)
		return EIO;
	return EOK;
}

static int sector_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			     uint64_t blk_id, uint32_t blk_cnt)
{
	uint32_t bsize = bdev->bdif->ph_bsize;

	if (fseek(sector_dev_file, (long)(blk_id * bsize), SEEK_SET))
		return EIO;
	if (blk_cnt && fwrite(buf, bsize * blk_cnt, 1, sector_dev_file) != 1)
		return EIO;
	return EOK;
}

static int sector_dev_close(struct ext4_blockdev *bdev)
{
	(void)bdev;
	fclose(sector_dev_file);
	sector_dev_file = NULL;
	return EOK;
}

#endif /* LWEXT4_TEST_SECTOR_DEV_H_ */
