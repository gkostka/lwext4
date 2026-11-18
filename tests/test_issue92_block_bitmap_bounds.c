/*
 * Issue #92: ext4_fs_init_block_bitmap() set bits up to a bit_max derived
 * from unchecked superblock fields (s_first_meta_bg, s_reserved_gdt_blocks,
 * s_blocks_per_group) and wrote far past the end of the block bitmap buffer.
 * Such superblocks have to be rejected at mount time.
 */

#include "test_util.h"

static void expect_mount_fails(const char *image, const char *suffix)
{
	char path[1024];
	ext4_file f;
	int r;

	snprintf(path, sizeof(path), "%s.%s", image, suffix);
	r = test_mount(path, false);
	if (r == EOK) {
		fprintf(stderr, "%s: mount unexpectedly succeeded\n", path);
		/* Touch block group 0 to show the damage. */
		if (ext4_fopen(&f, TEST_MP "x", "wb") == EOK)
			ext4_fclose(&f);
		test_umount();
		exit(1);
	}
	ext4_device_unregister(TEST_DEV);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	expect_mount_fails(image, "meta_bg");
	expect_mount_fails(image, "rsv_gdt");
	expect_mount_fails(image, "bpg");
	return 0;
}
