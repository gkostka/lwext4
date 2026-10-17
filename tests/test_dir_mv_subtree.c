/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Renaming a directory to a name inside its own subtree (mv a a/b/x)
 * succeeded: a was unlinked from its parent and linked into its own
 * descendant, so the whole subtree was cut off from the root (a/b/x is a,
 * and the ".." of a is b: a loop nothing leads to). It must fail with
 * EINVAL (as rename(2) does) and leave everything in place, while moves
 * that leave the subtree (up, or to a sibling) keep working.
 */

#include "test_util.h"

#include <string.h>

static void expect_dir(const char *path)
{
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(path, EXT4_DE_DIR));
}

static void expect_none(const char *path)
{
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(path, EXT4_DE_DIR));
}

static void check(void)
{
	expect_dir(TEST_MP "a");
	expect_dir(TEST_MP "a/b");
	expect_dir(TEST_MP "a/b/c");
	expect_dir(TEST_MP "c2");
	expect_dir(TEST_MP "a/s2");
	expect_none(TEST_MP "a/x");
	expect_none(TEST_MP "a/b/x");
	expect_none(TEST_MP "a/b/c/x");
	expect_none(TEST_MP "a/b/b");
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "a/b/c/d"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "s"));

	/* Into itself, at every depth */
	TEST_ASSERT_EQ(EINVAL, ext4_dir_mv(TEST_MP "a", TEST_MP "a/x"));
	TEST_ASSERT_EQ(EINVAL, ext4_dir_mv(TEST_MP "a", TEST_MP "a/b/x"));
	TEST_ASSERT_EQ(EINVAL, ext4_dir_mv(TEST_MP "a", TEST_MP "a/b/c/x"));
	TEST_ASSERT_EQ(EINVAL, ext4_dir_mv(TEST_MP "a/b", TEST_MP "a/b/b"));
	TEST_ASSERT_EQ(EINVAL, ext4_frename(TEST_MP "a/b", TEST_MP "a/b/c/x"));

	/* Out of the subtree and across */
	TEST_ASSERT_EQ(EOK, ext4_dir_mv(TEST_MP "a/b/c/d", TEST_MP "c2"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mv(TEST_MP "s", TEST_MP "a/s2"));
	check();
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check();
	test_umount();
	return 0;
}
