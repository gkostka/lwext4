/*
 * Issue #91: ext4_ext_binsearch_idx() trusted the root extent header, which
 * was never validated. An index node (eh_depth > 0) with eh_entries == 0
 * made the lookup follow an index slot that is not in use, and an oversized
 * eh_entries made the binary search read far past i_block. A root depth
 * beyond the ext4 maximum must be rejected as well. Reading such a file
 * must fail cleanly with EIO.
 */

#include "test_util.h"

#include <string.h>

static void read_expect_eio(const char *path)
{
	ext4_file f;
	char buf[64];
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(EIO, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;
	char buf[64];
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, test_mount(image, true));

	/* An intact depth 1 tree must still be readable. */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "idx_ok", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, 10 * 4096, SEEK_SET));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(7, rcnt);
	TEST_ASSERT(memcmp(buf, "chunk10", 7) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	/* Depth 1 root with no index entries. */
	read_expect_eio(TEST_MP "idx_empty");
	/* Depth 1 root with eh_max = eh_entries = 0xffff. */
	read_expect_eio(TEST_MP "idx_huge");
	/* Root depth 0xffff. */
	read_expect_eio(TEST_MP "idx_deep");

	test_umount();
	return 0;
}
