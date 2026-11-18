/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * ext4_mkfs_read_info() pointed info->label into the superblock copy it had
 * just freed (use after free, reported by AddressSanitizer when lwext4-mkfs
 * prints the label), and a 16 character label has no terminating NUL on
 * disk.
 */

#include "test_util.h"

#include <ext4_mkfs.h>

#include <string.h>

#include "../../blockdev/linux/file_dev.h"

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	struct ext4_mkfs_info info;

	file_dev_name_set(image);
	memset(&info, 0, sizeof(info));
	TEST_ASSERT_EQ(EOK, ext4_mkfs_read_info(file_dev_get(), &info));
	TEST_ASSERT_EQ(1024, info.block_size);
	TEST_ASSERT(info.label != NULL);
	TEST_ASSERT_EQ(16, strlen(info.label));
	TEST_ASSERT(strcmp(info.label, "sixteen-chars-ok") == 0);
	return 0;
}
