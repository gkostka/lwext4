/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Large and fragmented files, truncated step by step across the block
 * mapping boundaries:
 *
 *   - ext2 (1 KiB blocks, block maps): a file reaching into the double
 *     indirect blocks, cut back into the single indirect and the direct
 *     blocks;
 *   - ext4 (1 KiB blocks, extents): two files written block by block in
 *     turn, so every block of each is its own extent. Hundreds of extents
 *     need an extent tree of depth 2 (leaf splits, index splits, growing
 *     the tree in depth); truncating removes leaves and index entries again.
 *
 * After every step the remaining contents are read back. The check script
 * runs e2fsck on both images and compares sizes with debugfs.
 *
 * red-green: guard (coverage test, passes on the base)
 */
#include "test_util.h"

#include <string.h>

#define BS 1024

static uint8_t buf[BS];

static void fill(uint32_t file, uint32_t blk)
{
	for (uint32_t i = 0; i < BS; i += 4) {
		uint32_t v = file * 0x1000000u + blk * 256u + i / 4;

		/* Little endian on every host, for the check script */
		buf[i] = (uint8_t)v;
		buf[i + 1] = (uint8_t)(v >> 8);
		buf[i + 2] = (uint8_t)(v >> 16);
		buf[i + 3] = (uint8_t)(v >> 24);
	}
}

static void write_blocks(ext4_file *f, uint32_t file, uint32_t from,
			 uint32_t to)
{
	size_t n;

	for (uint32_t b = from; b < to; b++) {
		fill(file, b);
		TEST_ASSERT_EQ(EOK, ext4_fwrite(f, buf, BS, &n));
		TEST_ASSERT_EQ(BS, n);
	}
}

static void verify(const char *path, uint32_t file, uint64_t size)
{
	ext4_file f;
	uint8_t rd[BS];
	size_t n;
	uint32_t blocks = (uint32_t)((size + BS - 1) / BS);

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(size, ext4_fsize(&f));
	for (uint32_t b = 0; b < blocks; b++) {
		size_t want = b + 1 < blocks ? BS : size - (uint64_t)b * BS;

		fill(file, b);
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, rd, BS, &n));
		TEST_ASSERT_EQ(want, n);
		if (memcmp(rd, buf, want)) {
			fprintf(stderr, "%s: block %u differs\n", path, b);
			exit(1);
		}
	}
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, rd, BS, &n));
	TEST_ASSERT_EQ(0, n);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void truncate_to(const char *path, uint64_t size)
{
	ext4_file f;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "r+b"));
	TEST_ASSERT_EQ(EOK, ext4_ftruncate(&f, size));
	TEST_ASSERT_EQ(size, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

/* Depth of the extent tree of @path: the extent header in i_block holds
 * it (little endian) at byte 6. */
static int extent_depth(const char *path)
{
	uint32_t ino;
	struct ext4_inode inode;
	const uint8_t *h = (const uint8_t *)inode.blocks;

	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
	TEST_ASSERT(h[0] == 0x0a && h[1] == 0xf3); /* extent magic */
	return h[6] | h[7] << 8;
}

static uint64_t free_blocks(void)
{
	struct ext4_mount_stats st;

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &st));
	return st.free_blocks_count;
}

/* ext2: 12 direct blocks, 256 per indirect block. */
static void block_maps(const char *image)
{
	/* 12 + 256 + 2 * 256 blocks: into the double indirect tree */
	const uint32_t blocks = 12 + 256 + 2 * 256 + 7;
	const uint64_t steps[] = {
		(uint64_t)blocks * BS - 100,	/* partial last block */
		(12 + 256 + 256 + 3) * BS,	/* second dind subtree */
		(12 + 256 + 1) * BS + 1,	/* first dind block */
		(12 + 200) * BS,		/* single indirect */
		12 * BS + 5,			/* first indirect block */
		12 * BS,			/* direct blocks only */
		3 * BS + 17,
		0,
	};
	ext4_file f;
	uint64_t before;

	printf("== block maps: %s\n", image);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	before = free_blocks();
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "wb"));
	write_blocks(&f, 1, 0, blocks);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	verify(TEST_MP "big", 1, (uint64_t)blocks * BS);
	/* Data plus indirect, double indirect and 3 second level blocks */
	TEST_ASSERT_EQ(before - blocks - 1 - 1 - 3, free_blocks());

	/* The indirect blocks are only released with the fix
	 * (test_truncate_indirect checks the block counts). */
	for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
		truncate_to(TEST_MP "big", steps[i]);
		verify(TEST_MP "big", 1, steps[i]);
	}

	/* Grow the file again after the truncation, reusing freed blocks. */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "wb"));
	write_blocks(&f, 1, 0, 300);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	verify(TEST_MP "big", 1, 300 * BS);
	test_umount();
}

/* ext4: interleaved single block writes, one extent per block. */
static void extents(const char *image)
{
	const uint32_t blocks = 700;
	const uint64_t steps[] = {
		650 * BS + 1, 500 * BS, 340 * BS - 3, 100 * BS, 84 * BS,
		5 * BS, 4 * BS, 1, 0,
	};
	ext4_file a, b;
	uint64_t before;

	printf("== extents: %s\n", image);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	before = free_blocks();
	TEST_ASSERT_EQ(EOK, ext4_fopen(&a, TEST_MP "a", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&b, TEST_MP "b", "wb"));
	for (uint32_t i = 0; i < blocks; i++) {
		write_blocks(&a, 1, i, i + 1);
		write_blocks(&b, 2, i, i + 1);
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&a));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&b));
	verify(TEST_MP "a", 1, (uint64_t)blocks * BS);
	verify(TEST_MP "b", 2, (uint64_t)blocks * BS);
	TEST_ASSERT_EQ(2, extent_depth(TEST_MP "a"));
	TEST_ASSERT_EQ(2, extent_depth(TEST_MP "b"));
	test_umount();

	/* After a remount, from disk. */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	verify(TEST_MP "a", 1, (uint64_t)blocks * BS);
	for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
		truncate_to(TEST_MP "a", steps[i]);
		verify(TEST_MP "a", 1, steps[i]);
	}
	verify(TEST_MP "b", 2, (uint64_t)blocks * BS);
	/* The other file goes in one piece, with its whole tree. */
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "b"));
	TEST_ASSERT_EQ(before, free_blocks());

	/* Extending the truncated file appends to the existing extent. */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&a, TEST_MP "a", "wb"));
	write_blocks(&a, 1, 0, 50);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&a));
	verify(TEST_MP "a", 1, 50 * BS);

	/* Truncating to a larger size does not change the file. */
	truncate_to(TEST_MP "a", 50 * BS);
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char ext2[1024];

	snprintf(ext2, sizeof(ext2), "%s.ext2", image);
	block_maps(ext2);
	extents(image);
	return 0;
}
