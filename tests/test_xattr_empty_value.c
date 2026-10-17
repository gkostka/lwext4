/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * An attribute with an empty value has value offset zero. When another
 * attribute's value was removed or replaced, ext4_xattr_set_entry() moved
 * the values in front of it and adjusted their offsets, including the zero
 * offset of the empty value. An entry with no value but a non-zero offset
 * makes the whole xattr block (or inode body) invalid: every following
 * ext4_getxattr() on the inode failed with EIO, and e2fsck clears the
 * attributes. Covered in the xattr block (ext2, 128 byte inodes: removal
 * and replacement) and in the inode body (ext4, 256 byte inodes:
 * replacement).
 */

#include "test_util.h"

#include <string.h>

#define F TEST_MP "f"

static void set(const char *name, const char *v)
{
	TEST_ASSERT_EQ(EOK, ext4_setxattr(F, name, strlen(name), v,
					  strlen(v)));
}

static void expect(const char *name, const char *v)
{
	char buf[64];
	size_t len = 12345;

	TEST_ASSERT_EQ(EOK, ext4_getxattr(F, name, strlen(name), buf,
					  sizeof(buf), &len));
	TEST_ASSERT_EQ(strlen(v), len);
	TEST_ASSERT(memcmp(buf, v, len) == 0);
}

static void check(bool removed)
{
	char buf[8];
	size_t len;

	expect("user.empty", "");
	expect("user.a", "a longer value");
	if (removed)
		TEST_ASSERT_EQ(ENODATA, ext4_getxattr(F, "user.b", 6, buf,
						      sizeof(buf), &len));
	else
		expect("user.b", "B");
}

static void run(const char *image, bool remove)
{
	ext4_file f;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, F, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	set("user.empty", "");
	set("user.a", "A");
	set("user.b", "B");
	/* Replacing user.a moves its value (and user.b's) */
	set("user.a", "a longer value");
	check(false);
	if (remove) {
		TEST_ASSERT_EQ(EOK, ext4_removexattr(F, "user.b", 6));
		check(true);
	}
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check(remove);
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char ext2[1024];

	snprintf(ext2, sizeof(ext2), "%s.ext2", image);
	run(ext2, true);
	/* Removal from the inode body is covered elsewhere */
	run(image, false);
	return 0;
}
