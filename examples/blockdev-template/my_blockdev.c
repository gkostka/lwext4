/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Template for a custom lwext4 block device.
 *
 * lwext4 does all its I/O through a struct ext4_blockdev: a table of
 * functions (open, bread, bwrite, close and optionally lock/unlock) plus the
 * physical sector size and count. To run lwext4 on new hardware you write
 * one of these; everything above it (ext4_mkfs, ext4_mount, ext4_fopen...)
 * stays the same.
 *
 * Copy this file (and my_blockdev.h) into your project, rename my_blockdev
 * and replace the storage_*() calls with your driver (see storage.h). As it
 * is, it runs on the RAM disk of ram_storage.c; main.c formats, mounts and
 * uses it, and CI checks the result with e2fsck.
 *
 * Rules every block device must follow:
 *
 *  - Block numbers passed to bread/bwrite are *physical* sector numbers
 *    (units of ph_bsize), counted from the start of the medium. lwext4
 *    already adds part_offset, so a device for one partition (see
 *    ext4_mbr_scan() in ext4_mbr.h) needs no extra arithmetic here.
 *  - Transfer exactly blk_cnt sectors or fail: there are no short reads.
 *  - Return EOK (0) on success and an errno value (EIO...) on failure.
 *    lwext4 passes the error up to the API call that caused it.
 *  - Buffers have no alignment guarantee. If your DMA needs aligned
 *    memory, bounce through an aligned buffer.
 */

#include "my_blockdev.h"
#include "storage.h"

#include <ext4_errno.h>

/* 1. The callbacks. They are static: lwext4 only sees them through the
 *    ext4_blockdev instance below. */
static int my_open(struct ext4_blockdev *bdev);
static int my_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		    uint32_t blk_cnt);
static int my_bwrite(struct ext4_blockdev *bdev, const void *buf,
		     uint64_t blk_id, uint32_t blk_cnt);
static int my_close(struct ext4_blockdev *bdev);
static int my_lock(struct ext4_blockdev *bdev);
static int my_unlock(struct ext4_blockdev *bdev);

/* 2. The instance. EXT4_BLOCKDEV_STATIC_INSTANCE defines
 *
 *      static struct ext4_blockdev my_blockdev;
 *
 *    together with its interface and a one sector buffer (so no heap is
 *    needed for it). Arguments: name, sector size, sector count (0 here:
 *    my_open() fills it in once the medium is known), then the callbacks.
 *    lock/unlock may be NULL when only one thread uses lwext4. */
EXT4_BLOCKDEV_STATIC_INSTANCE(my_blockdev, STORAGE_SECTOR_SIZE, 0, my_open,
			      my_bread, my_bwrite, my_close, my_lock,
			      my_unlock);

/* 3. open: called by ext4_mount() and ext4_mkfs() (through
 *    ext4_block_init()). Initialise the hardware and report its size. */
static int my_open(struct ext4_blockdev *bdev)
{
	if (storage_init() != 0)
		return EIO;

	/* Physical size of the medium, in sectors... */
	bdev->bdif->ph_bcnt = storage_sector_count();
	/* ...and the part of it that holds the filesystem, in bytes. For a
	 * whole-disk filesystem that is everything. For a partition set
	 * part_offset/part_size from the partition table instead, or use
	 * ext4_mbr_scan(), which fills them in for you. */
	bdev->part_offset = 0;
	bdev->part_size = bdev->bdif->ph_bcnt * bdev->bdif->ph_bsize;
	return EOK;
}

/* 4. bread/bwrite: move blk_cnt whole sectors starting at blk_id. lwext4
 *    reads and writes filesystem blocks (1-4 KiB, several sectors) and
 *    groups consecutive blocks, so multi-sector transfers are common:
 *    pass them to the driver in one go if it supports that. */
static int my_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		    uint32_t blk_cnt)
{
	(void)bdev;
	if (storage_read(blk_id, buf, blk_cnt) != 0)
		return EIO;
	return EOK;
}

static int my_bwrite(struct ext4_blockdev *bdev, const void *buf,
		     uint64_t blk_id, uint32_t blk_cnt)
{
	(void)bdev;
	if (storage_write(blk_id, buf, blk_cnt) != 0)
		return EIO;
	return EOK;
}

/* 5. close: called by ext4_umount() (through ext4_block_fini()) after
 *    the block cache has been written back. Flush the medium's own
 *    buffers so that the filesystem survives a power cut from here on. */
static int my_close(struct ext4_blockdev *bdev)
{
	int r = storage_sync();

	(void)bdev;
	storage_deinit();
	return r == 0 ? EOK : EIO;
}

/* 6. lock/unlock: bracket every access to the medium. They matter when
 *    several block devices (partitions) share one medium and are used
 *    from different threads. With an RTOS take/give a mutex here, e.g.
 *    xSemaphoreTake(storage_mutex, portMAX_DELAY) / xSemaphoreGive(). Locking of
 *    the filesystem itself is separate: see ext4_mount_setup_locks(). */
static int my_lock(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

static int my_unlock(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

/* 7. The only public symbol: hand the instance to ext4_mkfs() and
 *    ext4_device_register(). */
struct ext4_blockdev *my_blockdev_get(void)
{
	return &my_blockdev;
}
