/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ext4_mbr_write / ext4_mbr_scan on devices whose sector size is not 512:
 *
 * - 4096 byte sectors: ext4_mbr_write built the MBR in the interface's
 *   block buffer and wrote only its first 512 bytes. For a partial sector
 *   ext4_block_writebytes first reads the whole sector into that same
 *   buffer, so the old sector contents were written back and the MBR never
 *   reached the disk. (ext4_mbr_scan had the same pattern, a memcpy of the
 *   buffer onto itself, harmless in practice; it reads whole sectors now
 *   too.)
 * - Sectors smaller than the 512 byte MBR overflowed the block buffer; they
 *   are rejected with ENOTSUP.
 *
 * Also: the disk signature ("disk identifier" in fdisk, "label-id" in
 * sfdisk) lives at offset 440 (0x1B8) of the MBR, followed by two reserved
 * bytes; ext4_mbr_write stored it at offset 442.
 */

#include "test_util.h"
#include "sector_dev.h"

#include <ext4_mbr.h>

#include "../blockdev/linux/file_dev.h"

#include <string.h>

#define DISK_ID 0x12345678

static struct ext4_mbr_bdevs bdevs;
static uint8_t sector[4096];
static char path[4096];

static void read_sector0(const char *file, size_t size)
{
	FILE *f = fopen(file, "rb");

	TEST_ASSERT(f);
	TEST_ASSERT_EQ(1, fread(sector, size, 1, f));
	fclose(f);
}

static uint32_t le32(const uint8_t *p)
{
	return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	       (uint32_t)p[3] << 24;
}

/* sector holds the MBR ext4_mbr_write wrote for divisions {50, 50}. */
static void check_written_mbr(uint32_t bsize)
{
	size_t i;

	TEST_ASSERT_EQ(0x55, sector[510]);
	TEST_ASSERT_EQ(0xAA, sector[511]);
	TEST_ASSERT_EQ(DISK_ID, le32(sector + 440));
	TEST_ASSERT_EQ(0, sector[444]);
	TEST_ASSERT_EQ(0, sector[445]);
	TEST_ASSERT_EQ(0x83, sector[446 + 4]);
	TEST_ASSERT_EQ(0x83, sector[446 + 16 + 4]);
	TEST_ASSERT_EQ(0, sector[446 + 32 + 4]);
	TEST_ASSERT_EQ(0, sector[446 + 48 + 4]);
	/* The rest of the sector is cleared */
	for (i = 512; i < bsize; i++)
		TEST_ASSERT_EQ(0, sector[i]);
}

static void check_scanned(struct ext4_blockdev *parent, uint32_t bsize)
{
	int i;

	for (i = 0; i < 2; i++) {
		const uint8_t *pe = sector + 446 + 16 * i;

		TEST_ASSERT(bdevs.partitions[i].bdif == parent->bdif);
		TEST_ASSERT_EQ((uint64_t)le32(pe + 8) * bsize,
			       bdevs.partitions[i].part_offset);
		TEST_ASSERT_EQ((uint64_t)le32(pe + 12) * bsize,
			       bdevs.partitions[i].part_size);
	}
	TEST_ASSERT(bdevs.partitions[2].bdif == NULL);
	TEST_ASSERT(bdevs.partitions[3].bdif == NULL);
}

int main(int argc, char **argv)
{
	struct ext4_mbr_parts parts = {{50, 50, 0, 0}};
	const char *image = test_image_arg(argc, argv);
	struct ext4_blockdev *bd;

	/* 512 byte sectors: disk signature at offset 440 */
	file_dev_name_set(image);
	bd = file_dev_get();
	TEST_ASSERT_EQ(EOK, ext4_mbr_write(bd, &parts, DISK_ID));
	read_sector0(image, 512);
	check_written_mbr(512);
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(bd, &bdevs));
	check_scanned(bd, 512);

	/* 4096 byte sectors: write and scan back */
	snprintf(path, sizeof(path), "%s.4k", image);
	sector_dev_name = path;
	TEST_ASSERT_EQ(EOK, ext4_mbr_write(&sector_dev_4k, &parts, DISK_ID));
	read_sector0(path, 4096);
	check_written_mbr(4096);
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(&sector_dev_4k, &bdevs));
	check_scanned(&sector_dev_4k, 4096);

	/* 4096 byte sectors: an MBR written by another tool */
	snprintf(path, sizeof(path), "%s.4k-scan", image);
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(&sector_dev_4k, &bdevs));
	TEST_ASSERT(bdevs.partitions[0].bdif == sector_dev_4k.bdif);
	TEST_ASSERT_EQ(256 * 4096, bdevs.partitions[0].part_offset);
	TEST_ASSERT_EQ(1024 * 4096, bdevs.partitions[0].part_size);
	TEST_ASSERT(bdevs.partitions[1].bdif == NULL);

	/* Sectors smaller than an MBR */
	TEST_ASSERT_EQ(ENOTSUP, ext4_mbr_scan(&sector_dev_256, &bdevs));
	TEST_ASSERT(bdevs.partitions[0].bdif == NULL);
	TEST_ASSERT_EQ(ENOTSUP, ext4_mbr_write(&sector_dev_256, &parts, 0));

	printf("test_mbr_sector_size: ok\n");
	return 0;
}
