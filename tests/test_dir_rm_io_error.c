/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_dir_rm() of a directory tree while the block device fails every
 * write from the kth on, for every k until ext4_dir_rm() writes fewer
 * blocks. ext4_dir_rm() works in write-back mode, so the writes that fail
 * are the ones that make room in a full block cache, when a block is got.
 * When getting the removed directory's own inode for the last unlink
 * failed, ext4_dir_rm() released that inode reference, which it did not
 * hold (an assertion in ext4_bcache_free(), with assertions off a use of a
 * released buffer), and never released the parent directory's.
 *
 * Every attempt must fail cleanly: locks balanced, the cache out of
 * write-back mode, and the filesystem still mountable and usable.
 */

#include "fault_dev.h"

static int lock_depth, unlock_underflow;

static void count_lock(void)
{
	lock_depth++;
}

static void count_unlock(void)
{
	if (--lock_depth < 0)
		unlock_underflow++;
}

static const struct ext4_lock counting_locks = {
	.lock = count_lock,
	.unlock = count_unlock,
};

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

/* Returns false once ext4_dir_rm() wrote fewer than k blocks. */
static bool rm_failing_from(const char *image, const char *orig, uint64_t k)
{
	ext4_file f;
	bool failed;
	int r;

	copy_file(orig, image);
	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_setup_locks(TEST_MP, &counting_locks));
	lock_depth = unlock_underflow = 0;

	fault_dev_fail_nth(FAULT_DEV_WRITE, k, 0);
	r = ext4_dir_rm(TEST_MP "d");
	failed = fault_dev_failed() > 0;
	fault_dev_disarm();

	TEST_ASSERT_EQ(0, unlock_underflow);
	TEST_ASSERT_EQ(0, lock_depth);
	TEST_ASSERT_EQ(0, fault_dev.cache_write_back);
	if (!failed) {
		TEST_ASSERT_EQ(EOK, r);
		TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "d",
							EXT4_DE_DIR));
	}

	/* The device works again: the filesystem is usable. */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "new", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_umount(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_device_unregister(TEST_DEV));
	return failed;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char orig[512];
	uint64_t k = 1;

	snprintf(orig, sizeof(orig), "%s.orig", image);
	while (rm_failing_from(image, orig, k))
		k++;
	/* ext4_dir_rm() did write, so the error paths were exercised. */
	TEST_ASSERT(k > 1);
	printf("ext4_dir_rm() with writes failing from 1 to %llu\n",
	       (unsigned long long)(k - 1));
	return 0;
}
