/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Running out of space and inodes. On small images with several block
 * groups, files are written until the blocks run out, and created until the
 * inodes run out: the calls must fail with ENOSPC, leave what was written
 * before intact and the filesystem consistent, and everything must work
 * again after files are removed. Allocation walks through every group,
 * fills them and wraps around. The check script runs e2fsck.
 *
 * red-green: guard (coverage test, passes on the base)
 */
#include "test_util.h"

#include <string.h>

#define CHUNK 4096

static uint8_t buf[CHUNK];

static uint64_t free_blocks(void)
{
	struct ext4_mount_stats st;

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &st));
	return st.free_blocks_count;
}

static uint32_t free_inodes(void)
{
	struct ext4_mount_stats st;

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &st));
	return st.free_inodes_count;
}

static void fill(int file, size_t chunk)
{
	for (size_t i = 0; i < CHUNK; i++)
		buf[i] = (uint8_t)(file * 31 + chunk * 7 + i);
}

/* Write @path in chunks until ENOSPC; return the bytes written. */
static uint64_t write_until_full(const char *path, int file)
{
	ext4_file f;
	uint64_t total = 0;
	size_t n;
	int r;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
	for (size_t c = 0;; c++) {
		fill(file, c);
		r = ext4_fwrite(&f, buf, CHUNK, &n);
		total += n;
		/* The return code when full is test_fwrite_enospc's subject */
		if (r != EOK || n < CHUNK) {
			TEST_ASSERT(r == EOK || r == ENOSPC);
			break;
		}
	}
	TEST_ASSERT_EQ(total, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	return total;
}

static void verify(const char *path, int file, uint64_t size)
{
	ext4_file f;
	uint8_t rd[CHUNK];
	size_t n;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(size, ext4_fsize(&f));
	for (size_t c = 0; (uint64_t)c * CHUNK < size; c++) {
		size_t want = size - (uint64_t)c * CHUNK < CHUNK ?
				      (size_t)(size - (uint64_t)c * CHUNK) :
				      CHUNK;

		fill(file, c);
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, rd, CHUNK, &n));
		TEST_ASSERT_EQ(want, n);
		TEST_ASSERT(memcmp(rd, buf, want) == 0);
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void blocks(const char *image)
{
	uint64_t size_a, size_b, size_c, before;
	ext4_file f;
	size_t n;

	printf("== blocks: %s\n", image);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	before = free_blocks();

	/* Two files share the space, then one more takes what is left */
	size_a = write_until_full(TEST_MP "a", 1);
	TEST_ASSERT(size_a > 0);
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "a"));
	TEST_ASSERT_EQ(before, free_blocks());

	size_a = write_until_full(TEST_MP "a", 1);
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "a", "r+b"));
	TEST_ASSERT_EQ(EOK, ext4_ftruncate(&f, size_a / 2));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	size_a /= 2;
	size_b = write_until_full(TEST_MP "b", 2);
	TEST_ASSERT(size_b > 0);

	/* Full: a new file gets no blocks */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "c", "wb"));
	n = 12345;
	ext4_fwrite(&f, buf, CHUNK, &n);
	TEST_ASSERT_EQ(0, n);
	TEST_ASSERT_EQ(0, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	verify(TEST_MP "a", 1, size_a);
	verify(TEST_MP "b", 2, size_b);
	test_umount();

	/* After a remount, free space again */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	verify(TEST_MP "a", 1, size_a);
	verify(TEST_MP "b", 2, size_b);
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "b"));
	size_c = write_until_full(TEST_MP "c", 3);
	TEST_ASSERT(size_c >= size_b);
	verify(TEST_MP "c", 3, size_c);
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "c"));
	test_umount();
}

static void inodes(const char *image)
{
	char path[64];
	ext4_file f;
	int n = 0, r;

	printf("== inodes: %s\n", image);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT(free_inodes() > 0);
	for (;; n++) {
		snprintf(path, sizeof(path), TEST_MP "f%04d", n);
		r = ext4_fopen(&f, path, "wb");
		if (r != EOK)
			break;
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	}
	TEST_ASSERT_EQ(ENOSPC, r);
	TEST_ASSERT_EQ(0, free_inodes());
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(path, EXT4_DE_REG_FILE));
	TEST_ASSERT_EQ(ENOSPC, ext4_dir_mk(TEST_MP "dir"));

	/* Remove every other one: new files fill the holes in every group */
	for (int i = 0; i < n; i += 2) {
		snprintf(path, sizeof(path), TEST_MP "f%04d", i);
		TEST_ASSERT_EQ(EOK, ext4_fremove(path));
	}
	for (int i = 0; i < n; i += 2) {
		snprintf(path, sizeof(path), TEST_MP "g%04d", i);
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	}
	TEST_ASSERT_EQ(0, free_inodes());
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char other[1024];

	blocks(image);
	snprintf(other, sizeof(other), "%s.ext2", image);
	blocks(other);
	snprintf(other, sizeof(other), "%s.inodes", image);
	inodes(other);
	return 0;
}
