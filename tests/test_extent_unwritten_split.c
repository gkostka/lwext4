/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Writing into the middle of an unwritten (preallocated) extent splits it
 * in three: unwritten | written | unwritten. ext4_ext_convert_to_initialized()
 * split off the right part and then split "the extent" of the path again at
 * the start of the written part, but the insertion of the right part had
 * left the path at the right part: the second split computed its length
 * and physical block from the wrong extent. The extent tree then had
 * overlapping extents with bogus physical blocks and lengths (e2fsck
 * reports it, lwext4 fails to read the file with EINVAL, and the blocks
 * behind the bogus extents get overwritten).
 *
 * Small writes, each inside one block, into the middle of the extent and
 * of the parts left of earlier splits; the whole file is compared with
 * what it must hold after every write and after a remount.
 */

#include "test_util.h"

#include <string.h>

#define BS 1024
#define SIZE (200 * BS)

static uint8_t expect[SIZE], buf[SIZE];

/* Read block by block: the reads of unwritten blocks are single block
 * reads (reading runs of them is gkostka/lwext4#103). */
static void check_file(void)
{
	ext4_file f;
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "u", "rb"));
	TEST_ASSERT_EQ(SIZE, ext4_fsize(&f));
	memset(buf, 0x5a, sizeof(buf));
	for (int off = 0; off < SIZE; off += BS) {
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf + off, BS, &rcnt));
		TEST_ASSERT_EQ(BS, rcnt);
	}
	for (int i = 0; i < SIZE; i++)
		if (buf[i] != expect[i]) {
			fprintf(stderr, "byte %d is 0x%02x, expected 0x%02x\n",
				i, buf[i], expect[i]);
			exit(1);
		}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void write_at(uint32_t off, uint32_t len, uint8_t c)
{
	ext4_file f;
	size_t wcnt;

	/* Inside one block */
	TEST_ASSERT(off / BS == (off + len - 1) / BS);
	memset(expect + off, c, len);
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "u", "r+"));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, off, SEEK_SET));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, expect + off, len, &wcnt));
	TEST_ASSERT_EQ(len, wcnt);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	check_file();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	check_file();

	/* The middle of the extent, then the middle of the left and of the
	 * right unwritten part, then next to written blocks. */
	write_at(50 * BS + 100, 100, 'm');
	write_at(20 * BS + 1000, 24, 'l');
	write_at(150 * BS, 1, 'r');
	write_at(51 * BS, 10, 'n');
	write_at(49 * BS + 1014, 10, 'p');
	for (int blk = 100; blk < 140; blk += 3)
		write_at(blk * BS + 17, 5, 'o');
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	check_file();
	test_umount();
	return 0;
}
