/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Path lookup (ext4_generic_open2(), behind ext4_fopen(), ext4_fremove(),
 * ext4_dir_open(), ext4_dir_mk() and every other call that takes a path)
 * when the block device fails to read the inode of a path component.
 * The lookup had released the reference to the previous inode and, when
 * loading the next one failed, released the reference again on its way
 * out: an assertion in ext4_bcache_free(), and with assertions off (a
 * release build) a use after free of the block cache buffer that was
 * dropped after the failed read.
 */

#include "fault_dev.h"

#include <ext4_super.h>

#include <string.h>

static int lock_depth;

static void count_lock(void)
{
	lock_depth++;
}

static void count_unlock(void)
{
	lock_depth--;
}

static const struct ext4_lock counting_locks = {
	.lock = count_lock,
	.unlock = count_unlock,
};

/* First inode table block of group 0 (1 KiB blocks: descriptors at 2). */
static uint32_t inode_table(const char *image)
{
	uint8_t b[4];
	FILE *f = fopen(image, "rb");

	TEST_ASSERT(f);
	TEST_ASSERT(fseek(f, 2048 + 8, SEEK_SET) == 0);
	TEST_ASSERT(fread(b, 1, 4, f) == 4);
	fclose(f);
	return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24;
}

#define FILES 128

/* The file of d/ with the highest (or lowest) inode number. */
static int file_by_inode(bool highest)
{
	struct ext4_inode inode;
	uint32_t ino, best = highest ? 0 : UINT32_MAX;
	char path[32];
	int n = -1;

	for (int i = 0; i < FILES; i++) {
		snprintf(path, sizeof(path), TEST_MP "d/%d", i);
		TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
		if (highest ? ino > best : ino < best) {
			best = ino;
			n = i;
		}
	}
	return n;
}

/* Fail reads of the inode table block that holds path's inode, and push
 * that block out of the block cache by looking up the files of d/ (the
 * ones in the same block fail, that is fine). The inode must have a
 * higher number than the files of d/ and d itself, so that their lookups
 * do not need its block. */
static void fail_inode_block(const char *image, const char *path)
{
	struct ext4_sblock *sb;
	struct ext4_inode inode;
	uint32_t ino, blk;
	char other[32];

	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	TEST_ASSERT_EQ(1024, ext4_sb_get_block_size(sb));
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
	blk = inode_table(image) + (ino - 1) * ext4_get16(sb, inode_size) / 1024;

	fault_dev_fail_range(FAULT_DEV_READ, (uint64_t)blk * 1024, 1024);
	for (int i = 0; i < FILES; i++) {
		snprintf(other, sizeof(other), TEST_MP "d/%d", i);
		if (strcmp(other, path))
			ext4_inode_exist(other, EXT4_DE_REG_FILE);
	}
}

/* The same for the path walk of the new name of ext4_flink() and
 * ext4_frename() (ext4_create_hardlink()): the new name is in directory
 * e, whose inode cannot be read. */
static void new_name_lookup(const char *image, const char *from, bool rename)
{
	int r;

	fail_inode_block(image, TEST_MP "e");
	if (rename)
		r = ext4_frename(from, TEST_MP "e/new");
	else
		r = ext4_flink(from, TEST_MP "e/new");
	TEST_ASSERT_EQ(EIO, r);
	TEST_ASSERT(fault_dev_failed() > 0);
	TEST_ASSERT_EQ(0, lock_depth);

	/* The device works again. */
	fault_dev_disarm();
	if (rename)
		r = ext4_frename(from, TEST_MP "e/new");
	else
		r = ext4_flink(from, TEST_MP "e/new");
	TEST_ASSERT_EQ(EOK, r);
	TEST_ASSERT_EQ(0, lock_depth);
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "e/new", EXT4_DE_REG_FILE));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	ext4_file f;
	char path[32], buf[16];
	size_t rcnt;
	int n;

	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_setup_locks(TEST_MP, &counting_locks));

	/* ext4_generic_open2(): the file's own inode cannot be read. */
	n = file_by_inode(true);
	snprintf(path, sizeof(path), TEST_MP "d/%d", n);
	fail_inode_block(image, path);
	TEST_ASSERT_EQ(EIO, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT(fault_dev_failed() > 0);
	TEST_ASSERT_EQ(0, lock_depth);

	/* The device works again: the lookup and the file are fine. */
	fault_dev_disarm();
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT(rcnt > 5 && memcmp(buf, "file ", 5) == 0);
	TEST_ASSERT_EQ(n, atoi(buf + 5));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(0, lock_depth);

	/* A new directory gets an inode after all of d/. */
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "e"));
	snprintf(path, sizeof(path), TEST_MP "d/%d", file_by_inode(false));
	new_name_lookup(image, path, false);
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "e/new"));
	new_name_lookup(image, path, true);
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(path, EXT4_DE_REG_FILE));

	test_umount();
	return 0;
}
