/*
 * Issue #93: ext4_fs_init_inode_bitmap() cleared (inodes_per_group + 7) / 8
 * bytes of a block sized buffer without checking s_inodes_per_group against
 * the block size (heap buffer overflow). Such a superblock has to be rejected
 * at mount time.
 */

#include "test_util.h"

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;
	int r;

	r = test_mount(image, false);
	if (r == EOK) {
		fprintf(stderr, "mount unexpectedly succeeded\n");
		/* Touch block group 0 to show the damage. */
		if (ext4_fopen(&f, TEST_MP "x", "wb") == EOK)
			ext4_fclose(&f);
		test_umount();
		return 1;
	}
	ext4_device_unregister(TEST_DEV);
	return 0;
}
