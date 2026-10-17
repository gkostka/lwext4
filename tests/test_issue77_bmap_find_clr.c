/*
 * Issue #77: ext4_bmap_bit_find_clr returned the start bit instead of the
 * clear bit it found while scanning the leading, not byte aligned part of
 * the range.
 *
 * Pure unit test: compares ext4_bmap_bit_find_clr and ext4_bmap_bits_free
 * against brute force reference implementations for every (sbit, ebit)
 * range over small bitmaps filled with structured and random patterns.
 * The image path argument is ignored.
 */

#include "test_util.h"

#include <ext4_bitmap.h>

#include <string.h>

#define MAX_BYTES 12
#define MAX_BITS (MAX_BYTES * 8)

static int ref_find_clr(const uint8_t *bmap, uint32_t sbit, uint32_t ebit,
			uint32_t *bit_id)
{
	uint32_t i;

	for (i = sbit; i < ebit; ++i) {
		if (!(bmap[i >> 3] & (1 << (i & 7)))) {
			*bit_id = i;
			return EOK;
		}
	}
	return ENOSPC;
}

static void ref_bits_free(uint8_t *bmap, uint32_t sbit, uint32_t bcnt)
{
	uint32_t i;

	for (i = sbit; i < sbit + bcnt; ++i)
		bmap[i >> 3] &= ~(1 << (i & 7));
}

static uint32_t rng_state = 0x12345678;

static uint32_t rng(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

static void fill_pattern(uint8_t *bmap, size_t len, unsigned pattern)
{
	size_t i;

	for (i = 0; i < len; ++i) {
		switch (pattern) {
		case 0:
			bmap[i] = 0x00;
			break;
		case 1:
			bmap[i] = 0xFF;
			break;
		case 2:
			bmap[i] = 0xAA;
			break;
		case 3:
			bmap[i] = 0x55;
			break;
		default: {
			/* Mostly set bitmaps with a few clear bits. */
			uint8_t v = 0xFF;
			if ((rng() & 3) == 0)
				v &= ~(1 << (rng() & 7));
			if (pattern & 1)
				v = rng();
			bmap[i] = v;
		}
		}
	}
}

/* Exact sized heap buffer so ASan catches any access outside the bitmap. */
static uint8_t *dup_bmap(const uint8_t *src, size_t len)
{
	uint8_t *p = malloc(len);

	TEST_ASSERT(p != NULL);
	memcpy(p, src, len);
	return p;
}

static void check_find_clr(const uint8_t *bmap, size_t len, uint32_t sbit,
			   uint32_t ebit)
{
	uint32_t exp_id = 0xdeadbeef, got_id = 0xdeadbeef;
	int exp_rc = ref_find_clr(bmap, sbit, ebit, &exp_id);
	uint8_t *copy = dup_bmap(bmap, len);
	int got_rc = ext4_bmap_bit_find_clr(copy, sbit, ebit, &got_id);

	if (got_rc != exp_rc || (exp_rc == EOK && got_id != exp_id)) {
		size_t i;

		fprintf(stderr,
			"find_clr(sbit=%u, ebit=%u): expected rc=%d bit=%u, "
			"got rc=%d bit=%u\nbitmap:",
			sbit, ebit, exp_rc, exp_rc == EOK ? exp_id : 0,
			got_rc, got_rc == EOK ? got_id : 0);
		for (i = 0; i < len; ++i)
			fprintf(stderr, " %02x", bmap[i]);
		fprintf(stderr, "\n");
		exit(1);
	}
	TEST_ASSERT(memcmp(copy, bmap, len) == 0);
	free(copy);
}

static void check_bits_free(const uint8_t *bmap, size_t len, uint32_t sbit,
			    uint32_t bcnt)
{
	uint8_t exp[MAX_BYTES];
	uint8_t *got = dup_bmap(bmap, len);

	memcpy(exp, bmap, len);
	ref_bits_free(exp, sbit, bcnt);
	ext4_bmap_bits_free(got, sbit, bcnt);
	if (memcmp(exp, got, len) != 0) {
		fprintf(stderr, "bits_free(sbit=%u, bcnt=%u) mismatch\n", sbit,
			bcnt);
		exit(1);
	}
	free(got);
}

static void check_all_ranges(const uint8_t *bmap, size_t len)
{
	uint32_t nbits = len * 8;
	uint32_t sbit, ebit;

	for (sbit = 0; sbit <= nbits; ++sbit) {
		for (ebit = sbit; ebit <= nbits; ++ebit) {
			check_find_clr(bmap, len, sbit, ebit);
			check_bits_free(bmap, len, sbit, ebit - sbit);
		}
	}
}

int main(void)
{
	uint8_t bmap[MAX_BYTES];
	uint32_t id;
	unsigned pattern, round, bit;
	size_t len;

	/* The exact case from the issue: bit 1 set, bit 2 clear. */
	memset(bmap, 0xFF, sizeof(bmap));
	ext4_bmap_bit_clr(bmap, 2);
	TEST_ASSERT_EQ(EOK, ext4_bmap_bit_find_clr(bmap, 1, 8, &id));
	TEST_ASSERT_EQ(2, id);

	/* An empty or inverted range has no clear bits and must not scan. */
	memset(bmap, 0x00, sizeof(bmap));
	TEST_ASSERT_EQ(ENOSPC, ext4_bmap_bit_find_clr(bmap, 5, 5, &id));
	TEST_ASSERT_EQ(ENOSPC, ext4_bmap_bit_find_clr(bmap, 9, 3, &id));

	/* A single clear bit anywhere, searched from every start bit. */
	for (bit = 0; bit < MAX_BITS; ++bit) {
		memset(bmap, 0xFF, sizeof(bmap));
		ext4_bmap_bit_clr(bmap, bit);
		check_all_ranges(bmap, sizeof(bmap));
	}

	/* Structured and random patterns over various bitmap sizes. */
	for (len = 1; len <= MAX_BYTES; ++len) {
		for (pattern = 0; pattern < 4; ++pattern) {
			fill_pattern(bmap, len, pattern);
			check_all_ranges(bmap, len);
		}
		for (round = 0; round < 64; ++round) {
			fill_pattern(bmap, len, 4 + (round & 1));
			check_all_ranges(bmap, len);
		}
	}

	return 0;
}
