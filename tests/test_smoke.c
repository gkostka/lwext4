/*
 * Sanity check for the test harness: mount an image built by mke2fs, read a
 * file back, write a new one and read it back again.
 */

#include "test_util.h"

#include <string.h>

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;
	char buf[64];
	size_t rcnt, wcnt;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "hello.txt", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(13, rcnt);
	TEST_ASSERT(memcmp(buf, "hello lwext4\n", 13) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "new.txt", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "abc", 3, &wcnt));
	TEST_ASSERT_EQ(3, wcnt);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "new.txt", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(3, rcnt);
	TEST_ASSERT(memcmp(buf, "abc", 3) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	test_umount();
	return 0;
}
