/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Indexed (htree) directories growing to two index levels, with every
 * hash algorithm: leaf splits, index splits, a second level, lookups,
 * removal and re-insertion, directory listing. Names contain bytes >= 0x80,
 * which hash differently with signed and unsigned chars (the superblock
 * says which one the filesystem uses). The check script runs e2fsck, which
 * recomputes every hash, and checks the tree depth with debugfs.
 *
 * The images have no metadata_csum: on the base, the index node limits and
 * the leaf checksums are wrong there (test_htree_two_levels and
 * test_dir_dx_init_csum cover that with the fixes).
 *
 * red-green: guard (coverage test, passes on the base)
 */
#include "test_util.h"

#include <string.h>

static void name_of(char *path, size_t size, const char *dir, int i)
{
	/* Long names: few entries per 1 KiB leaf, many leaves. The
	 * "\xc3\xa9" (UTF-8 e acute) makes the hash depend on the char
	 * signedness. */
	snprintf(path, size,
		 "%s/entry-\xc3\xa9-%05d-%0150d", dir, i, i);
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

static int count(const char *dir)
{
	ext4_dir d;
	const ext4_direntry *de;
	int n = 0;

	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, dir));
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		if (de->name_length > 2 || de->name[0] != '.')
			n++;
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));
	return n;
}

static void run(const char *image, int files)
{
	char path[256];
	const char *dir = TEST_MP "big";

	printf("== %s: %d files\n", image, files);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, true));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(dir));
	for (int i = 0; i < files; i++) {
		name_of(path, sizeof(path), dir, i);
		create(path);
	}
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, false));
	TEST_ASSERT_EQ(files, count(dir));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	for (int i = 0; i < files; i++) {
		name_of(path, sizeof(path), dir, i);
		TEST_ASSERT_EQ(EOK, ext4_inode_exist(path, EXT4_DE_REG_FILE));
	}
	name_of(path, sizeof(path), dir, files);
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(path, EXT4_DE_REG_FILE));

	/* Remove every third entry, then put half of them back. */
	for (int i = 0; i < files; i += 3) {
		name_of(path, sizeof(path), dir, i);
		TEST_ASSERT_EQ(EOK, ext4_fremove(path));
	}
	for (int i = 0; i < files; i++) {
		name_of(path, sizeof(path), dir, i);
		TEST_ASSERT_EQ(i % 3 ? EOK : ENOENT,
			       ext4_inode_exist(path, EXT4_DE_REG_FILE));
	}
	for (int i = 0; i < files; i += 6) {
		name_of(path, sizeof(path), dir, i);
		create(path);
	}
	TEST_ASSERT_EQ(files - (files + 2) / 3 + (files + 5) / 6, count(dir));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char other[1024];

	/* Enough entries for a second index level with half_md4. */
	run(image, 1200);
	snprintf(other, sizeof(other), "%s.tea", image);
	run(other, 300);
	snprintf(other, sizeof(other), "%s.legacy", image);
	run(other, 300);
	return 0;
}
