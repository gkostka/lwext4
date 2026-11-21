/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_dir_rm() on filesystems with block mapped directories (ext2, ext3)
 * and dir_index. Removing a directory goes through ext4_trunc_dir(), which
 * turns it into an empty indexed directory of two blocks with
 * ext4_dir_dx_init() before truncating it. ext4_dir_dx_init() decided
 * whether to add blocks by comparing the directory's size in bytes with
 * the number of blocks it needs (2), so for any existing directory it
 * only looked the two blocks up. Without extents, looking up a block past
 * the end of a one block directory allocates nothing and gives physical
 * block 0, and the new empty directory block was written there: the boot
 * block with 1 KiB blocks, the superblock with larger ones (the
 * filesystem no longer mounts). With assertions on, lwext4 stops in
 * ext4_bcache_free() on the block number 0 first.
 *
 * Remove a directory tree (files, a subdirectory, an empty directory) on
 * ext2 and ext3; the filesystem must still mount afterwards, with the
 * other files intact and the tree gone.
 */

#include "test_util.h"

#include <string.h>

static void check_keep(void)
{
	char buf[16];
	ext4_file f;
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "keep", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(5, rcnt);
	TEST_ASSERT(memcmp(buf, "keep\n", 5) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void rm_tree(const char *image)
{
	struct ext4_mount_stats before, after;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &before));
	TEST_ASSERT_EQ(EOK, ext4_dir_rm(TEST_MP "d"));
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "d", EXT4_DE_DIR));
	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &after));
	/* d, d/sub, d/empty, d/f1, d/sub/f2 */
	TEST_ASSERT_EQ(before.free_inodes_count + 5, after.free_inodes_count);
	check_keep();
	test_umount();

	/* The superblock is intact: it mounts, the rest is there. */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	check_keep();
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "d", EXT4_DE_DIR));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d"));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char ext3[512];

	rm_tree(image);
	snprintf(ext3, sizeof(ext3), "%s.ext3", image);
	rm_tree(ext3);
	return 0;
}
