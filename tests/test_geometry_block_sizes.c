/*
 * The superblock geometry checks must not reject valid filesystems: mount
 * ext2 and ext4 images of 1, 2 and 4 KiB blocks, read a file back and write
 * into several directories.
 *
 * red-green: guard (must also pass without the fix: valid geometries)
 */

#include "test_util.h"

#include <string.h>

static void check_image(const char *image, const char *suffix)
{
	char path[1024];
	char buf[64];
	ext4_file f;
	size_t cnt;
	int i;

	snprintf(path, sizeof(path), "%s.%s", image, suffix);
	fprintf(stderr, "checking %s\n", path);
	TEST_ASSERT_EQ(EOK, test_mount(path, false));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "hello.txt", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &cnt));
	TEST_ASSERT_EQ(13, cnt);
	TEST_ASSERT(memcmp(buf, "hello lwext4\n", 13) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	for (i = 0; i < 8; i++) {
		snprintf(buf, sizeof(buf), TEST_MP "d%d", i);
		TEST_ASSERT_EQ(EOK, ext4_dir_mk(buf));
		snprintf(buf, sizeof(buf), TEST_MP "d%d/f", i);
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, buf, "wb"));
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "abc", 3, &cnt));
		TEST_ASSERT_EQ(3, cnt);
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	}

	test_umount();

	/* It still mounts after our own writes. */
	TEST_ASSERT_EQ(EOK, test_mount(path, true));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	check_image(image, "1024");
	check_image(image, "2048");
	check_image(image, "4096");
	check_image(image, "ext2.1024");
	check_image(image, "ext2.2048");
	check_image(image, "ext2.4096");
	return 0;
}
