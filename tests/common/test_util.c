#include "test_util.h"

#include "../../blockdev/linux/file_dev.h"

int test_mount(const char *image, bool read_only)
{
	file_dev_name_set(image);
	TEST_ASSERT_EQ(EOK, ext4_device_register(file_dev_get(), TEST_DEV));
	return ext4_mount(TEST_DEV, TEST_MP, read_only);
}

void test_umount(void)
{
	ext4_umount(TEST_MP);
	ext4_device_unregister(TEST_DEV);
}

const char *test_image_arg(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <image>\n", argv[0]);
		exit(2);
	}
	return argv[1];
}
