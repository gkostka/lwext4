/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * fs_test.mk's images_small target ("make test") formatted its images with
 * "sudo mkfs.ext*" and fsck_images checked them with "sudo fsck.ext*". The
 * setup script runs both targets without root (see
 * test_fs_test_mk_images.sh); here lwext4 has to work on the three images,
 * which also needs the e2fsprogs >= 1.47 defaults lwext4 does not support
 * (orphan_file, metadata_csum_seed) switched off.
 */

#include "test_util.h"

#include <string.h>

int main(int argc, char **argv)
{
	static const char *const types[] = {"ext2", "ext3", "ext4"};
	const char *image = test_image_arg(argc, argv);
	char path[4096];
	const char *slash = strrchr(image, '/');
	size_t dir_len = slash ? (size_t)(slash - image) : 0;
	size_t i, cnt;
	ext4_file f;
	char buf[16];

	for (i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		TEST_ASSERT(dir_len + 32 < sizeof(path));
		snprintf(path, sizeof(path), "%.*s%sext_images/%s",
			 (int)dir_len, image, slash ? "/" : "", types[i]);
		printf("%s\n", path);

		TEST_ASSERT_EQ(EOK, test_mount(path, false));
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "f", "wb"));
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, types[i], 4, &cnt));
		TEST_ASSERT_EQ(4, cnt);
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
		test_umount();

		TEST_ASSERT_EQ(EOK, test_mount(path, true));
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "f", "rb"));
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &cnt));
		TEST_ASSERT_EQ(4, cnt);
		TEST_ASSERT(memcmp(buf, types[i], 4) == 0);
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
		test_umount();
	}
	return 0;
}
