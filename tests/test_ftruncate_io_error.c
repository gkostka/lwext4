/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_ftruncate() when the block device fails.
 *
 * - Loading the file's inode fails: ext4_ftruncate_no_lock() unlocked the
 *   mount point and ext4_ftruncate() unlocked it again, one unlock more
 *   than locks (undefined for a pthread mutex, an error or an assertion
 *   with RTOS mutexes). The lock callbacks here count.
 * - Releasing the blocks fails (reading the block bitmap of a block mapped
 *   file): ext4_ftruncate_no_lock() returned without leaving the write-back
 *   cache mode it had entered, so the block cache stayed in write-back mode
 *   for good and later changes only reached the device at ext4_umount() or
 *   ext4_cache_flush() (a power cut lost them).
 */

#include "fault_dev.h"

#include <ext4_super.h>

#include <string.h>

static int lock_depth, unlock_underflow;

static void count_lock(void)
{
	lock_depth++;
}

static void count_unlock(void)
{
	if (--lock_depth < 0)
		unlock_underflow++;
}

static const struct ext4_lock counting_locks = {
	.lock = count_lock,
	.unlock = count_unlock,
};

/* Field of group descriptor 0 (1 KiB blocks: the table is block 2). */
static uint64_t gd0_field(const char *image, long off)
{
	uint8_t b[4];
	FILE *f = fopen(image, "rb");

	TEST_ASSERT(f);
	TEST_ASSERT(fseek(f, 2048 + off, SEEK_SET) == 0);
	TEST_ASSERT(fread(b, 1, 4, f) == 4);
	fclose(f);
	return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24;
}

static void mount_counting(const char *image)
{
	TEST_ASSERT_EQ(EOK, fault_dev_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_mount_setup_locks(TEST_MP, &counting_locks));
	lock_depth = unlock_underflow = 0;
}

/* Every lock was unlocked once, and the cache is out of write-back mode. */
#define CHECK_BALANCED()                                                       \
	do {                                                                   \
		TEST_ASSERT_EQ(0, unlock_underflow);                           \
		TEST_ASSERT_EQ(0, lock_depth);                                 \
		TEST_ASSERT_EQ(0, fault_dev.cache_write_back);                 \
	} while (0)

/* The inode of the file cannot be read. */
static void inode_read_error(const char *image)
{
	struct ext4_sblock *sb;
	struct ext4_inode inode;
	ext4_file f;
	uint32_t ino, bsize, itable, blk;
	char path[32];

	mount_counting(image);
	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	bsize = ext4_sb_get_block_size(sb);
	TEST_ASSERT_EQ(1024, bsize);
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "file", &ino, &inode));
	itable = gd0_field(image, 8);
	blk = itable + (ino - 1) * ext4_get16(sb, inode_size) / bsize;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "file", "r+"));
	CHECK_BALANCED();

	/* Fail reads of the file's inode table block, then push it out of
	 * the block cache by looking up the inodes of other files (the ones
	 * in the same inode table block fail, that is fine). */
	fault_dev_fail_range(FAULT_DEV_READ, (uint64_t)blk * bsize, bsize);
	for (int i = 0; i < 64; i++) {
		snprintf(path, sizeof(path), TEST_MP "other/%d", i);
		ext4_inode_exist(path, EXT4_DE_REG_FILE);
	}
	CHECK_BALANCED();

	TEST_ASSERT_EQ(EIO, ext4_ftruncate(&f, 1000));
	TEST_ASSERT(fault_dev_failed() > 0);
	CHECK_BALANCED();

	fault_dev_disarm();
	TEST_ASSERT_EQ(EOK, ext4_ftruncate(&f, 1000));
	CHECK_BALANCED();
	TEST_ASSERT_EQ(1000, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	test_umount();
}

/* The block bitmap cannot be read when the blocks are released. */
static void bitmap_read_error(const char *image)
{
	ext4_file f;
	uint32_t bitmap = gd0_field(image, 0);

	mount_counting(image);
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "file", "r+"));

	fault_dev_fail_range(FAULT_DEV_READ, (uint64_t)bitmap * 1024, 1024);
	TEST_ASSERT_EQ(EIO, ext4_ftruncate(&f, 0));
	TEST_ASSERT(fault_dev_failed() > 0);
	CHECK_BALANCED();

	fault_dev_disarm();
	TEST_ASSERT_EQ(EOK, ext4_ftruncate(&f, 0));
	CHECK_BALANCED();
	TEST_ASSERT_EQ(0, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char ext2[512];

	inode_read_error(image);

	snprintf(ext2, sizeof(ext2), "%s.ext2", image);
	bitmap_read_error(ext2);
	return 0;
}
