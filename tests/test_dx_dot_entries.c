/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * In a directory with an htree index, ext4_dir_find_entry() looked up "."
 * and ".." through the index only. They are not in the hash tree (they are
 * the first two entries of the index root block), so the lookup failed:
 * paths through "dir/.." did not resolve, and opening "dir/../name" for
 * writing created a new directory called ".." in dir (likewise for ".";
 * e2fsck: "duplicate '..' entry") and the file inside it instead of next
 * to dir. lwext4 gives every directory it creates an index when the
 * filesystem has dir_index.
 */

#include "test_util.h"

#include <string.h>

static int count_entries(const char *path, const char *name)
{
	ext4_dir d;
	const ext4_direntry *de;
	int n = 0;

	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, path));
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		if (!name || (de->name_length == strlen(name) &&
			      !memcmp(de->name, name, de->name_length)))
			n++;
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));
	return n;
}

static void create(const char *path)
{
	ext4_file f;
	int r = ext4_fopen(&f, path, "wb");

	if (r != EOK)
		fprintf(stderr, "create %s: %d\n", path, r);
	TEST_ASSERT_EQ(EOK, r);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void check(void)
{
	ext4_dir d;

	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "d/..", EXT4_DE_DIR));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "d/.", EXT4_DE_DIR));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "d/e/../..",
					     EXT4_DE_DIR));
	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, TEST_MP "d/e/.."));
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));

	/* Created next to d, and in d through "." */
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "up",
					     EXT4_DE_REG_FILE));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "d/here",
					     EXT4_DE_REG_FILE));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "d/e/../up2",
					     EXT4_DE_REG_FILE));

	/* d: ".", "..", e, here, up2 and no second ".." */
	TEST_ASSERT_EQ(1, count_entries(TEST_MP "d", ".."));
	TEST_ASSERT_EQ(1, count_entries(TEST_MP "d", "."));
	TEST_ASSERT_EQ(5, count_entries(TEST_MP "d", NULL));
	TEST_ASSERT_EQ(1, count_entries(TEST_MP "d/e", ".."));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d/e"));
	create(TEST_MP "d/../up");
	create(TEST_MP "d/./here");
	create(TEST_MP "d/e/../up2");
	check();
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check();
	test_umount();
	return 0;
}
