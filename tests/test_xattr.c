/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Extended attributes: set, get, replace, list and remove, in the inode body
 * (256 byte inodes) and in an external xattr block, with many attributes,
 * large values, an empty value and every name prefix. Runs on an ext4 image
 * (256 byte inodes, metadata_csum) and an ext2 image (128 byte inodes: no
 * room in the inode, everything goes to the block). The check script reads
 * the attributes back with debugfs and runs e2fsck on both images.
 *
 * All names are 7 characters after the prefix. Other lengths, removal from
 * the inode body and changes next to an empty value are covered by the
 * regression tests of their fixes (test_xattr_list_align,
 * test_xattr_remove_ibody, test_xattr_empty_value).
 *
 * red-green: guard (coverage test, passes on the base)
 */
#include "test_util.h"

#include <string.h>

#define MANY 16

static size_t block_size;

static void set(const char *path, const char *name, const void *v, size_t len)
{
	int r = ext4_setxattr(path, name, strlen(name), v, len);

	if (r != EOK)
		fprintf(stderr, "setxattr %s %s: %d\n", path, name, r);
	TEST_ASSERT_EQ(EOK, r);
}

static void expect(const char *path, const char *name, const void *v,
		   size_t len)
{
	static char buf[4096];
	size_t got = 12345;

	TEST_ASSERT(len <= sizeof(buf));
	memset(buf, 0xa5, sizeof(buf));
	TEST_ASSERT_EQ(EOK, ext4_getxattr(path, name, strlen(name), buf,
					  sizeof(buf), &got));
	TEST_ASSERT_EQ(len, got);
	TEST_ASSERT(memcmp(buf, v, len) == 0);

	/* Size query without a buffer */
	got = 12345;
	TEST_ASSERT_EQ(EOK, ext4_getxattr(path, name, strlen(name), NULL, 0,
					  &got));
	TEST_ASSERT_EQ(len, got);
}

static void expect_missing(const char *path, const char *name)
{
	char buf[16];
	size_t got;

	TEST_ASSERT_EQ(ENODATA, ext4_getxattr(path, name, strlen(name), buf,
					      sizeof(buf), &got));
}

static void many_name(int i, char *name, size_t size)
{
	snprintf(name, size, "user.many-%02d", i);
}

static void many_value(int i, char *v, size_t len)
{
	for (size_t k = 0; k < len; k++)
		v[k] = (char)('a' + (i + k) % 26);
}

static size_t many_len(void)
{
	/* 16 attributes must fit into one xattr block together with the
	 * header: entry (16) + name (8) + value, rounded to 4. */
	return block_size >= 4096 ? 100 : 24;
}

static size_t large_len(void)
{
	return block_size >= 4096 ? 3000 : 700;
}

static int list_has(const char *list, size_t size, const char *name)
{
	for (size_t off = 0; off < size; off += strlen(list + off) + 1)
		if (!strcmp(list + off, name))
			return 1;
	return 0;
}

static int list_count(const char *list, size_t size)
{
	int n = 0;

	for (size_t off = 0; off < size; off += strlen(list + off) + 1)
		n++;
	return n;
}

static void create(const char *path)
{
	ext4_file f;
	size_t w;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "x", 1, &w));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void check_many(bool removed_odd)
{
	char name[32], v[128], list[1024];
	size_t size = 0;

	for (int i = 0; i < MANY; i++) {
		many_name(i, name, sizeof(name));
		many_value(i, v, many_len());
		if (removed_odd && (i & 1))
			expect_missing(TEST_MP "many", name);
		else
			expect(TEST_MP "many", name, v, many_len());
	}
	expect(TEST_MP "many", "trusted.trusted", "T", 1);

	/* Size query, then the list, then a buffer that is too small */
	TEST_ASSERT_EQ(EOK, ext4_listxattr(TEST_MP "many", NULL, 0, &size));
	TEST_ASSERT(size > 0 && size <= sizeof(list));
	TEST_ASSERT_EQ(EOK, ext4_listxattr(TEST_MP "many", list, sizeof(list),
					   &size));
	TEST_ASSERT_EQ(removed_odd ? MANY / 2 + 1 : MANY + 1,
		       list_count(list, size));
	TEST_ASSERT(list_has(list, size, "trusted.trusted"));
	for (int i = 0; i < MANY; i++) {
		many_name(i, name, sizeof(name));
		TEST_ASSERT_EQ(!(removed_odd && (i & 1)),
			       list_has(list, size, name));
	}
	TEST_ASSERT_EQ(ERANGE, ext4_listxattr(TEST_MP "many", list, 5, &size));
}

static void check_small(void)
{
	char list[256];
	size_t size = 0;

	expect(TEST_MP "small", "user.replace", "replaced value", 14);
	expect(TEST_MP "small", "trusted.trusted", "T", 1);
	expect(TEST_MP "small", "security.securit", "S", 1);
	expect_missing(TEST_MP "small", "user.missing");
	expect_missing(TEST_MP "small", "security.replace");
	TEST_ASSERT_EQ(EOK, ext4_listxattr(TEST_MP "small", list, sizeof(list),
					   &size));
	TEST_ASSERT_EQ(3, list_count(list, size));
	TEST_ASSERT(list_has(list, size, "user.replace"));
	TEST_ASSERT(list_has(list, size, "trusted.trusted"));
	TEST_ASSERT(list_has(list, size, "security.securit"));

	expect(TEST_MP "empty", "user.nothing", "", 0);
	expect(TEST_MP "dir", "user.on-dir1", "D", 1);
}

static void check_large(void)
{
	static char v[4096];

	many_value(7, v, large_len());
	expect(TEST_MP "large", "user.large-1", v, large_len());
	expect(TEST_MP "large", "user.after-1", "after", 5);
}

static void run(const char *image, bool remove)
{
	struct ext4_mount_stats st;
	char name[32], v[128];
	static char big[4096];

	printf("== %s\n", image);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &st));
	block_size = st.block_size;

	create(TEST_MP "small");
	create(TEST_MP "many");
	create(TEST_MP "large");
	create(TEST_MP "empty");
	create(TEST_MP "gone");

	/* Small attributes (in the inode body when there is room), replaced
	 * by a longer value, every prefix */
	set(TEST_MP "small", "user.replace", "1", 1);
	expect(TEST_MP "small", "user.replace", "1", 1);
	set(TEST_MP "small", "trusted.trusted", "T", 1);
	set(TEST_MP "small", "security.securit", "S", 1);
	set(TEST_MP "small", "user.replace", "replaced value", 14);
	set(TEST_MP "empty", "user.nothing", "", 0);
	set(TEST_MP "dir", "user.on-dir1", "D", 1);
	check_small();

	/* Names without a known prefix or with an empty name */
	TEST_ASSERT_EQ(EINVAL, ext4_setxattr(TEST_MP "small", "bogus.x", 7,
					     "v", 1));
	TEST_ASSERT_EQ(EINVAL, ext4_setxattr(TEST_MP "small", "user.", 5,
					     "v", 1));
	TEST_ASSERT_EQ(EINVAL, ext4_getxattr(TEST_MP "small", "bogus.x", 7,
					     v, sizeof(v), NULL));
	TEST_ASSERT_EQ(EINVAL, ext4_removexattr(TEST_MP "small", "bogus.x", 7));
	TEST_ASSERT_EQ(ENOENT, ext4_setxattr(TEST_MP "nonexistent",
					     "user.replace", 12, "v", 1));
	TEST_ASSERT_EQ(ENOENT, ext4_getxattr(TEST_MP "nonexistent",
					     "user.replace", 12, v, sizeof(v),
					     NULL));

	/* Many attributes: overflow the inode body into the xattr block */
	set(TEST_MP "many", "trusted.trusted", "T", 1);
	for (int i = 0; i < MANY; i++) {
		many_name(i, name, sizeof(name));
		many_value(i, v, many_len());
		set(TEST_MP "many", name, v, many_len());
	}
	check_many(false);
	if (remove) {
		/* Every other one, from the xattr block */
		for (int i = 1; i < MANY; i += 2) {
			many_name(i, name, sizeof(name));
			TEST_ASSERT_EQ(EOK, ext4_removexattr(TEST_MP "many",
							     name,
							     strlen(name)));
		}
		many_name(1, name, sizeof(name));
		TEST_ASSERT_EQ(ENODATA, ext4_removexattr(TEST_MP "many", name,
							 strlen(name)));
	}
	check_many(remove);

	/* A value too large for the inode body, then one more */
	many_value(7, big, large_len());
	set(TEST_MP "large", "user.large-1", big, large_len());
	set(TEST_MP "large", "user.after-1", "after", 5);
	check_large();
	/* No room for a value of a whole block */
	TEST_ASSERT(ext4_setxattr(TEST_MP "large", "user.huge-01", 12, big,
				  block_size) != EOK);
	check_large();

	/* Removing a file with an xattr block frees the block */
	set(TEST_MP "gone", "user.large-1", big, large_len());
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "gone"));
	test_umount();

	/* Everything is still there after a remount */
	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check_small();
	check_many(remove);
	check_large();
	TEST_ASSERT_EQ(EROFS, ext4_setxattr(TEST_MP "small", "user.replace",
					    12, "v", 1));
	TEST_ASSERT_EQ(EROFS, ext4_removexattr(TEST_MP "small", "user.replace",
					       12));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char ext2[1024];

	snprintf(ext2, sizeof(ext2), "%s.ext2", image);
	/* Removal only where every attribute is in the xattr block */
	run(image, false);
	run(ext2, true);
	return 0;
}
