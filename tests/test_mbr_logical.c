/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Logical MBR partitions (gkostka/lwext4#55): ext4_mbr_scan_all /
 * ext4_partition_scan follow the chain of extended boot records of an
 * extended partition, on disk images written by sfdisk (see
 * test_mbr_logical.sh).
 *
 * - Three logical partitions are reported after the primary partitions and
 *   numbered 5, 6, 7; the ext4 filesystems in them mount.
 * - An EBR without data partition is skipped, link and data records are
 *   accepted in any slot.
 * - Loops, EBRs outside the extended partition, an extended partition
 *   beyond the device and an EBR without signature are errors; logical and
 *   primary partitions outside their bounds are skipped.
 * - A plain MBR gives the same partitions as ext4_mbr_scan.
 * - 4096 byte sectors.
 */

#include "test_util.h"
#include "sector_dev.h"

#include <ext4_mbr.h>
#include <ext4_partition.h>

#include "../blockdev/linux/file_dev.h"

#include <string.h>

#define PART_DEV "mbr_part"
#define PART_MP "/mbr/"

static struct ext4_part_bdevs bdevs;
static const char *image;
static char path[4096];

static struct ext4_blockdev *disk(const char *suffix)
{
	snprintf(path, sizeof(path), "%s%s", image, suffix);
	file_dev_name_set(path);
	return file_dev_get();
}

/* Mount the partition block device and compare /hello.txt. */
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

struct part {
	uint32_t number;
	uint8_t type;
	uint64_t first, last;
	const char *hello; /* ext4 with this /hello.txt, or NULL */
};

/* bdevs holds exactly the n partitions of expect, 512 byte sectors. */
static void check_parts(struct ext4_blockdev *parent,
			const struct part *expect, uint32_t n)
{
	uint32_t i;

	TEST_ASSERT_EQ(EXT4_PART_TABLE_MBR, bdevs.table);
	TEST_ASSERT_EQ(0, bdevs.flags);
	TEST_ASSERT_EQ(n, bdevs.count);
	TEST_ASSERT_EQ(n, bdevs.total);
	for (i = 0; i < n; i++) {
		const struct ext4_part_info *info = &bdevs.info[i];
		const struct ext4_blockdev *bd = &bdevs.partitions[i];

		TEST_ASSERT_EQ(expect[i].number, info->number);
		TEST_ASSERT_EQ(expect[i].type, info->mbr_type);
		TEST_ASSERT_EQ(expect[i].first, info->first_lba);
		TEST_ASSERT_EQ(expect[i].last, info->last_lba);
		TEST_ASSERT_EQ(expect[i].type == 0x83,
			       ext4_part_is_linux(info));
		TEST_ASSERT_EQ(info->number == 1 ? 0x80 : 0,
			       info->attributes);
		TEST_ASSERT_EQ(0, info->name[0]);
		TEST_ASSERT(bd->bdif == parent->bdif);
		TEST_ASSERT_EQ(expect[i].first * 512, bd->part_offset);
		TEST_ASSERT_EQ((expect[i].last - expect[i].first + 1) * 512,
			       bd->part_size);
	}
	TEST_ASSERT(bdevs.partitions[n].bdif == NULL);

	for (i = 0; i < n; i++)
		if (expect[i].hello)
			check_mount(&bdevs.partitions[i], expect[i].hello);
}

/* Both scan functions accept the image and report expect. */
static void check_image(const char *suffix, const struct part *expect,
			uint32_t n)
{
	fprintf(stderr, "image '%s'\n", suffix);
	TEST_ASSERT_EQ(EOK, ext4_partition_scan(disk(suffix), &bdevs));
	check_parts(file_dev_get(), expect, n);
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan_all(disk(suffix), &bdevs));
	check_parts(file_dev_get(), expect, n);
}

static void check_failed(const char *suffix, int expect)
{
	fprintf(stderr, "image '%s'\n", suffix);
	TEST_ASSERT_EQ(expect, ext4_partition_scan(disk(suffix), &bdevs));
	TEST_ASSERT_EQ(EXT4_PART_TABLE_NONE, bdevs.table);
	TEST_ASSERT_EQ(0, bdevs.count);
	TEST_ASSERT_EQ(0, bdevs.total);
	TEST_ASSERT(bdevs.partitions[0].bdif == NULL);
	TEST_ASSERT_EQ(expect, ext4_mbr_scan_all(disk(suffix), &bdevs));
	TEST_ASSERT_EQ(0, bdevs.count);
}

#define P1 {1, 0x83, 2048, 6143, "mbr p1\n"}
#define P3 {3, 0x0c, 34816, 38911, NULL}

static void test_logical(void)
{
	static const struct part main_parts[] = {
	    P1,
	    P3,
	    {5, 0x83, 8192, 12287, "mbr p5\n"},
	    {6, 0x82, 14336, 16383, NULL},
	    {7, 0x83, 18432, 22527, "mbr p7\n"},
	};
	static const struct part empty[] = {
	    P1,
	    P3,
	    {5, 0x82, 14336, 16383, NULL},
	    {6, 0x83, 18432, 22527, "mbr p7\n"},
	};
	static const struct part no_p6[] = {
	    P1,
	    P3,
	    {5, 0x83, 8192, 12287, "mbr p5\n"},
	    {6, 0x83, 18432, 22527, "mbr p7\n"},
	};
	static const struct part no_p3[] = {
	    P1,
	    {5, 0x83, 8192, 12287, "mbr p5\n"},
	    {6, 0x82, 14336, 16383, NULL},
	    {7, 0x83, 18432, 22527, "mbr p7\n"},
	};

	check_image("", main_parts, 5);
	check_image(".swapped", main_parts, 5);
	check_image(".empty", empty, 4);
	check_image(".logical-oob", no_p6, 4);
	check_image(".logical-at-ebr", no_p6, 4);
	check_image(".primary-oob", no_p3, 4);
	check_image(".primary-zero", no_p3, 4);
}

static void test_broken_chain(void)
{
	check_failed(".loop", EIO);
	check_failed(".self-loop", EIO);
	check_failed(".oob", EIO);
	check_failed(".oob-far", EIO);
	check_failed(".ext-oob", EIO);
	check_failed(".nosig", EIO);
}

/* Plain MBR: the same partitions as ext4_mbr_scan (only 0x83 there). */
static void test_plain(void)
{
	static const struct part plain[] = {
	    {1, 0x83, 2048, 6143, "mbr p1\n"},
	    {2, 0x0c, 6144, 8191, NULL},
	    {3, 0x83, 8192, 12287, "mbr p3\n"},
	};
	static struct ext4_mbr_bdevs mbr;
	int i;

	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(disk(".plain"), &mbr));
	TEST_ASSERT_EQ(EOK, ext4_partition_scan(disk(".plain"), &bdevs));
	for (i = 0; i < 4; i++) {
		const struct ext4_blockdev *old = &mbr.partitions[i];
		const struct ext4_blockdev *bd = &bdevs.partitions[i];

		if (i == 0 || i == 2) {
			TEST_ASSERT(old->bdif == bd->bdif);
			TEST_ASSERT_EQ(old->part_offset, bd->part_offset);
			TEST_ASSERT_EQ(old->part_size, bd->part_size);
		} else {
			TEST_ASSERT(old->bdif == NULL);
		}
	}
	check_parts(file_dev_get(), plain, 3);
}

static void test_4k_sectors(void)
{
	snprintf(path, sizeof(path), "%s.4k", image);
	sector_dev_name = path;
	TEST_ASSERT_EQ(EOK, ext4_partition_scan(&sector_dev_4k, &bdevs));
	TEST_ASSERT_EQ(EXT4_PART_TABLE_MBR, bdevs.table);
	TEST_ASSERT_EQ(2, bdevs.count);
	TEST_ASSERT_EQ(1, bdevs.info[0].number);
	TEST_ASSERT_EQ(256, bdevs.info[0].first_lba);
	TEST_ASSERT_EQ(1279, bdevs.info[0].last_lba);
	TEST_ASSERT_EQ(256 * 4096, bdevs.partitions[0].part_offset);
	TEST_ASSERT_EQ(1024 * 4096, bdevs.partitions[0].part_size);
	TEST_ASSERT_EQ(5, bdevs.info[1].number);
	TEST_ASSERT_EQ(1536, bdevs.info[1].first_lba);
	TEST_ASSERT_EQ(2047, bdevs.info[1].last_lba);
	TEST_ASSERT_EQ(1536 * 4096, bdevs.partitions[1].part_offset);
	TEST_ASSERT_EQ(512 * 4096, bdevs.partitions[1].part_size);
	check_mount(&bdevs.partitions[0], "mbr 4k p1\n");
	check_mount(&bdevs.partitions[1], "mbr 4k p5\n");

	/* Records could cross sector boundaries: not supported */
	TEST_ASSERT_EQ(ENOTSUP, ext4_mbr_scan_all(&sector_dev_256, &bdevs));
}

int main(int argc, char **argv)
{
	image = test_image_arg(argc, argv);

	test_logical();
	test_broken_chain();
	test_plain();
	test_4k_sectors();

	printf("test_mbr_logical: ok\n");
	return 0;
}
