/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Truncating and removing extent mapped files when the block bitmap
 * cannot be read, so the blocks cannot be released. The extent code
 * ignored the error of ext4_balloc_free_blocks(): ext4_ftruncate() and
 * ext4_fremove() reported success, the extents were gone, and the blocks
 * stayed allocated in the bitmap for good (lost space; e2fsck: "block
 * bitmap differences").
 *
 * The error must be reported, and once the device works again the same
 * call must release every block: the free block count is back to where it
 * was before the file had them.
 */

#include "fault_dev.h"

#include <string.h>

/* Block bitmap of group 0 (1 KiB blocks: the descriptors are in block 2). */
static uint32_t block_bitmap(const char *image)
{
	uint8_t b[4];
	FILE *f = fopen(image, "rb");

	TEST_ASSERT(f);
	TEST_ASSERT(fseek(f, 2048, SEEK_SET) == 0);
	TEST_ASSERT(fread(b, 1, 4, f) == 4);
	fclose(f);
	return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24;
}

static uint64_t free_blocks(void)
{
	struct ext4_mount_stats st;

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &st));
	return st.free_blocks_count;
}

/* Truncate path to size, first with the bitmap unreadable. Returns the
 * number of blocks released. */
static uint64_t truncate_failing(uint32_t bitmap, const char *path,
				 uint64_t size)
{
	uint64_t before = free_blocks();
	ext4_file f;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "r+"));
	fault_dev_fail_range(FAULT_DEV_READ, (uint64_t)bitmap * 1024, 1024);
	TEST_ASSERT_EQ(EIO, ext4_ftruncate(&f, size));
	TEST_ASSERT(fault_dev_failed() > 0);
	fault_dev_disarm();
	TEST_ASSERT_EQ(before, free_blocks());

	TEST_ASSERT_EQ(EOK, ext4_ftruncate(&f, size));
	TEST_ASSERT_EQ(size, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	return free_blocks() - before;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	uint32_t bitmap = block_bitmap(image);
	uint64_t before;

	/* One extent in the inode: 64 blocks, 1 stays. */
	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(63, truncate_failing(bitmap, TEST_MP "file", 1000));
	test_umount();

	/* 32 extents in a leaf block: 32 blocks, the leaf block is released
	 * with the last extent. */
	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(33, truncate_failing(bitmap, TEST_MP "sparse", 0));
	test_umount();

	/* ext4_fremove() releases the blocks the same way. */
	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	before = free_blocks();
	fault_dev_fail_range(FAULT_DEV_READ, (uint64_t)bitmap * 1024, 1024);
	TEST_ASSERT(ext4_fremove(TEST_MP "file") != EOK);
	TEST_ASSERT(fault_dev_failed() > 0);
	fault_dev_disarm();
	TEST_ASSERT_EQ(before, free_blocks());
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "file"));
	TEST_ASSERT_EQ(before + 1, free_blocks());
	test_umount();
	return 0;
}
