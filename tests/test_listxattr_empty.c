/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_listxattr() only stored the list size in *ret_size when the inode
 * had at least one attribute. For an inode without attributes it returned
 * EOK and left *ret_size untouched, so callers read whatever was in
 * their variable (usually uninitialised) as the size of the list. It must
 * report size 0.
 */

#include "test_util.h"

#include <string.h>

static void expect_empty(const char *path)
{
	char list[64];
	size_t size = 12345;

	TEST_ASSERT_EQ(EOK, ext4_listxattr(path, NULL, 0, &size));
	TEST_ASSERT_EQ(0, size);
	size = 12345;
	TEST_ASSERT_EQ(EOK, ext4_listxattr(path, list, sizeof(list), &size));
	TEST_ASSERT_EQ(0, size);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;
	size_t size = 0;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "f", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	expect_empty(TEST_MP "f");
	expect_empty(TEST_MP);

	/* With an attribute the size is still reported */
	TEST_ASSERT_EQ(EOK, ext4_setxattr(TEST_MP "f", "user.a", 6, "v", 1));
	TEST_ASSERT_EQ(EOK, ext4_listxattr(TEST_MP "f", NULL, 0, &size));
	TEST_ASSERT_EQ(sizeof("user.a"), size);
	test_umount();
	return 0;
}
