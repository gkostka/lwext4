/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * A block bitmap that says a block of an htree directory is free (found
 * by fuzzing: a symlink shared its block with the root of a directory,
 * and removing the symlink freed it). The allocator handed that block out
 * as the directory's next block, and filling it overwrote the block in
 * use: a split wrote the entries of the leaf over the root, and
 * ext4_dir_dx_insert_entry() then moved the root's index entries by a
 * count read from a file name (heap-buffer-overflow).
 *
 * Every block of the image is made in use except one of a directory (see
 * the .sh), after lwext4 made the directory:
 *
 *   /a  its root: the split of its leaf gets the root
 *   /b  its leaf: the split of the leaf gets the leaf itself
 *   /c  its root, which is full: the new index level gets the root
 *
 * The addition that gets the block must fail with EIO, and every name
 * added before must still be found.
 */

#include "test_util.h"

#include <ext4_misc.h>
#include <ext4_types.h>

#include <stdio.h>
#include <stdlib.h>

static char path[256];

static const char *name(const char *dir, int i)
{
	/* Long names: four fill a leaf */
	snprintf(path, sizeof(path), TEST_MP "%s/%0200d", dir, i);
	return path;
}

static int add(const char *dir, int i)
{
	ext4_file f;
	int r = ext4_fopen(&f, name(dir, i), "wb");

	if (r == EOK)
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	return r;
}

/* Index entries of the root, one per leaf: all blocks but the root */
static uint32_t leaves(const char *dir)
{
	struct ext4_inode inode;
	uint32_t ino;

	snprintf(path, sizeof(path), TEST_MP "%s", dir);
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
	return to_le32(inode.size_lo) / 1024 - 1;
}

static uint32_t root_limit(void)
{
	struct ext4_sblock *sb;
	/* After ., .. and the root info */
	uint32_t space = 1024 - 32;

	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	if (to_le32(sb->features_read_only) & EXT4_FRO_COM_METADATA_CSUM)
		space -= sizeof(struct ext4_dir_idx_tail);
	return space / sizeof(struct ext4_dir_idx_entry);
}

static void only_free(const char *image, const char *dir, int iblk)
{
	char cmd[1024];

	snprintf(cmd, sizeof(cmd), "sh -e '%s.only-free' /%s %d", image, dir,
		 iblk);
	TEST_ASSERT_EQ(0, system(cmd));
}

/* Names 0 to n - 1 of dir are there, name n is not */
static void check(const char *image, const char *dir, int n)
{
	const ext4_direntry *de;
	ext4_dir d;
	int i, files = 0;

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	for (i = 0; i < n; i++)
		TEST_ASSERT_EQ(EOK, ext4_inode_exist(name(dir, i),
						     EXT4_DE_REG_FILE));
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(name(dir, n),
						EXT4_DE_REG_FILE));
	snprintf(path, sizeof(path), TEST_MP "%s", dir);
	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, path));
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		if (de->inode_type == EXT4_DE_REG_FILE)
			files++;
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));
	TEST_ASSERT_EQ(n, files);
	test_umount();
}

/* Add names to dir until its leaf splits and gets the free block */
static void split(const char *image, const char *dir, int iblk)
{
	int n, r = EOK;

	only_free(image, dir, iblk);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	for (n = 0; n < 100; n++) {
		r = add(dir, n);
		if (r != EOK)
			break;
	}
	TEST_ASSERT_EQ(EIO, r);
	/* The leaf was full */
	TEST_ASSERT(n >= 4);
	test_umount();
	check(image, dir, n);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	uint32_t limit;
	int n;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "a"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "b"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "c"));
	/* A root and one leaf */
	TEST_ASSERT_EQ(1, leaves("c"));
	limit = root_limit();
	for (n = 0; leaves("c") < limit; n++) {
		TEST_ASSERT(n < 2000);
		TEST_ASSERT_EQ(EOK, add("c", n));
	}
	test_umount();

	split(image, "a", 0);
	split(image, "b", 1);

	/* The root of /c is full: the next addition adds a level */
	only_free(image, "c", 0);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EIO, add("c", n));
	test_umount();
	check(image, "c", n);
	return 0;
}
