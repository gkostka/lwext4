/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_xattr_list() packs the list entries it builds (struct
 * ext4_xattr_list_entry, which holds pointers and a size_t) back to back,
 * each directly after the previous entry's name. With names of most
 * lengths the next entry is misaligned: undefined behaviour, a trap on
 * CPUs without unaligned access, "misaligned address" with UBSan. It
 * happens on every ext4_listxattr() of an inode with two or more
 * attributes. The attributes here live in the inode body (256 byte inodes)
 * and in the xattr block (128 byte inodes), with names of every length
 * from 1 to 9 characters.
 */

#include "test_util.h"

#include <string.h>

#define N 9

static void list_check(const char *path, int n)
{
	char list[512], name[32];
	size_t size = 0, off;
	int found = 0;

	/* Size query, then the list itself */
	TEST_ASSERT_EQ(EOK, ext4_listxattr(path, NULL, 0, &size));
	TEST_ASSERT(size > 0 && size <= sizeof(list));
	memset(list, 0x5a, sizeof(list));
	TEST_ASSERT_EQ(EOK, ext4_listxattr(path, list, sizeof(list), &size));

	for (off = 0; off < size; off += strlen(list + off) + 1) {
		for (int i = 1; i <= n; i++) {
			snprintf(name, sizeof(name), "user.%.*s", i,
				 "abcdefghi");
			if (!strcmp(list + off, name))
				found++;
		}
	}
	TEST_ASSERT_EQ(n, found);
}

static void run(const char *image)
{
	ext4_file f;
	char name[32];

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "f", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	for (int i = 1; i <= N; i++) {
		snprintf(name, sizeof(name), "user.%.*s", i, "abcdefghi");
		TEST_ASSERT_EQ(EOK, ext4_setxattr(TEST_MP "f", name,
						  strlen(name), "v", 1));
		list_check(TEST_MP "f", i);
	}
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	list_check(TEST_MP "f", N);
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char ext2[1024];

	snprintf(ext2, sizeof(ext2), "%s.ext2", image);
	run(image);
	run(ext2);
	return 0;
}
