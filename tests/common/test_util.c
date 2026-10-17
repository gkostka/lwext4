#include "test_util.h"

#include "../../blockdev/linux/file_dev.h"

static int (*file_dev_bwrite)(struct ext4_blockdev *bdev, const void *buf,
			      uint64_t blk_id, uint32_t blk_cnt);

/*
 * The file device hands write buffers straight to stdio, so sanitizers never
 * see an out of bounds source buffer. Read every byte here first so that a
 * wild buffer pointer or length is reported where it happens.
 */
static int checked_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt)
{
	const volatile uint8_t *p = buf;
	size_t len = (size_t)bdev->bdif->ph_bsize * blk_cnt;
	uint8_t acc = 0;

	for (size_t i = 0; i < len; i++)
		acc |= p[i];
	(void)acc;

	return file_dev_bwrite(bdev, buf, blk_id, blk_cnt);
}

int test_mount(const char *image, bool read_only)
{
	struct ext4_blockdev *bdev = file_dev_get();

	if (bdev->bdif->bwrite != checked_bwrite) {
		file_dev_bwrite = bdev->bdif->bwrite;
		bdev->bdif->bwrite = checked_bwrite;
	}

	file_dev_name_set(image);
	TEST_ASSERT_EQ(EOK, ext4_device_register(bdev, TEST_DEV));
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
