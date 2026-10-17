/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_dir_rm() removes a tree by truncating and freeing every inode in
 * it. A file in the tree that also has a name outside of it (a hard link)
 * was freed as well: the outside name then pointed to a free inode with
 * link count 0 (e2fsck: "has deleted/unused inode"). A file with two names
 * inside the tree was freed twice, which made the free inode count too
 * high. Such files must only lose their names inside the tree.
 */

#include "test_util.h"

#include <ext4_inode.h>

#include <string.h>

static const char data[] = "linked from outside";

static uint32_t links(const char *path, uint32_t *ino)
{
	struct ext4_inode inode;

	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, ino, &inode));
	return ext4_inode_get_links_cnt(&inode);
}

static void check(void)
{
	ext4_file f;
	char buf[64];
	size_t n;
	uint32_t ino;

	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "tree", EXT4_DE_DIR));
	TEST_ASSERT_EQ(1, links(TEST_MP "kept", &ino));
	TEST_ASSERT_EQ(2, links(TEST_MP "kept2", &ino));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "kept", "rb"));
	TEST_ASSERT_EQ(sizeof(data), ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &n));
	TEST_ASSERT_EQ(sizeof(data), n);
	TEST_ASSERT(memcmp(buf, data, n) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	struct ext4_mount_stats before, after;
	ext4_file f;
	size_t n;
	uint32_t kept_ino, new_ino;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "tree/sub"));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "tree/sub/f", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, data, sizeof(data), &n));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_flink(TEST_MP "tree/sub/f", TEST_MP "kept"));
	/* Two names inside the tree and two outside */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "tree/g", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_flink(TEST_MP "tree/g", TEST_MP "tree/sub/g"));
	TEST_ASSERT_EQ(EOK, ext4_flink(TEST_MP "tree/g", TEST_MP "kept2"));
	TEST_ASSERT_EQ(EOK, ext4_flink(TEST_MP "tree/g", TEST_MP "kept3"));
	TEST_ASSERT_EQ(4, links(TEST_MP "kept2", &new_ino));

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &before));
	TEST_ASSERT_EQ(EOK, ext4_dir_rm(TEST_MP "tree"));
	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &after));
	check();

	/* Only the two directories were freed, no file inode */
	TEST_ASSERT_EQ(before.free_inodes_count + 2, after.free_inodes_count);

	/* A new file does not get the inode of the kept one */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "new", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	links(TEST_MP "kept", &kept_ino);
	links(TEST_MP "new", &new_ino);
	TEST_ASSERT(kept_ino != new_ino);
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check();
	test_umount();
	return 0;
}
