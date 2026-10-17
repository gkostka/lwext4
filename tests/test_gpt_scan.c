/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * GPT support (gkostka/lwext4#82): ext4_gpt_scan / ext4_partition_scan on
 * disk images written by sfdisk (see test_gpt_scan.sh).
 *
 * - Six partitions: LBA ranges, type and unique GUIDs, attributes and
 *   UTF-16 names (including surrogate pairs and unpaired surrogates) are
 *   reported, the partition block devices mount and the ext4 filesystems
 *   in them can be read.
 * - A damaged primary header or primary entry array falls back to the
 *   backup GPT at the end of the device; both copies damaged is an error.
 * - Every header check of the UEFI specification (signature, revision,
 *   header size, header CRC, MyLBA, usable range, entry size, entry array
 *   location and CRC) rejects a header that fails it, a header that passes
 *   is used, partition entries outside the usable range are skipped.
 * - Entry sizes above 128 bytes, more partitions than
 *   CONFIG_EXT4_PARTITIONS_COUNT, 4096 byte sectors, unsupported sector
 *   sizes, a missing protective MBR, a hybrid MBR and a blank device.
 */

#include "test_util.h"
#include "sector_dev.h"

#include <ext4_partition.h>

#include "../blockdev/linux/file_dev.h"

#include <string.h>

#define PART_DEV "gpt_part"
#define PART_MP "/gpt/"

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

static void check_failed(int expect, int r)
{
	TEST_ASSERT_EQ(expect, r);
	TEST_ASSERT_EQ(EXT4_PART_TABLE_NONE, bdevs.table);
	TEST_ASSERT_EQ(0, bdevs.count);
	TEST_ASSERT_EQ(0, bdevs.total);
	TEST_ASSERT(bdevs.partitions[0].bdif == NULL);
}

static const uint8_t linux_fs[16] = EXT4_GPT_TYPE_LINUX_FS;
/* C12A7328-F81F-11D2-BA4B-00A0C93EC93B in on-disk byte order */
static const uint8_t efi_system[16] = {0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8,
				       0xd2, 0x11, 0xba, 0x4b, 0x00, 0xa0,
				       0xc9, 0x3e, 0xc9, 0x3b};
/* 11111111-2222-3333-4444-555555555555 in on-disk byte order */
static const uint8_t root_uuid[16] = {0x11, 0x11, 0x11, 0x11, 0x22, 0x22,
				      0x33, 0x33, 0x44, 0x44, 0x55, 0x55,
				      0x55, 0x55, 0x55, 0x55};

static const struct {
	uint64_t first, last, attributes;
	const char *name;
	const char *hello; /* ext4 with this /hello.txt, or NULL */
} main_parts[6] = {
	{2048, 6143, 0, "root", "gpt root\n"},
	{6144, 8191, 0, "Gr\xc3\xbc\xc3\x9f" "e \xe2\x82\xac", NULL},
	{8192, 12287, 0, "data", "gpt data\n"},
	{12288, 14335, 0,
	 "\xf0\x90\x8d\x88" "\xef\xbf\xbd" "x" "\xef\xbf\xbd", NULL},
	{14336, 16383, 0, "123456789012345678901234567890123456", NULL},
	{16384, 20479, 5, "six", "gpt six\n"},
};

/* The main image (or a copy whose primary GPT is damaged). */
static void check_main(const char *suffix, uint32_t flags)
{
	struct ext4_blockdev *parent = disk(suffix);
	int pass, i;

	for (pass = 0; pass < 2; pass++) {
		if (pass == 0)
			TEST_ASSERT_EQ(EOK, ext4_partition_scan(parent, &bdevs));
		else
			TEST_ASSERT_EQ(EOK, ext4_gpt_scan(parent, &bdevs));

		TEST_ASSERT_EQ(EXT4_PART_TABLE_GPT, bdevs.table);
		TEST_ASSERT_EQ(flags, bdevs.flags);
		TEST_ASSERT_EQ(6, bdevs.count);
		TEST_ASSERT_EQ(6, bdevs.total);

		for (i = 0; i < 6; i++) {
			const struct ext4_part_info *info = &bdevs.info[i];
			const struct ext4_blockdev *bd = &bdevs.partitions[i];

			TEST_ASSERT_EQ(i + 1, info->number);
			TEST_ASSERT_EQ(0, info->mbr_type);
			TEST_ASSERT_EQ(main_parts[i].first, info->first_lba);
			TEST_ASSERT_EQ(main_parts[i].last, info->last_lba);
			TEST_ASSERT_EQ(main_parts[i].attributes,
				       info->attributes);
			if (strcmp(main_parts[i].name, info->name)) {
				fprintf(stderr, "partition %d: name '%s'\n",
					i + 1, info->name);
				exit(1);
			}
			TEST_ASSERT_EQ(main_parts[i].hello != NULL,
				       ext4_part_is_linux(info));
			TEST_ASSERT(bd->bdif == parent->bdif);
			TEST_ASSERT_EQ(main_parts[i].first * 512,
				       bd->part_offset);
			TEST_ASSERT_EQ((main_parts[i].last -
					main_parts[i].first + 1) * 512,
				       bd->part_size);
		}
		TEST_ASSERT(!memcmp(bdevs.info[0].type_guid, linux_fs, 16));
		TEST_ASSERT(!memcmp(bdevs.info[0].unique_guid, root_uuid, 16));
		TEST_ASSERT(!memcmp(bdevs.info[1].type_guid, efi_system, 16));
		/* sfdisk made up the other unique GUIDs: must differ */
		TEST_ASSERT(memcmp(bdevs.info[1].unique_guid,
				   bdevs.info[2].unique_guid, 16));
	}

	for (i = 0; i < 6; i++)
		if (main_parts[i].hello)
			check_mount(&bdevs.partitions[i], main_parts[i].hello);
}

/* Small image (or a copy of it): the partitions "one" and "two". */
static void check_small(const char *suffix, uint32_t flags)
{
	char s[64];

	snprintf(s, sizeof(s), ".s%s", suffix);
	TEST_ASSERT_EQ(EOK, ext4_gpt_scan(disk(s), &bdevs));
	TEST_ASSERT_EQ(EXT4_PART_TABLE_GPT, bdevs.table);
	TEST_ASSERT_EQ(flags, bdevs.flags);
	TEST_ASSERT_EQ(2, bdevs.count);
	TEST_ASSERT_EQ(2, bdevs.total);
	TEST_ASSERT_EQ(1, bdevs.info[0].number);
	TEST_ASSERT_EQ(40, bdevs.info[0].first_lba);
	TEST_ASSERT_EQ(47, bdevs.info[0].last_lba);
	TEST_ASSERT(!strcmp("one", bdevs.info[0].name));
	TEST_ASSERT_EQ(2, bdevs.info[1].number);
	TEST_ASSERT_EQ(48, bdevs.info[1].first_lba);
	TEST_ASSERT_EQ(55, bdevs.info[1].last_lba);
	TEST_ASSERT(!strcmp("two", bdevs.info[1].name));
}

/* Primary header of the small image rejected: backup used. */
static void check_small_backup(const char *suffix)
{
	check_small(suffix, EXT4_PART_GPT_BACKUP_USED);
}

/* Entry 2 of both copies outside the usable range: skipped. */
static void check_small_entry_skipped(const char *suffix)
{
	char s[64];

	snprintf(s, sizeof(s), ".s%s", suffix);
	TEST_ASSERT_EQ(EOK, ext4_gpt_scan(disk(s), &bdevs));
	TEST_ASSERT_EQ(0, bdevs.flags);
	TEST_ASSERT_EQ(1, bdevs.count);
	TEST_ASSERT_EQ(1, bdevs.total);
	TEST_ASSERT_EQ(1, bdevs.info[0].number);
	TEST_ASSERT(bdevs.partitions[1].bdif == NULL);
}

static void test_main_image(void)
{
	check_main("", 0);
}

static void test_backup_fallback(void)
{
	/* Primary header CRC wrong */
	check_main(".bad-hdr", EXT4_PART_GPT_BACKUP_USED);
	/* Primary entry array CRC wrong */
	check_main(".bad-arr", EXT4_PART_GPT_BACKUP_USED);
	/* Both entry arrays / both headers damaged */
	check_failed(EIO, ext4_partition_scan(disk(".bad-arrs"), &bdevs));
	check_failed(EIO, ext4_gpt_scan(disk(".bad-arrs"), &bdevs));
	check_failed(EIO, ext4_partition_scan(disk(".bad-hdrs"), &bdevs));
	check_failed(EIO, ext4_gpt_scan(disk(".bad-hdrs"), &bdevs));
}

static void test_header_checks(void)
{
	static const char *const rejected[] = {
	    ".rev",	    ".hsize-small", ".hsize-big",   ".mylba",
	    ".esize-small", ".esize-odd",   ".nentries",    ".elba-mbr",
	    ".elba-hdr",    ".elba-usable", ".elba-disk",   ".elba-end",
	    ".usable-start", ".usable-order", ".usable-end", ".arrcrc",
	};
	size_t i;

	check_small("", 0);
	for (i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++) {
		fprintf(stderr, "variant %s\n", rejected[i]);
		check_small_backup(rejected[i]);
	}

	/* Valid: a newer minor revision, a header size of a whole sector
	 * (its CRC covers all 512 bytes), a damaged backup, bigger entries */
	check_small(".rev-minor", 0);
	check_small(".hsize-sector", 0);
	check_small(".backup-bad", 0);
	check_small(".esize256", 0);
	check_small(".esize1024", 0);

	/* Entry array CRC field wrong in both headers */
	check_failed(EIO, ext4_gpt_scan(disk(".s.arrcrc-both"), &bdevs));

	check_small_entry_skipped(".entry-end");
	check_small_entry_skipped(".entry-start");
	check_small_entry_skipped(".entry-order");
}

static void test_many(void)
{
	uint32_t i;

	TEST_ASSERT_EQ(EOK, ext4_partition_scan(disk(".many"), &bdevs));
	TEST_ASSERT_EQ(CONFIG_EXT4_PARTITIONS_COUNT, bdevs.count);
	TEST_ASSERT_EQ(20, bdevs.total);
	for (i = 0; i < bdevs.count; i++) {
		TEST_ASSERT_EQ(i + 1, bdevs.info[i].number);
		TEST_ASSERT_EQ(40 + 8 * i, bdevs.info[i].first_lba);
		TEST_ASSERT_EQ((40 + 8 * i) * 512,
			       bdevs.partitions[i].part_offset);
	}
}

static void test_not_gpt(void)
{
	/* Protective MBR record removed: an (empty) MBR */
	check_failed(ENOENT, ext4_gpt_scan(disk(".no-pmbr"), &bdevs));
	TEST_ASSERT_EQ(EOK, ext4_partition_scan(disk(".no-pmbr"), &bdevs));
	TEST_ASSERT_EQ(EXT4_PART_TABLE_MBR, bdevs.table);
	TEST_ASSERT_EQ(0, bdevs.count);

	/* No partition table at all */
	check_failed(ENOENT, ext4_gpt_scan(disk(".blank"), &bdevs));
	check_failed(ENOENT, ext4_partition_scan(disk(".blank"), &bdevs));
	check_failed(ENOENT, ext4_mbr_scan_all(disk(".blank"), &bdevs));

	/* Hybrid MBR: the GPT wins, the MBR view shows both records */
	TEST_ASSERT_EQ(EOK, ext4_partition_scan(disk(".hybrid"), &bdevs));
	TEST_ASSERT_EQ(EXT4_PART_TABLE_GPT, bdevs.table);
	TEST_ASSERT_EQ(6, bdevs.count);
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan_all(disk(".hybrid"), &bdevs));
	TEST_ASSERT_EQ(EXT4_PART_TABLE_MBR, bdevs.table);
	TEST_ASSERT_EQ(2, bdevs.count);
	TEST_ASSERT_EQ(1, bdevs.info[0].number);
	TEST_ASSERT_EQ(0xEE, bdevs.info[0].mbr_type);
	TEST_ASSERT_EQ(1, bdevs.info[0].first_lba);
	TEST_ASSERT_EQ(2, bdevs.info[1].number);
	TEST_ASSERT_EQ(0x83, bdevs.info[1].mbr_type);
	TEST_ASSERT_EQ(2048, bdevs.info[1].first_lba);
	TEST_ASSERT_EQ(6143, bdevs.info[1].last_lba);
	TEST_ASSERT(ext4_part_is_linux(&bdevs.info[1]));
	TEST_ASSERT(!ext4_part_is_linux(&bdevs.info[0]));
	check_mount(&bdevs.partitions[1], "gpt root\n");
}

static void test_sector_sizes(void)
{
	snprintf(path, sizeof(path), "%s.4k", image);
	sector_dev_name = path;
	TEST_ASSERT_EQ(EOK, ext4_partition_scan(&sector_dev_4k, &bdevs));
	TEST_ASSERT_EQ(EXT4_PART_TABLE_GPT, bdevs.table);
	TEST_ASSERT_EQ(0, bdevs.flags);
	TEST_ASSERT_EQ(1, bdevs.count);
	TEST_ASSERT_EQ(256, bdevs.info[0].first_lba);
	TEST_ASSERT_EQ(1279, bdevs.info[0].last_lba);
	TEST_ASSERT(!strcmp("big", bdevs.info[0].name));
	TEST_ASSERT_EQ(256 * 4096, bdevs.partitions[0].part_offset);
	TEST_ASSERT_EQ(1024 * 4096, bdevs.partitions[0].part_size);
	check_mount(&bdevs.partitions[0], "gpt 4k\n");

	/* Records could cross sector boundaries: not supported */
	check_failed(ENOTSUP, ext4_partition_scan(&sector_dev_256, &bdevs));
}

int main(int argc, char **argv)
{
	image = test_image_arg(argc, argv);

	test_main_image();
	test_backup_fallback();
	test_header_checks();
	test_many();
	test_not_gpt();
	test_sector_sizes();

	printf("test_gpt_scan: ok\n");
	return 0;
}
