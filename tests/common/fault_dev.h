/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Fault injecting block device for the error path tests. fault_dev wraps
 * another block device (normally blockdev/linux/file_dev), passes every
 * read and write through to it and fails the ones selected with
 * fault_dev_fail_nth() or fault_dev_fail_range() with EIO, without
 * touching the image.
 *
 *	fault_dev_mount(image, false);             register + mount
 *	...                                        prepare
 *	fault_dev_fail_nth(FAULT_DEV_READ, 1, 0);  every read from now on fails
 *	r = ext4_...();                            the operation under test
 *	fault_dev_disarm();
 *	test_umount();
 *
 * Header only, include it in one test.
 */

#ifndef LWEXT4_TEST_FAULT_DEV_H_
#define LWEXT4_TEST_FAULT_DEV_H_

#include "test_util.h"

#include "../../blockdev/linux/file_dev.h"

#include <ext4_blockdev.h>

#include <stdint.h>

#define FAULT_DEV_READ 1
#define FAULT_DEV_WRITE 2

#define FAULT_DEV_MAX_BSIZE 4096

struct fault_dev_state {
	/* The wrapped device. */
	struct ext4_blockdev *inner;
	/* Ops that can fail: FAULT_DEV_READ | FAULT_DEV_WRITE, 0 = none. */
	int ops;
	/* Fail matching op number nth (1 = the next one) and count - 1 more
	 * after it; count 0 = every matching op from nth on. */
	uint64_t nth;
	uint64_t count;
	/* Only ops touching device bytes [lo, hi) match. */
	uint64_t lo, hi;
	/* Matching ops since the last arm, and how many of them failed. */
	uint64_t seen;
	uint64_t failed;
	/* Every read and write passed to the device (failed ones included). */
	uint64_t reads;
	uint64_t writes;
};

static struct fault_dev_state fault_dev_state;

static int fault_dev_open(struct ext4_blockdev *bdev);
static int fault_dev_bread(struct ext4_blockdev *bdev, void *buf,
			   uint64_t blk_id, uint32_t blk_cnt);
static int fault_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			    uint64_t blk_id, uint32_t blk_cnt);
static int fault_dev_close(struct ext4_blockdev *bdev);

static uint8_t fault_dev_ph_bbuf[FAULT_DEV_MAX_BSIZE];
static struct ext4_blockdev_iface fault_dev_iface = {
	.open = fault_dev_open,
	.bread = fault_dev_bread,
	.bwrite = fault_dev_bwrite,
	.close = fault_dev_close,
	.ph_bbuf = fault_dev_ph_bbuf,
};
static struct ext4_blockdev fault_dev = {
	.bdif = &fault_dev_iface,
};

/* Wrap inner; the returned device is what gets registered. */
static inline struct ext4_blockdev *fault_dev_wrap(struct ext4_blockdev *inner)
{
	TEST_ASSERT(inner->bdif->ph_bsize <= FAULT_DEV_MAX_BSIZE);
	fault_dev_state.inner = inner;
	fault_dev_iface.ph_bsize = inner->bdif->ph_bsize;
	fault_dev_iface.ph_bcnt = inner->bdif->ph_bcnt;
	fault_dev.part_offset = inner->part_offset;
	fault_dev.part_size = inner->part_size;
	return &fault_dev;
}

/* Fail matching op number nth from now on and count - 1 more (count 0: all
 * of them). ops is a mask of FAULT_DEV_READ and FAULT_DEV_WRITE. */
static inline void fault_dev_fail_nth(int ops, uint64_t nth, uint64_t count)
{
	fault_dev_state.ops = ops;
	fault_dev_state.nth = nth;
	fault_dev_state.count = count;
	fault_dev_state.lo = 0;
	fault_dev_state.hi = UINT64_MAX;
	fault_dev_state.seen = 0;
	fault_dev_state.failed = 0;
}

/* Fail every op of the ops mask that touches device bytes [off, off+len). */
static inline void fault_dev_fail_range(int ops, uint64_t off, uint64_t len)
{
	fault_dev_fail_nth(ops, 1, 0);
	fault_dev_state.lo = off;
	fault_dev_state.hi = off + len;
}

static inline void fault_dev_disarm(void)
{
	fault_dev_state.ops = 0;
}

/* Number of ops failed since the last arm. */
static inline uint64_t fault_dev_failed(void)
{
	return fault_dev_state.failed;
}

static bool fault_dev_hit(int op, uint64_t blk_id, uint32_t blk_cnt)
{
	struct fault_dev_state *s = &fault_dev_state;
	uint64_t bsize = fault_dev_iface.ph_bsize;
	uint64_t off = blk_id * bsize;
	uint64_t end = off + blk_cnt * bsize;
	uint64_t n;

	if (!(s->ops & op) || end <= s->lo || off >= s->hi)
		return false;
	n = ++s->seen;
	if (n < s->nth || (s->count && n >= s->nth + s->count))
		return false;
	s->failed++;
	return true;
}

static int fault_dev_open(struct ext4_blockdev *bdev)
{
	struct ext4_blockdev *inner = fault_dev_state.inner;
	int r;

	(void)bdev;
	r = inner->bdif->open(inner);
	if (r != EOK)
		return r;
	fault_dev_wrap(inner);
	return EOK;
}

static int fault_dev_bread(struct ext4_blockdev *bdev, void *buf,
			   uint64_t blk_id, uint32_t blk_cnt)
{
	struct ext4_blockdev *inner = fault_dev_state.inner;

	(void)bdev;
	fault_dev_state.reads++;
	if (fault_dev_hit(FAULT_DEV_READ, blk_id, blk_cnt))
		return EIO;
	return inner->bdif->bread(inner, buf, blk_id, blk_cnt);
}

static int fault_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			    uint64_t blk_id, uint32_t blk_cnt)
{
	struct ext4_blockdev *inner = fault_dev_state.inner;

	(void)bdev;
	fault_dev_state.writes++;
	if (fault_dev_hit(FAULT_DEV_WRITE, blk_id, blk_cnt))
		return EIO;
	return inner->bdif->bwrite(inner, buf, blk_id, blk_cnt);
}

static int fault_dev_close(struct ext4_blockdev *bdev)
{
	struct ext4_blockdev *inner = fault_dev_state.inner;

	(void)bdev;
	return inner->bdif->close(inner);
}

/* test_mount() on a fault_dev around file_dev: register the image as
 * TEST_DEV and mount it at TEST_MP. Unmount with test_umount(). */
static inline int fault_dev_mount(const char *image, bool read_only)
{
	file_dev_name_set(image);
	fault_dev_disarm();
	TEST_ASSERT_EQ(EOK, ext4_device_register(fault_dev_wrap(file_dev_get()),
						 TEST_DEV));
	return ext4_mount(TEST_DEV, TEST_MP, read_only);
}

#endif /* LWEXT4_TEST_FAULT_DEV_H_ */
