/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Truncating a file that uses block maps (ext2/ext3, no extents) released
 * the data blocks past the new end, but never the indirect blocks that
 * mapped them: single, double and second level indirect blocks stayed
 * allocated and attached to the inode, even after truncating to size 0,
 * until the file was deleted. With 1 KiB blocks (256 entries per indirect
 * block) a file is written into its double indirect tree and cut back step
 * by step; after every step the free block count must be exactly the data
 * plus the indirect blocks the remaining size needs, the data must read
 * back, and the file must be able to grow again.
 */

#include "test_util.h"

#include <ext4_inode.h>

#include <string.h>

#define BS 1024
#define PER 256 /* block numbers per indirect block */

static uint8_t buf[BS];

static void fill(uint32_t blk)
{
	for (uint32_t i = 0; i < BS; i++)
		buf[i] = (uint8_t)(blk * 7 + i);
}

static void write_blocks(ext4_file *f, uint32_t from, uint32_t to)
{
	size_t n;

	for (uint32_t b = from; b < to; b++) {
		fill(b);
		TEST_ASSERT_EQ(EOK, ext4_fwrite(f, buf, BS, &n));
		TEST_ASSERT_EQ(BS, n);
	}
}

static void verify(uint64_t size)
{
	ext4_file f;
	uint8_t rd[BS];
	size_t n;
	uint32_t blocks = (uint32_t)((size + BS - 1) / BS);

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "rb"));
	TEST_ASSERT_EQ(size, ext4_fsize(&f));
	for (uint32_t b = 0; b < blocks; b++) {
		size_t want = b + 1 < blocks ? BS : size - (uint64_t)b * BS;

		fill(b);
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, rd, BS, &n));
		TEST_ASSERT_EQ(want, n);
		TEST_ASSERT(memcmp(rd, buf, want) == 0);
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

/* Data blocks plus the indirect blocks that map them */
static uint64_t blocks_for(uint64_t size)
{
	uint64_t n = (size + BS - 1) / BS;
	uint64_t total = n;

	if (n > 12)
		total += 1;
	if (n > 12 + PER)
		total += 1 + (n - 12 - PER + PER - 1) / PER;
	return total;
}

static uint64_t free_blocks(void)
{
	struct ext4_mount_stats st;

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &st));
	return st.free_blocks_count;
}

static void check_usage(uint64_t before, uint64_t size)
{
	uint32_t ino;
	struct ext4_inode inode;
	struct ext4_sblock *sb;
	uint64_t used = before - free_blocks();

	if (used != blocks_for(size))
		fprintf(stderr, "size %llu: %llu blocks in use, expected %llu\n",
			(unsigned long long)size, (unsigned long long)used,
			(unsigned long long)blocks_for(size));
	TEST_ASSERT_EQ(blocks_for(size), used);

	/* i_blocks counts 512 byte sectors */
	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "big", &ino, &inode));
	TEST_ASSERT_EQ(blocks_for(size) * (BS / 512),
		       ext4_inode_get_blocks_count(sb, &inode));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	const uint32_t blocks = 12 + PER + 2 * PER + 7;
	const uint64_t steps[] = {
		(uint64_t)blocks * BS - 100,	/* partial last block */
		(12 + PER + PER + 3) * BS,	/* 2nd level block 2 goes */
		(12 + PER + 1) * BS + 1,	/* in 2nd level block 0 */
		(12 + PER) * BS,		/* double indirect goes */
		(12 + 200) * BS,		/* in the single indirect */
		12 * BS + 5,			/* its first entry */
		12 * BS,			/* single indirect goes */
		3 * BS + 17,
		0,
	};
	ext4_file f;
	uint64_t before;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	before = free_blocks();

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "wb"));
	write_blocks(&f, 0, blocks);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	verify((uint64_t)blocks * BS);
	check_usage(before, (uint64_t)blocks * BS);

	for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "r+b"));
		TEST_ASSERT_EQ(EOK, ext4_ftruncate(&f, steps[i]));
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
		verify(steps[i]);
		check_usage(before, steps[i]);
	}
	test_umount();

	/* From disk, and growing again into the indirect blocks */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	check_usage(before, 0);
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "wb"));
	write_blocks(&f, 0, 12 + PER + 10);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	verify((12 + PER + 10) * BS);
	check_usage(before, (12 + PER + 10) * BS);
	test_umount();
	return 0;
}
