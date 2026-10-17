/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * The fault injecting block device of common/fault_dev.h, which the error
 * path tests rely on: it fails exactly the reads and writes it is armed for
 * (by number and by byte range), passes everything else through unchanged,
 * never writes a failed write to the image, and its errors reach the
 * caller of the lwext4 API.
 *
 * red-green: guard (a test helper, not a fix: it passes on the base).
 */

#include "fault_dev.h"

#include <string.h>

#define SECTOR 512
#define DATA_SIZE 65536

static uint8_t sector[SECTOR], back[SECTOR];

static int rd(uint64_t sec, uint32_t cnt)
{
	static uint8_t buf[4 * SECTOR];

	TEST_ASSERT(cnt <= 4);
	return fault_dev.bdif->bread(&fault_dev, buf, sec, cnt);
}

static void device_level(const char *image)
{
	file_dev_name_set(image);
	fault_dev_wrap(file_dev_get());
	TEST_ASSERT_EQ(EOK, fault_dev.bdif->open(&fault_dev));
	TEST_ASSERT_EQ(SECTOR, fault_dev.bdif->ph_bsize);
	TEST_ASSERT_EQ(4 * 1024 * 1024 / SECTOR, fault_dev.bdif->ph_bcnt);

	/* Nothing armed: everything passes. */
	TEST_ASSERT_EQ(EOK, rd(0, 1));

	/* Reads 2 and 3 after arming fail, 1 and 4 do not; writes are not
	 * affected by a read fault. */
	fault_dev_fail_nth(FAULT_DEV_READ, 2, 2);
	TEST_ASSERT_EQ(EOK, rd(0, 1));
	TEST_ASSERT_EQ(EIO, rd(1, 1));
	TEST_ASSERT_EQ(EIO, rd(2, 1));
	TEST_ASSERT_EQ(EOK, rd(3, 1));
	TEST_ASSERT_EQ(2, fault_dev_failed());
	TEST_ASSERT_EQ(EOK, fault_dev.bdif->bread(&fault_dev, sector, 5, 1));
	TEST_ASSERT_EQ(EOK, fault_dev.bdif->bwrite(&fault_dev, sector, 5, 1));

	/* count 0: every read from the first on fails. */
	fault_dev_fail_nth(FAULT_DEV_READ, 1, 0);
	for (int i = 0; i < 10; i++)
		TEST_ASSERT_EQ(EIO, rd(i, 1));
	TEST_ASSERT_EQ(10, fault_dev_failed());

	/* A failed write does not reach the image. */
	fault_dev_fail_nth(FAULT_DEV_WRITE, 1, 1);
	memset(back, 0xa5, sizeof(back));
	TEST_ASSERT_EQ(EIO, fault_dev.bdif->bwrite(&fault_dev, back, 5, 1));
	TEST_ASSERT_EQ(EOK, fault_dev.bdif->bread(&fault_dev, back, 5, 1));
	TEST_ASSERT(memcmp(back, sector, SECTOR) == 0);
	TEST_ASSERT_EQ(EOK, fault_dev.bdif->bwrite(&fault_dev, sector, 5, 1));

	/* Byte range [4096, 5120): sectors 8 and 9 and anything that overlaps
	 * them. */
	fault_dev_fail_range(FAULT_DEV_READ | FAULT_DEV_WRITE, 4096, 1024);
	TEST_ASSERT_EQ(EOK, rd(7, 1));
	TEST_ASSERT_EQ(EIO, rd(8, 1));
	TEST_ASSERT_EQ(EIO, rd(9, 1));
	TEST_ASSERT_EQ(EOK, rd(10, 1));
	TEST_ASSERT_EQ(EIO, rd(6, 3));
	TEST_ASSERT_EQ(EIO, rd(9, 4));
	TEST_ASSERT_EQ(EOK, rd(4, 4));
	TEST_ASSERT_EQ(EIO, fault_dev.bdif->bwrite(&fault_dev, sector, 8, 1));
	TEST_ASSERT_EQ(5, fault_dev_failed());

	fault_dev_disarm();
	TEST_ASSERT_EQ(EOK, rd(8, 1));
	TEST_ASSERT_EQ(EOK, fault_dev.bdif->close(&fault_dev));
}

static void through_lwext4(const char *image)
{
	static char data[DATA_SIZE];
	ext4_file f;
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "data", "rb"));

	/* The file's data blocks are not cached: the read fails. */
	fault_dev_fail_nth(FAULT_DEV_READ, 1, 0);
	TEST_ASSERT_EQ(EIO, ext4_fread(&f, data, DATA_SIZE, &rcnt));
	TEST_ASSERT(fault_dev_failed() > 0);
	fault_dev_disarm();

	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, 0, SEEK_SET));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, data, DATA_SIZE, &rcnt));
	TEST_ASSERT_EQ(DATA_SIZE, rcnt);
	for (int i = 0; i < DATA_SIZE; i++)
		TEST_ASSERT_EQ('A' + i % 26, data[i]);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	device_level(image);
	through_lwext4(image);
	return 0;
}
