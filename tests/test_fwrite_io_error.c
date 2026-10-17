/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_fwrite() of whole blocks over existing data when the block device
 * fails to read the block map (here the indirect block of a block mapped
 * file). ext4_fwrite() writes whole blocks in the block cache's write-back
 * mode and, when looking up a block failed, returned the error without
 * leaving that mode: the cache stayed in write-back mode for good, later
 * changes only reached the device at ext4_umount() or ext4_cache_flush(),
 * and a power cut lost them.
 */

#include "fault_dev.h"

#include <string.h>

#define OFF 16384
#define LEN 4096

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

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static char data[LEN], back[LEN];
	ext4_file f;
	size_t cnt;

	memset(data, 'z', sizeof(data));
	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_setup_locks(TEST_MP, &counting_locks));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "file", "r+"));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, OFF, SEEK_SET));

	/* Every read fails from now on: the indirect block is not cached. */
	fault_dev_fail_nth(FAULT_DEV_READ, 1, 0);
	/* Not checked here: whether the error is returned (without
	 * gkostka/lwext4#150 ext4_fwrite() returns EOK after any error). */
	ext4_fwrite(&f, data, LEN, &cnt);
	TEST_ASSERT(fault_dev_failed() > 0);
	TEST_ASSERT_EQ(0, lock_depth);
	TEST_ASSERT_EQ(0, fault_dev.cache_write_back);

	/* The device works again: so does ext4_fwrite. */
	fault_dev_disarm();
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, OFF, SEEK_SET));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, data, LEN, &cnt));
	TEST_ASSERT_EQ(LEN, cnt);
	TEST_ASSERT_EQ(0, lock_depth);
	TEST_ASSERT_EQ(0, fault_dev.cache_write_back);
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, OFF, SEEK_SET));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, back, LEN, &cnt));
	TEST_ASSERT_EQ(LEN, cnt);
	TEST_ASSERT(memcmp(back, data, LEN) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	test_umount();
	return 0;
}
