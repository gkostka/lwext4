/*
 * Regression test for issue #67: "Failure on writing large files".
 *
 * ext4_fwrite() appends new blocks at the inode's i_size, while the
 * whole-block loop assumes they land at the handle's fpos. If the two
 * disagree (fpos past EOF, e.g. after the file was truncated through another
 * handle) the loop maps more blocks than the request covers, so
 * block_size * fblock_count exceeds the remaining size, size underflows and
 * the device write is handed a pointer past the end of the caller's buffer.
 *
 * The test also covers the reporter's workload: journal enabled, a large file
 * written in 32 KiB chunks, on 4 KiB block ext4 and on 1 KiB block ext3
 * (indirect block mapping) with odd chunk sizes. All data is read back and
 * the images are checked with e2fsck afterwards.
 */

#include "test_util.h"

#include <fcntl.h>
#include <string.h>

#define BIG_FILE TEST_MP "big.bin"
#define STALE_FILE TEST_MP "stale.bin"

static uint8_t pattern_byte(uint64_t pos)
{
	return (uint8_t)((pos * 2654435761u) >> 13);
}

static void fill_pattern(uint8_t *buf, uint64_t pos, size_t len)
{
	for (size_t i = 0; i < len; i++)
		buf[i] = pattern_byte(pos + i);
}

static void check_pattern(const uint8_t *buf, uint64_t pos, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != pattern_byte(pos + i)) {
			fprintf(stderr, "data mismatch at offset %llu\n",
				(unsigned long long)(pos + i));
			exit(1);
		}
	}
}

static void run_fsck(const char *image)
{
	char cmd[512];

	snprintf(cmd, sizeof(cmd),
		 "PATH=\"$PATH:/sbin:/usr/sbin\" e2fsck -fn '%s' >&2", image);
	TEST_ASSERT_EQ(0, system(cmd));
}

/*
 * Write @total bytes in chunks cycling through @chunks[], then read the file
 * back with a different chunking and verify the contents.
 */
static void write_and_verify(const size_t *chunks, size_t nchunks,
			     uint64_t total)
{
	const size_t max_chunk = 65536;
	uint8_t *buf = malloc(max_chunk);
	uint64_t pos = 0;
	size_t i = 0;
	size_t cnt;
	ext4_file f;

	TEST_ASSERT(buf);
	TEST_ASSERT_EQ(EOK, ext4_fopen2(&f, BIG_FILE,
					O_WRONLY | O_CREAT | O_TRUNC));
	while (pos < total) {
		size_t len = chunks[i++ % nchunks];

		if (len > total - pos)
			len = (size_t)(total - pos);
		TEST_ASSERT(len <= max_chunk);
		fill_pattern(buf, pos, len);
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, buf, len, &cnt));
		TEST_ASSERT_EQ(len, cnt);
		pos += len;
		TEST_ASSERT_EQ(pos, ext4_ftell(&f));
	}
	TEST_ASSERT_EQ(total, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, BIG_FILE, "rb"));
	TEST_ASSERT_EQ(total, ext4_fsize(&f));
	pos = 0;
	while (pos < total) {
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, 50000, &cnt));
		TEST_ASSERT(cnt > 0);
		check_pattern(buf, pos, cnt);
		pos += cnt;
	}
	TEST_ASSERT_EQ(total, pos);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	free(buf);
}

/*
 * Handle A has written 64 KiB; handle B truncates the file to 4 KiB, leaving
 * A's position beyond EOF. A subsequent 32 KiB write through A must fail
 * cleanly instead of mapping more blocks than it has data for.
 */
static void stale_position_write(void)
{
	const size_t len = 32768;
	uint8_t *buf = malloc(len);
	ext4_file a, b;
	size_t cnt;

	TEST_ASSERT(buf);
	fill_pattern(buf, 0, len);

	TEST_ASSERT_EQ(EOK, ext4_fopen(&a, STALE_FILE, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&a, buf, len, &cnt));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&a, buf, len, &cnt));
	TEST_ASSERT_EQ(65536, ext4_ftell(&a));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&b, STALE_FILE, "r+"));
	TEST_ASSERT_EQ(EOK, ext4_ftruncate(&b, 4096));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&b));

	/* buf is a heap allocation of exactly len bytes: a runaway block
	 * count makes the device write read past its end. */
	cnt = 1;
	TEST_ASSERT_EQ(EINVAL, ext4_fwrite(&a, buf, len, &cnt));
	TEST_ASSERT_EQ(0, cnt);
	TEST_ASSERT_EQ(65536, ext4_ftell(&a));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&a));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&b, STALE_FILE, "rb"));
	TEST_ASSERT_EQ(4096, ext4_fsize(&b));
	TEST_ASSERT_EQ(EOK, ext4_fread(&b, buf, len, &cnt));
	TEST_ASSERT_EQ(4096, cnt);
	check_pattern(buf, 0, cnt);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&b));
	free(buf);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char ext3_image[256];
	static const size_t reporter_chunks[] = { 32768 };
	static const size_t odd_chunks[] = { 32768, 1, 4095, 12345, 65536,
					     1023, 2049, 33333 };

	snprintf(ext3_image, sizeof(ext3_image), "%s.ext3", image);

	/* Reporter's setup: ext4, journal started, write back cache on. */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, true));
	stale_position_write();
	write_and_verify(reporter_chunks, 1, 64ull << 20);
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, false));
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();
	run_fsck(image);

	/* 1 KiB blocks with indirect block mapping and unaligned writes. */
	TEST_ASSERT_EQ(EOK, test_mount(ext3_image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	write_and_verify(odd_chunks, sizeof(odd_chunks) / sizeof(odd_chunks[0]),
			 12ull << 20);
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();
	run_fsck(ext3_image);

	return 0;
}
