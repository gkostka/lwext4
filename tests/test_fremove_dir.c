/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_fremove() on a directory did nothing and returned EOK, so callers
 * believed the directory was gone. Directories are removed with
 * ext4_dir_rm(); ext4_fremove() must refuse them with EISDIR (as
 * unlink(2) does on Linux) and leave them alone.
 */

#include "test_util.h"

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "empty"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "full"));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "full/f", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	TEST_ASSERT_EQ(EISDIR, ext4_fremove(TEST_MP "empty"));
	TEST_ASSERT_EQ(EISDIR, ext4_fremove(TEST_MP "full"));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "empty", EXT4_DE_DIR));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "full/f",
					     EXT4_DE_REG_FILE));

	/* Files still go, and the directories with ext4_dir_rm */
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "full/f"));
	TEST_ASSERT_EQ(EOK, ext4_dir_rm(TEST_MP "full"));
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "full", EXT4_DE_DIR));
	test_umount();
	return 0;
}
