/*
 * Issue #39: ext4_readlink fails on fast symlinks with a non-zero i_blocks.
 *
 * A symlink whose target is shorter than 60 bytes is stored inline in
 * i_block (a "fast" symlink). If the inode also owns an extended attribute
 * block, that block is counted in i_blocks, so i_blocks != 0 does not mean the
 * link target lives in a data block. lwext4 treated such links as slow ones:
 * readlink interpreted the inline target text as block pointers, and removing
 * the link freed "blocks" parsed from that text, corrupting the filesystem.
 */

#include "test_util.h"

#include <string.h>

#define LONG_FAST "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/file.txt"
#define SLOW                                                                   \
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"                   \
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/file.txt"

static void check_link(const char *path, const char *expected)
{
	char buf[256];
	size_t rcnt = 0;
	int r;

	memset(buf, 0, sizeof(buf));
	r = ext4_readlink(path, buf, sizeof(buf), &rcnt);
	if (r != EOK || rcnt != strlen(expected) ||
	    memcmp(buf, expected, rcnt) != 0) {
		fprintf(stderr, "readlink(%s): r=%d rcnt=%zu got \"%.*s\", "
			"expected \"%s\"\n", path, r, rcnt, (int)rcnt, buf,
			expected);
		exit(1);
	}
}

static void check_fsck(const char *image)
{
	char cmd[1024];

	snprintf(cmd, sizeof(cmd),
		 "PATH=\"$PATH:/sbin:/usr/sbin\" e2fsck -fn '%s'", image);
	TEST_ASSERT_EQ(0, system(cmd));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	/* Read the links back. */
	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check_link(TEST_MP "fast", "file.txt");
	check_link(TEST_MP "fast_xattr", "file.txt");
	check_link(TEST_MP "fast_long_xattr", LONG_FAST);
	check_link(TEST_MP "slow", SLOW);
	test_umount();

	/* Removing links must not free blocks parsed from inline targets. */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "fast_xattr"));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "fast_long_xattr"));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "slow"));

	/* Symlinks created by lwext4 itself, fast and slow. */
	TEST_ASSERT_EQ(EOK, ext4_fsymlink(LONG_FAST, TEST_MP "new_fast"));
	TEST_ASSERT_EQ(EOK, ext4_fsymlink(SLOW, TEST_MP "new_slow"));
	check_link(TEST_MP "new_fast", LONG_FAST);
	check_link(TEST_MP "new_slow", SLOW);
	check_link(TEST_MP "fast", "file.txt");
	test_umount();

	check_fsck(image);

	/* And remove those again. */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "new_fast"));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "new_slow"));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "fast"));
	test_umount();

	check_fsck(image);
	return 0;
}
