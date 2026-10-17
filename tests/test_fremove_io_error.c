/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_fremove() of a file whose blocks cannot be released because the
 * block device fails to read the block bitmap. ext4_fremove() enters the
 * block cache's write-back mode to truncate the file and returned the
 * error without leaving it, so the cache stayed in write-back mode for
 * good: later changes only reached the device at ext4_umount() or
 * ext4_cache_flush(), and a power cut lost them.
 */

#include "fault_dev.h"

static int lock_depth;

static void count_lock(void)
{
	lock_depth++;
}

static void count_unlock(void)
{
	lock_depth--;
}

static const struct ext4_lock counting_locks = {
	.lock = count_lock,
	.unlock = count_unlock,
};

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

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	uint32_t bitmap = block_bitmap(image);

	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_setup_locks(TEST_MP, &counting_locks));

	fault_dev_fail_range(FAULT_DEV_READ, (uint64_t)bitmap * 1024, 1024);
	TEST_ASSERT_EQ(EIO, ext4_fremove(TEST_MP "file"));
	TEST_ASSERT(fault_dev_failed() > 0);
	TEST_ASSERT_EQ(0, lock_depth);
	TEST_ASSERT_EQ(0, fault_dev.cache_write_back);

	/* The device works again: so does ext4_fremove. */
	fault_dev_disarm();
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "file"));
	TEST_ASSERT_EQ(0, lock_depth);
	TEST_ASSERT_EQ(0, fault_dev.cache_write_back);
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "file",
						EXT4_DE_REG_FILE));

	test_umount();
	return 0;
}
