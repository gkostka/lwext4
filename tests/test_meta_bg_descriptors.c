/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Filesystems with meta_bg keep the group descriptors not in one table
 * after the superblock but one descriptor block per meta group (here 32
 * groups), in the first group of the meta group (after its superblock
 * backup, if it has one). ext4_fs_get_descriptor_block() looked in the
 * group whose descriptor it wanted instead, so every group but the first
 * one of a meta group (and the second, which has a backup) got some other
 * block as its descriptor, e.g. its own block bitmap: allocating in those
 * groups failed (ENXIO: block numbers beyond the device) or went to
 * random blocks.
 *
 * Fill the filesystem, which allocates in every group, read everything
 * back after a remount, then remove it all: the free block and inode
 * counts must be back where they started.
 */

#include "test_util.h"

#include <string.h>

#define CHUNK 16384
#define MAX_FILES 64

static char buf[CHUNK], back[CHUNK];

static void fill(int file, int chunk)
{
	for (int i = 0; i < CHUNK; i += 4) {
		uint32_t v = (uint32_t)file << 24 | (uint32_t)chunk << 16 | i;

		memcpy(buf + i, &v, 4);
	}
}

static void stats(struct ext4_mount_stats *st)
{
	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, st));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	struct ext4_mount_stats start, full, end;
	char path[32];
	ext4_file f;
	size_t cnt;
	int files = 0, chunks[MAX_FILES] = {0};
	bool full_fs = false;
	int r;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	stats(&start);
	/* Files of up to 2 MiB until the filesystem is full. */
	while (!full_fs && files < MAX_FILES) {
		snprintf(path, sizeof(path), TEST_MP "f%02d", files);
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
		for (int c = 0; c < 128; c++) {
			fill(files, c);
			r = ext4_fwrite(&f, buf, CHUNK, &cnt);
			/* A full filesystem: ENOSPC (EOK and a short write
			 * without gkostka/lwext4#150), nothing else. */
			TEST_ASSERT(r == EOK || r == ENOSPC);
			if (cnt != CHUNK) {
				full_fs = true;
				break;
			}
			chunks[files]++;
		}
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
		files++;
	}
	/* Full, and not because a group could not be used */
	TEST_ASSERT(full_fs);
	stats(&full);
	TEST_ASSERT(full.free_blocks_count < start.free_blocks_count / 50);
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	for (int i = 0; i < files; i++) {
		snprintf(path, sizeof(path), TEST_MP "f%02d", i);
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
		for (int c = 0; c < chunks[i]; c++) {
			fill(i, c);
			TEST_ASSERT_EQ(EOK, ext4_fread(&f, back, CHUNK, &cnt));
			TEST_ASSERT_EQ(CHUNK, cnt);
			TEST_ASSERT(memcmp(buf, back, CHUNK) == 0);
		}
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	}
	for (int i = 0; i < files; i++) {
		snprintf(path, sizeof(path), TEST_MP "f%02d", i);
		TEST_ASSERT_EQ(EOK, ext4_fremove(path));
	}
	stats(&end);
	TEST_ASSERT_EQ(start.free_blocks_count, end.free_blocks_count);
	TEST_ASSERT_EQ(start.free_inodes_count, end.free_inodes_count);
	test_umount();
	return 0;
}
