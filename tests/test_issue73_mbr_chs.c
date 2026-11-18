/*
 * Regression test for issue #73: ext4_mbr_write produced wrong CHS values
 * (and, as a consequence of the same unit mix-up, wrong LBA start values for
 * every partition but the first).
 *
 * For several (sparse) image sizes and partition divisions, write an MBR with
 * ext4_mbr_write, then parse the partition table by hand and check it against
 * an independent reference: LBA ranges must be cylinder aligned, ordered,
 * non-overlapping and inside the disk, and the CHS fields must match the
 * standard LBA -> CHS translation for the disk geometry (saturating at
 * 1023/254/63 when the LBA is beyond CHS range). Finally ext4_mbr_scan must
 * read back the same partitions.
 */

/* The sparse images are larger than 2 GiB: 64-bit off_t on 32-bit hosts. */
#define _FILE_OFFSET_BITS 64

#include "test_util.h"

#include "../blockdev/linux/file_dev.h"

#include <ext4_mbr.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define SECTOR 512ULL
#define SPT 63 /* sectors per track */

struct chs {
	unsigned c, h, s;
};

static struct chs decode_chs(const uint8_t *b)
{
	struct chs r;

	r.h = b[0];
	r.s = b[1] & 0x3F;
	r.c = ((unsigned)(b[1] & 0xC0) << 2) | b[2];
	return r;
}

/* Standard LBA -> CHS, saturating at 1023/254/63 (FE FF FF). */
static struct chs ref_chs(uint64_t lba, unsigned heads)
{
	struct chs r;

	r.c = lba / ((uint64_t)heads * SPT);
	r.h = (lba / SPT) % heads;
	r.s = lba % SPT + 1;
	if (r.c > 1023) {
		r.c = 1023;
		r.h = 254;
		r.s = 63;
	}
	return r;
}

/* Conventional LBA-assist geometry: fewest heads out of 16/32/64/128/255
 * that keep the cylinder count within 1024. */
static unsigned ref_heads(uint64_t sectors)
{
	unsigned h;

	for (h = 16; h < 256; h *= 2)
		if (sectors / h / SPT <= 1024)
			return h;
	return 255;
}

static uint32_t le32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static int failures;

#define CHECK(cond, ...)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "  FAIL %s: ", #cond);                 \
			fprintf(stderr, __VA_ARGS__);                          \
			fprintf(stderr, "\n");                                 \
			failures++;                                            \
		}                                                              \
	} while (0)

static void check_chs(const char *what, int i, const uint8_t *raw,
		      uint64_t lba, unsigned heads)
{
	struct chs got = decode_chs(raw);
	struct chs exp = ref_chs(lba, heads);

	CHECK(got.c == exp.c && got.h == exp.h && got.s == exp.s,
	      "part %d %s (lba %llu): got C/H/S %u/%u/%u "
	      "(%02x %02x %02x), expected %u/%u/%u",
	      i, what, (unsigned long long)lba, got.c, got.h, got.s, raw[0],
	      raw[1], raw[2], exp.c, exp.h, exp.s);
}

static void run_case(const char *dir, uint64_t size, const uint8_t div[4])
{
	char path[1024];
	uint8_t mbr[512];
	struct ext4_mbr_parts parts;
	struct ext4_mbr_bdevs bdevs;
	uint64_t sectors = size / SECTOR;
	unsigned heads = ref_heads(sectors);
	uint64_t cyl = (uint64_t)heads * SPT;
	uint64_t prev_end = 1; /* sector 0 holds the MBR */
	int fd, i;

	fprintf(stderr, "size %llu MiB, division {%u,%u,%u,%u}, heads %u\n",
		(unsigned long long)(size >> 20), div[0], div[1], div[2],
		div[3], heads);

	snprintf(path, sizeof(path), "%s.%llu", dir,
		 (unsigned long long)size);
	fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	TEST_ASSERT(fd >= 0);
	TEST_ASSERT_EQ(0, ftruncate(fd, size));
	close(fd);

	memcpy(parts.division, div, 4);
	file_dev_name_set(path);
	TEST_ASSERT_EQ(EOK, ext4_mbr_write(file_dev_get(), &parts, 0x12345678));

	fd = open(path, O_RDONLY);
	TEST_ASSERT(fd >= 0);
	TEST_ASSERT_EQ(512, pread(fd, mbr, 512, 0));
	close(fd);

	TEST_ASSERT_EQ(0x55, mbr[510]);
	TEST_ASSERT_EQ(0xAA, mbr[511]);

	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(file_dev_get(), &bdevs));

	for (i = 0; i < 4; i++) {
		const uint8_t *pe = mbr + 446 + 16 * i;
		uint64_t start = le32(pe + 8);
		uint64_t len = le32(pe + 12);
		uint64_t want = sectors * div[i] / 100;

		if (!len) {
			CHECK(want < cyl, "part %d unexpectedly empty", i);
			CHECK(bdevs.partitions[i].part_size == 0,
			      "part %d: scan found empty entry", i);
			continue;
		}

		fprintf(stderr, "  part %d: start %llu len %llu\n", i,
			(unsigned long long)start, (unsigned long long)len);

		CHECK(pe[4] == 0x83, "part %d type 0x%02x", i, pe[4]);
		CHECK(start >= prev_end,
		      "part %d start %llu overlaps previous end %llu", i,
		      (unsigned long long)start,
		      (unsigned long long)prev_end);
		CHECK(start + len <= sectors,
		      "part %d end %llu beyond disk (%llu sectors)", i,
		      (unsigned long long)(start + len),
		      (unsigned long long)sectors);
		/* The first partition starts on track 1 of cylinder 0, all
		 * others on a cylinder boundary; all end on one. */
		CHECK(start == SPT || start % cyl == 0,
		      "part %d start %llu not cylinder aligned", i,
		      (unsigned long long)start);
		CHECK((start + len) % cyl == 0,
		      "part %d end %llu not cylinder aligned", i,
		      (unsigned long long)(start + len));
		CHECK(len <= want && len + 2 * cyl > want,
		      "part %d len %llu, expected about %llu", i,
		      (unsigned long long)len, (unsigned long long)want);

		check_chs("start", i, pe + 1, start, heads);
		check_chs("end", i, pe + 5, start + len - 1, heads);

		CHECK(bdevs.partitions[i].part_offset == start * SECTOR,
		      "part %d: scan offset %llu", i,
		      (unsigned long long)bdevs.partitions[i].part_offset);
		CHECK(bdevs.partitions[i].part_size == len * SECTOR,
		      "part %d: scan size %llu", i,
		      (unsigned long long)bdevs.partitions[i].part_size);

		prev_end = start + len;
	}

	unlink(path);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static const uint8_t d_std[4] = {50, 20, 10, 20};
	static const uint8_t d_quarter[4] = {25, 25, 25, 25};
	static const uint8_t d_gap[4] = {0, 40, 0, 60};
	static const uint64_t sizes[] = {
		8ULL << 20,	/* 16 heads */
		500ULL << 20,	/* 16 heads, ~1000 cylinders */
		1000ULL << 20,	/* 32 heads */
		3ULL << 30,	/* 128 heads */
		20ULL << 30,	/* 255 heads, CHS saturates */
	};
	size_t n;

	for (n = 0; n < sizeof(sizes) / sizeof(sizes[0]); n++) {
		run_case(image, sizes[n], d_std);
		run_case(image, sizes[n], d_quarter);
		run_case(image, sizes[n], d_gap);
	}

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	return 0;
}
