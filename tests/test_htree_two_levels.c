/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * A directory with more entries than one level of htree index blocks can
 * address (about 7000 with 1 KiB blocks) gets a second index level. With
 * metadata_csum the new index node's limit was computed without room for
 * the checksum tail, so the next lookup through it failed with
 * EXT4_ERR_BAD_DX_DIR; the error path then released the root block twice
 * (assertion in ext4_bcache_free).
 */

#include "test_util.h"

#include <string.h>

#define NFILES 12000

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	const ext4_direntry *de;
	char path[128];
	ext4_file f;
	ext4_dir d;
	int i, n;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "big"));
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, true));
	for (i = 0; i < NFILES; i++) {
		snprintf(path, sizeof(path), TEST_MP "big/entry_%d", i);
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	}
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, false));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	for (i = 0; i < NFILES; i += 97) {
		snprintf(path, sizeof(path), TEST_MP "big/entry_%d", i);
		TEST_ASSERT_EQ(EOK, ext4_inode_exist(path, EXT4_DE_REG_FILE));
	}
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "big/entry_x",
						EXT4_DE_REG_FILE));
	n = 0;
	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, TEST_MP "big"));
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		if (de->inode_type == EXT4_DE_REG_FILE)
			n++;
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));
	TEST_ASSERT_EQ(NFILES, n);
	test_umount();
	return 0;
}
