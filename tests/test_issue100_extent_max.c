/*
 * Issue #100: ext4_ext_check() did not bound eh_max/eh_entries to the
 * capacity of the buffer holding the extent header, so a corrupted header
 * made the extent binary search and the tail checksum lookup read far past
 * the inode's i_block or the extent tree block. Reading such a file must
 * fail cleanly with EIO.
 */

#include "test_util.h"

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

	TEST_ASSERT_EQ(EOK, test_mount(image, true));

	/* Root header in i_block with eh_max = eh_entries = 0xffff. */
	read_expect_eio(TEST_MP "root_huge");
	/* Root header in i_block with eh_max = eh_entries = 5 (> 4). */
	read_expect_eio(TEST_MP "root_five");
	/* Leaf block header with eh_max = eh_entries = 0xffff. */
	read_expect_eio(TEST_MP "leaf_huge");

	test_umount();
	return 0;
}
