/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Extent trees with full index blocks. Two files are written block by
 * block in turn, so each block is an extent of its own. With 1 KiB blocks
 * a tree block holds 84 entries and the inode 4: after 4 * 84 extents the
 * tree has two levels below the inode, and after 84 leaves an index block
 * is full and has to be split (ext4_ext_split_node() at an index level).
 * test_truncate stops before that. The files are read back, truncated
 * back step by step through the index levels (removing index entries and
 * whole subtrees) and removed; the free block count must be back where it
 * started, and the check script runs e2fsck.
 *
 * red-green: guard (coverage test, passes on the base)
 */
#include "test_util.h"

#include <string.h>

#define BS 1024
#define BLOCKS 7200 /* per file: 86 full leaves of 84 extents */

static uint8_t buf[BS];

static void fill(uint32_t file, uint32_t blk)
{
	for (uint32_t i = 0; i < BS; i += 4) {
		uint32_t v = file << 28 | blk << 8 | i / 4;

		memcpy(buf + i, &v, 4);
	}
}

static void verify(const char *path, uint32_t file, uint32_t blocks)
{
	uint8_t rd[BS];
	ext4_file f;
	size_t n;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ((uint64_t)blocks * BS, ext4_fsize(&f));
	for (uint32_t b = 0; b < blocks; b++) {
		fill(file, b);
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, rd, BS, &n));
		TEST_ASSERT_EQ(BS, n);
		if (memcmp(rd, buf, BS)) {
			fprintf(stderr, "%s: block %u differs\n", path, b);
			exit(1);
		}
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static int extent_depth(const char *path)
{
	struct ext4_inode inode;
	const uint8_t *h = (const uint8_t *)inode.blocks;
	uint32_t ino;

	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
	TEST_ASSERT(h[0] == 0x0a && h[1] == 0xf3);
	return h[6] | h[7] << 8;
}

static uint64_t free_blocks(void)
{
	struct ext4_mount_stats st;

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &st));
	return st.free_blocks_count;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static const uint32_t steps[] = {7000, 5000, 3000, 400, 300, 90, 4, 1, 0};
	ext4_file a, b;
	uint64_t before;
	size_t n;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	before = free_blocks();
	TEST_ASSERT_EQ(EOK, ext4_fopen(&a, TEST_MP "a", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&b, TEST_MP "b", "wb"));
	for (uint32_t blk = 0; blk < BLOCKS; blk++) {
		fill(1, blk);
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&a, buf, BS, &n));
		TEST_ASSERT_EQ(BS, n);
		fill(2, blk);
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&b, buf, BS, &n));
		TEST_ASSERT_EQ(BS, n);
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&a));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&b));
	TEST_ASSERT_EQ(2, extent_depth(TEST_MP "a"));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	verify(TEST_MP "a", 1, BLOCKS);
	verify(TEST_MP "b", 2, BLOCKS);
	for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
		TEST_ASSERT_EQ(EOK, ext4_fopen(&a, TEST_MP "a", "r+b"));
		TEST_ASSERT_EQ(EOK, ext4_ftruncate(&a, (uint64_t)steps[i] * BS));
		TEST_ASSERT_EQ(EOK, ext4_fclose(&a));
		verify(TEST_MP "a", 1, steps[i]);
	}
	/* Empty: the tree is back in the inode */
	TEST_ASSERT_EQ(0, extent_depth(TEST_MP "a"));
	verify(TEST_MP "b", 2, BLOCKS);
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "a"));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "b"));
	TEST_ASSERT_EQ(before, free_blocks());
	test_umount();
	return 0;
}
