/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Whole block writes into unwritten (preallocated) extents. Before a block
 * of an unwritten extent is marked written, ext4_ext_zero_unwritten_range()
 * zeroes it, and it did so through the block cache (and the journal) while
 * ext4_fwrite() writes whole blocks straight to the device, in write-back
 * mode. The zeroed copy stayed dirty in the cache and was written back
 * over the file data when write-back mode ended: the data was lost, the
 * blocks read as zeros. With the journal, the journaled zero block is
 * checkpointed over the data the same way.
 *
 * Whole blocks at the start and at the end of an unwritten extent, and
 * appended into the blocks preallocated beyond the end of a file, without
 * and with the journal; every file is compared with what it must hold
 * after every write and after a remount.
 */

#include "test_util.h"

#include <string.h>

#define BS 1024
#define U_SIZE (200 * BS)
#define PRE_SIZE (16 * BS)
#define MAX_SIZE (256 * BS)

static uint8_t expect_u[MAX_SIZE], expect_pre[MAX_SIZE], buf[MAX_SIZE];
static uint32_t size_u = U_SIZE, size_pre = PRE_SIZE;

/* Read block by block: the reads of unwritten blocks are single block
 * reads (reading runs of them is gkostka/lwext4#103). */
static void check_file(const char *path, const uint8_t *expect, uint32_t size)
{
	ext4_file f;
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(size, ext4_fsize(&f));
	memset(buf, 0x5a, sizeof(buf));
	for (uint32_t off = 0; off < size; off += BS) {
		uint32_t len = size - off < BS ? size - off : BS;

		TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf + off, len, &rcnt));
		TEST_ASSERT_EQ(len, rcnt);
	}
	for (uint32_t i = 0; i < size; i++)
		if (buf[i] != expect[i]) {
			fprintf(stderr, "%s: byte %u is 0x%02x, expected 0x%02x\n",
				path, i, buf[i], expect[i]);
			exit(1);
		}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void write_blocks(const char *path, uint8_t *expect, uint32_t *size,
			 uint32_t blk, uint32_t cnt, uint8_t c)
{
	ext4_file f;
	size_t wcnt;

	for (uint32_t i = 0; i < cnt * BS; i++)
		expect[blk * BS + i] = (uint8_t)(c + i % 7);
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "r+"));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, blk * BS, SEEK_SET));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, expect + blk * BS, cnt * BS, &wcnt));
	TEST_ASSERT_EQ(cnt * BS, wcnt);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	if ((blk + cnt) * BS > *size)
		*size = (blk + cnt) * BS;
	check_file(path, expect, *size);
}

static void writes(bool journal, uint32_t first, uint32_t last, uint8_t c)
{
	if (journal) {
		TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
		TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	}
	/* The first and the last blocks of the unwritten extent of /u. */
	write_blocks(TEST_MP "u", expect_u, &size_u, first, 2, c);
	write_blocks(TEST_MP "u", expect_u, &size_u, last, 1, c + 1);
	/* Appended into the preallocated blocks of /pre. */
	write_blocks(TEST_MP "pre", expect_pre, &size_pre, size_pre / BS, 3,
		     c + 2);
	if (journal)
		TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	for (int i = 0; i < PRE_SIZE; i++)
		expect_pre[i] = (uint8_t)('A' + i % 26);

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	check_file(TEST_MP "u", expect_u, size_u);
	check_file(TEST_MP "pre", expect_pre, size_pre);
	writes(false, 0, 199, 'a');
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	check_file(TEST_MP "u", expect_u, size_u);
	check_file(TEST_MP "pre", expect_pre, size_pre);
	/* What is left unwritten of /u: blocks 2 to 198. */
	writes(true, 2, 198, 'k');
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	check_file(TEST_MP "u", expect_u, size_u);
	check_file(TEST_MP "pre", expect_pre, size_pre);
	test_umount();
	return 0;
}
