/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Removing an extended attribute stored in the inode body (256 byte inodes)
 * went through the xattr block search context, which is not initialised in
 * that case: undefined behaviour, in practice the mount point table got
 * corrupted and every following path lookup failed with ENOENT. Removing an
 * attribute that does not exist at all must fail with ENODATA.
 */

#include "test_util.h"

#include <string.h>

#define F TEST_MP "f"

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;
	ext4_dir d;
	char buf[64];
	size_t len;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, F, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	TEST_ASSERT_EQ(EOK, ext4_setxattr(F, "user.keep", 9, "hello", 5));
	TEST_ASSERT_EQ(EOK, ext4_setxattr(F, "user.gone", 9, "world", 5));
	TEST_ASSERT_EQ(EOK, ext4_removexattr(F, "user.gone", 9));
	TEST_ASSERT_EQ(ENODATA, ext4_removexattr(F, "user.never", 10));

	TEST_ASSERT_EQ(ENODATA, ext4_getxattr(F, "user.gone", 9, buf,
					      sizeof(buf), &len));
	TEST_ASSERT_EQ(EOK, ext4_getxattr(F, "user.keep", 9, buf, sizeof(buf),
					  &len));
	TEST_ASSERT_EQ(5, len);
	TEST_ASSERT(memcmp(buf, "hello", 5) == 0);
	/* The remaining in-inode entries are intact: another one fits in */
	TEST_ASSERT_EQ(EOK, ext4_setxattr(F, "user.new", 8, "again", 5));
	TEST_ASSERT_EQ(EOK, ext4_getxattr(F, "user.new", 8, buf, sizeof(buf),
					  &len));
	TEST_ASSERT_EQ(5, len);
	TEST_ASSERT(memcmp(buf, "again", 5) == 0);
	TEST_ASSERT_EQ(EOK, ext4_getxattr(F, "user.keep", 9, buf, sizeof(buf),
					  &len));
	TEST_ASSERT_EQ(5, len);

	/* The mount point is still intact */
	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "dir"));
	test_umount();

	/* And the removal reached the disk */
	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	TEST_ASSERT_EQ(ENODATA, ext4_getxattr(F, "user.gone", 9, buf,
					      sizeof(buf), &len));
	TEST_ASSERT_EQ(EOK, ext4_getxattr(F, "user.keep", 9, buf, sizeof(buf),
					  &len));
	TEST_ASSERT_EQ(5, len);
	TEST_ASSERT_EQ(EOK, ext4_getxattr(F, "user.new", 8, buf, sizeof(buf),
					  &len));
	TEST_ASSERT_EQ(5, len);
	test_umount();
	return 0;
}
