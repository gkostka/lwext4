/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_fwrite() overwrote its error code with the result of releasing the
 * inode (EOK) on the way out. When the filesystem was full it returned
 * EOK: with a short count while blocks were still found, then EOK and 0
 * bytes on every further call, so callers that check the return code
 * never learnt that their data was not written. The failing allocation
 * also skipped switching the block cache back from write back mode, so
 * the cache stayed in write back mode after the call.
 *
 * Writes until the filesystem is full, without and with a journal: the
 * call that cannot write everything must return ENOSPC, the counts must
 * add up to the file size, the cache must be back in write through mode,
 * and after a remount the file must hold exactly the data written.
 */

#include "test_util.h"

#include <ext4_blockdev.h>
#include "../blockdev/linux/file_dev.h"

#include <string.h>

#define CHUNK 4096

static uint8_t buf[CHUNK];

static void fill(size_t chunk)
{
	for (size_t i = 0; i < CHUNK; i++)
		buf[i] = (uint8_t)(chunk * 7 + i);
}

static uint64_t fill_up(void)
{
	ext4_file f;
	uint64_t total = 0;
	size_t n;
	int r = EOK;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "wb"));
	for (size_t c = 0; c < 100000; c++) {
		fill(c);
		n = 12345;
		r = ext4_fwrite(&f, buf, CHUNK, &n);
		TEST_ASSERT(n <= CHUNK);
		total += n;
		if (r != EOK || n < CHUNK)
			break;
	}
	if (r != ENOSPC)
		fprintf(stderr, "full after %llu bytes: %d, %zu bytes written\n",
			(unsigned long long)total, r, n);
	TEST_ASSERT_EQ(ENOSPC, r);
	TEST_ASSERT_EQ(0, file_dev_get()->cache_write_back);

	/* Nothing more fits */
	n = 12345;
	TEST_ASSERT_EQ(ENOSPC, ext4_fwrite(&f, buf, CHUNK, &n));
	TEST_ASSERT_EQ(0, n);
	TEST_ASSERT_EQ(0, file_dev_get()->cache_write_back);
	TEST_ASSERT_EQ(total, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	return total;
}

static void verify(uint64_t size)
{
	ext4_file f;
	uint8_t rd[CHUNK];
	size_t n;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "big", "rb"));
	TEST_ASSERT_EQ(size, ext4_fsize(&f));
	for (size_t c = 0; (uint64_t)c * CHUNK < size; c++) {
		uint64_t left = size - (uint64_t)c * CHUNK;
		size_t want = left < CHUNK ? (size_t)left : CHUNK;

		fill(c);
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, rd, CHUNK, &n));
		TEST_ASSERT_EQ(want, n);
		TEST_ASSERT(memcmp(rd, buf, want) == 0);
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void run(const char *image, bool journal)
{
	uint64_t size;

	printf("== %s%s\n", image, journal ? " (journal)" : "");
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	if (journal) {
		TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
		TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	}
	size = fill_up();
	TEST_ASSERT(size > 0);
	if (journal)
		TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	verify(size);
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char other[1024];

	run(image, false);
	snprintf(other, sizeof(other), "%s.journal", image);
	run(other, true);
	return 0;
}
