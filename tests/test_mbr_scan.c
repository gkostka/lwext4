/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ext4_mbr_scan on MBRs written by sfdisk (see test_mbr_scan.sh): it
 * reports the four primary partition slots, a block device for each Linux
 * (0x83) partition, nothing for other types, empty slots and extended
 * partitions, and ENOENT without boot signature. The Linux partitions
 * mount.
 *
 * red-green: guard (ext4_mbr_scan must keep behaving as before the
 * partition table work that added ext4_partition_scan)
 */

#include "test_util.h"

#include <ext4_mbr.h>

#include "../blockdev/linux/file_dev.h"

#include <string.h>

#define PART_DEV "mbr_part"
#define PART_MP "/mbr/"

static struct ext4_mbr_bdevs bdevs;
static const char *image;
static char path[4096];

static struct ext4_blockdev *disk(const char *suffix)
{
	snprintf(path, sizeof(path), "%s%s", image, suffix);
	file_dev_name_set(path);
	return file_dev_get();
}

static void check_mount(struct ext4_blockdev *bd, const char *expect)
{
	ext4_file f;
	char buf[64];
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_device_register(bd, PART_DEV));
	TEST_ASSERT_EQ(EOK, ext4_mount(PART_DEV, PART_MP, true));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, PART_MP "hello.txt", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(strlen(expect), rcnt);
	TEST_ASSERT(memcmp(buf, expect, rcnt) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_umount(PART_MP));
	TEST_ASSERT_EQ(EOK, ext4_device_unregister(PART_DEV));
}

static void check_part(int i, struct ext4_blockdev *parent, uint64_t first,
		       uint64_t sectors)
{
	TEST_ASSERT(bdevs.partitions[i].bdif == parent->bdif);
	TEST_ASSERT_EQ(first * 512, bdevs.partitions[i].part_offset);
	TEST_ASSERT_EQ(sectors * 512, bdevs.partitions[i].part_size);
}

int main(int argc, char **argv)
{
	struct ext4_blockdev *parent;

	image = test_image_arg(argc, argv);

	parent = disk("");
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(parent, &bdevs));
	check_part(0, parent, 2048, 4096);
	TEST_ASSERT(bdevs.partitions[1].bdif == NULL); /* FAT32 */
	check_part(2, parent, 8192, 4096);
	TEST_ASSERT(bdevs.partitions[3].bdif == NULL); /* empty */
	check_mount(&bdevs.partitions[0], "mbr p1\n");
	check_mount(&bdevs.partitions[2], "mbr p3\n");

	/* Extended partitions and their logical partitions are not reported */
	parent = disk(".ext");
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(parent, &bdevs));
	check_part(0, parent, 2048, 4096);
	TEST_ASSERT(bdevs.partitions[1].bdif == NULL);
	TEST_ASSERT(bdevs.partitions[2].bdif == NULL);
	TEST_ASSERT(bdevs.partitions[3].bdif == NULL);

	memset(&bdevs, 0xff, sizeof(bdevs));
	TEST_ASSERT_EQ(ENOENT, ext4_mbr_scan(disk(".blank"), &bdevs));
	TEST_ASSERT(bdevs.partitions[0].bdif == NULL);

	printf("test_mbr_scan: ok\n");
	return 0;
}
