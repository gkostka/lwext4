/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_removexattr() on a path that does not exist returned with the
 * mount point locked twice instead of unlocked: with real lock callbacks
 * (a mutex, ext4_mount_setup_locks) the next filesystem call deadlocks.
 * The lock callbacks here count, and every call must leave the count at
 * zero, on success and on error.
 */

#include "test_util.h"

#include <string.h>

static int depth, max_depth;

static void lock(void)
{
	depth++;
	if (depth > max_depth)
		max_depth = depth;
}

static void unlock(void)
{
	depth--;
}

static const struct ext4_lock locks = {
	.lock = lock,
	.unlock = unlock,
};

#define BALANCED(call)                                                         \
	do {                                                                   \
		(void)(call);                                                  \
		if (depth != 0)                                                \
			fprintf(stderr, "lock depth %d after %s\n", depth,    \
				#call);                                        \
		TEST_ASSERT_EQ(0, depth);                                      \
	} while (0)

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;
	char buf[64];
	size_t len;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_setup_locks(TEST_MP, &locks));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "f", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(0, depth);

	/* The failing case */
	TEST_ASSERT_EQ(ENOENT, ext4_removexattr(TEST_MP "missing", "user.a",
						6));
	BALANCED(0);

	/* The other xattr calls, on success and on error */
	BALANCED(ext4_setxattr(TEST_MP "f", "user.a", 6, "v", 1));
	BALANCED(ext4_getxattr(TEST_MP "f", "user.a", 6, buf, sizeof(buf),
			       &len));
	BALANCED(ext4_listxattr(TEST_MP "f", buf, sizeof(buf), &len));
	BALANCED(ext4_removexattr(TEST_MP "f", "user.a", 6));
	BALANCED(ext4_removexattr(TEST_MP "f", "bogus.a", 7));
	BALANCED(ext4_setxattr(TEST_MP "missing", "user.a", 6, "v", 1));
	BALANCED(ext4_getxattr(TEST_MP "missing", "user.a", 6, buf,
			       sizeof(buf), &len));
	BALANCED(ext4_listxattr(TEST_MP "missing", buf, sizeof(buf), &len));
	TEST_ASSERT(max_depth == 1);

	test_umount();
	TEST_ASSERT_EQ(0, depth);
	return 0;
}
