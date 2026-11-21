/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * A filesystem with the 64bit feature has group descriptors of at least 64
 * bytes (Linux refuses to mount it otherwise). With a smaller s_desc_size
 * (a damaged superblock), ext4_sb_get_desc_size() rounds anything below 32
 * up to 32 and lwext4 mounted it, walking the descriptor table in steps
 * of 32 bytes: every group but the first got the second half of another
 * group's descriptor (bitmap and inode table locations), and writes went
 * to wherever those pointed. ext4_sb_check() tested the rounded size
 * against the minimum of 32, which can never fail.
 *
 * s_desc_size 0, 16 and 32 on a 64bit filesystem must be refused; the
 * unmodified image mounts.
 */

#include "test_util.h"

static void copy_with_desc_size(const char *from, const char *to,
				uint16_t size)
{
	static char buf[65536];
	FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
	size_t n;
	uint8_t b[2] = {(uint8_t)size, (uint8_t)(size >> 8)};

	TEST_ASSERT(in && out);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
	fclose(in);
	/* s_desc_size: offset 0xfe of the superblock */
	TEST_ASSERT(fseek(out, 1024 + 0xfe, SEEK_SET) == 0);
	TEST_ASSERT_EQ(2, fwrite(b, 1, 2, out));
	TEST_ASSERT_EQ(0, fclose(out));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static const uint16_t sizes[] = {0, 16, 32};
	char copy[512];
	ext4_file f;

	snprintf(copy, sizeof(copy), "%s.copy", image);
	for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		copy_with_desc_size(image, copy, sizes[i]);
		if (test_mount(copy, false) == EOK) {
			fprintf(stderr, "64bit, s_desc_size %u: mounted\n",
				sizes[i]);
			exit(1);
		}
		TEST_ASSERT_EQ(EOK, ext4_device_unregister(TEST_DEV));
	}

	/* The unmodified image (s_desc_size 64) still mounts. */
	copy_with_desc_size(image, copy, 64);
	TEST_ASSERT_EQ(EOK, test_mount(copy, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "file", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	test_umount();
	return 0;
}
