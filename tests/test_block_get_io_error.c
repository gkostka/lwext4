/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Getting a block into a full block cache first writes out the least
 * recently used dirty block (write-back mode). When that write failed,
 * ext4_block_get_noread() returned the error with the struct ext4_block
 * already naming the block but not holding a buffer. Callers that release
 * a block they got when b->lb_id is set, such as the extent tree code
 * reading a tree block (read_extent_tree_block()), then released a buffer
 * they never had: an assertion in ext4_block_set(), and with assertions
 * off a buffer pointer taken from uninitialised memory.
 *
 * Here: reading a file whose extent tree has a leaf block, with the cache
 * full of dirty blocks, and the device failing every write from the kth
 * on, for every k until the read no longer writes that many blocks. Each
 * read must succeed or fail with EIO, and the data must be intact once the
 * device works again.
 */

#include "fault_dev.h"

#include <string.h>

#define FILES 64
#define READ_SIZE 8192

static void copy_file(const char *from, const char *to)
{
	static char buf[65536];
	FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
	size_t n;

	TEST_ASSERT(in && out);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
	fclose(in);
	TEST_ASSERT_EQ(0, fclose(out));
}

/* "sparse" has "block <n>\n" at the start of every even block. */
static void check_data(const char *buf)
{
	/* READ_SIZE bytes from offset 2048: blocks 2 to 9. */
	for (int blk = 2; blk < 10; blk += 2) {
		char expect[16];

		snprintf(expect, sizeof(expect), "block %d\n", blk);
		TEST_ASSERT(memcmp(buf + (blk - 2) * 1024, expect,
				   strlen(expect)) == 0);
	}
}

/* Returns false once the read wrote fewer than k blocks. */
static bool read_failing_from(const char *image, const char *orig,
			      uint64_t k)
{
	static char buf[READ_SIZE];
	ext4_file f, g;
	char path[32];
	size_t cnt;
	bool failed;
	int r;

	copy_file(orig, image);
	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));

	/* Dirty more blocks than the cache holds (inode table, bitmaps,
	 * directory blocks) and keep them there. */
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, true));
	for (int i = 0; i < FILES; i++) {
		snprintf(path, sizeof(path), TEST_MP "new%d", i);
		TEST_ASSERT_EQ(EOK, ext4_fopen(&g, path, "wb"));
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&g, "x", 1, &cnt));
		TEST_ASSERT_EQ(EOK, ext4_fclose(&g));
	}
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "sparse", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, 2048, SEEK_SET));

	fault_dev_fail_nth(FAULT_DEV_WRITE, k, 0);
	r = ext4_fread(&f, buf, READ_SIZE, &cnt);
	failed = fault_dev_failed() > 0;
	fault_dev_disarm();
	if (failed) {
		TEST_ASSERT_EQ(EIO, r);
	} else {
		TEST_ASSERT_EQ(EOK, r);
		TEST_ASSERT_EQ(READ_SIZE, cnt);
		check_data(buf);
	}

	/* The device works again. */
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, 2048, SEEK_SET));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, READ_SIZE, &cnt));
	TEST_ASSERT_EQ(READ_SIZE, cnt);
	check_data(buf);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, false));
	test_umount();
	return failed;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char orig[512];
	uint64_t k = 1;

	snprintf(orig, sizeof(orig), "%s.orig", image);
	while (read_failing_from(image, orig, k))
		k++;
	/* At least one write happened during the read, so it was tested. */
	TEST_ASSERT(k > 1);
	return 0;
}
