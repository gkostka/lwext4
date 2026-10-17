/*
 * Issue #90: a superblock with an out of range s_log_block_size made
 * ext4_sb_get_block_size() overflow to 0, which ext4_mount() then passed to
 * ext4_block_set_lb_size() where it was used as a divisor (SIGFPE).
 * The mount has to be rejected instead.
 */

#include "test_util.h"

static void expect_mount_fails(const char *image, const char *suffix)
{
	char path[1024];
	int r;

	snprintf(path, sizeof(path), "%s.%s", image, suffix);
	r = test_mount(path, true);
	if (r == EOK) {
		fprintf(stderr, "%s: mount unexpectedly succeeded\n", path);
		test_umount();
		exit(1);
	}
	ext4_device_unregister(TEST_DEV);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	expect_mount_fails(image, "7");
	expect_mount_fails(image, "22");
	expect_mount_fails(image, "30");
	return 0;
}
