/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * lwext4 built without its GPLv2 files (see test_xattr_disabled.cmake):
 * ext4_extent.c and ext4_xattr.c removed, CONFIG_EXTENTS_ENABLE=0 and
 * CONFIG_XATTR_ENABLE=0. ext4.c used to call the xattr implementation
 * regardless of CONFIG_XATTR_ENABLE, so nothing using the library linked.
 * The library has to link, work on an ext2 image, and the xattr API has to
 * report ENOTSUP.
 */

#include "test_util.h"

#include <string.h>

static void check_file(const char *path, const char *want)
{
	ext4_file f;
	char buf[64];
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(strlen(want), rcnt);
	TEST_ASSERT(memcmp(buf, want, rcnt) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	const char *path = TEST_MP "d/file";
	ext4_file f;
	char buf[16];
	size_t len;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d"));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "no gpl", 6, &len));
	TEST_ASSERT_EQ(6, len);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	TEST_ASSERT_EQ(ENOTSUP, ext4_setxattr(path, "user.a", 6, "v", 1));
	TEST_ASSERT_EQ(ENOTSUP, ext4_getxattr(path, "user.a", 6, buf,
					      sizeof(buf), &len));
	TEST_ASSERT_EQ(ENOTSUP, ext4_listxattr(path, buf, sizeof(buf), &len));
	TEST_ASSERT_EQ(ENOTSUP, ext4_removexattr(path, "user.a", 6));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check_file(path, "no gpl");
	test_umount();
	return 0;
}
