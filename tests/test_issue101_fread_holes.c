/*
 * Issue #101: ext4_fread must return zeros for holes in sparse files. It used
 * to read the device's first blocks (superblock included) for every hole
 * that covered a whole block of the requested range, and for a hole in the
 * trailing partial block.
 *
 * The setup script builds sparse.bin with whole-block holes between data
 * blocks and a hole at the tail. The file is read through lwext4 with a
 * range of start offsets and chunk sizes and compared against the host copy
 * mke2fs populated the image from.
 */

#include "test_util.h"

#include <string.h>

#define FILE_NAME "sparse.bin"
#define MAX_SIZE (64 * 1024)

static uint8_t expected[MAX_SIZE];
static uint8_t got[MAX_SIZE];

static size_t load_expected(const char *image)
{
	char path[4096];
	FILE *fp;
	size_t n;

	snprintf(path, sizeof(path), "%s.d/" FILE_NAME, image);
	fp = fopen(path, "rb");
	TEST_ASSERT(fp != NULL);
	n = fread(expected, 1, sizeof(expected), fp);
	TEST_ASSERT(feof(fp));
	fclose(fp);
	return n;
}

static void check_range(size_t fsize, size_t start, size_t chunk)
{
	ext4_file f;
	size_t pos, rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP FILE_NAME, "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, start, SEEK_SET));

	/* Poison the buffer so a missing zero-fill cannot go unnoticed. */
	memset(got, 0xa5, sizeof(got));
	for (pos = start; pos < fsize; pos += rcnt) {
		size_t len = chunk;

		if (len > fsize - pos)
			len = fsize - pos;
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, got + pos, len, &rcnt));
		TEST_ASSERT_EQ(len, rcnt);
	}

	/* Reading at EOF returns nothing. */
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, got, 1, &rcnt));
	TEST_ASSERT_EQ(0, rcnt);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	for (pos = start; pos < fsize; pos++) {
		if (got[pos] != expected[pos]) {
			fprintf(stderr,
				"start %zu chunk %zu: offset %zu: expected "
				"0x%02x, got 0x%02x\n",
				start, chunk, pos, expected[pos], got[pos]);
			TEST_ASSERT(got[pos] == expected[pos]);
		}
	}
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static const size_t starts[] = {
		0, 1, 4095, 4096, 4097, 3 * 4096 - 1, 5 * 4096 + 17,
		10 * 4096 + 1500, 14 * 4096, 14 * 4096 + 50,
	};
	static const size_t chunks[] = {
		1, 100, 4095, 4096, 4097, 3 * 4096 + 17, 10000, MAX_SIZE,
	};
	size_t fsize, i, j;

	fsize = load_expected(image);
	TEST_ASSERT_EQ(14 * 4096 + 100, fsize);

	TEST_ASSERT_EQ(EOK, test_mount(image, true));

	for (i = 0; i < sizeof(starts) / sizeof(starts[0]); i++)
		for (j = 0; j < sizeof(chunks) / sizeof(chunks[0]); j++)
			check_range(fsize, starts[i], chunks[j]);

	test_umount();
	return 0;
}
