/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Library unit tests that need no block device: byte order helpers,
 * checksums, bitmaps and the htree name hash. The hash reference values come
 * from e2fsprogs (debugfs dx_hash, see mkimages.py), everything else from the
 * published algorithms. They are cheap enough to run on 8/16-bit targets.
 */

#include <ext4.h>
#include <ext4_bitmap.h>
#include <ext4_block_group.h>
#include <ext4_crc32.h>
#include <ext4_hash.h>
#include <ext4_misc.h>
#include <ext4_types.h>

#include <stdint.h>
#include <string.h>

#include "check.h"
#include "testdata.h"

static void byte_order(void)
{
	static const uint8_t bytes[8] = {0x01, 0x23, 0x45, 0x67,
					 0x89, 0xab, 0xcd, 0xef};
	union {
		uint32_t u32;
		uint8_t b[4];
	} probe = {.u32 = 0x01020304};
	uint16_t v16;
	uint32_t v32;
	uint64_t v64;
	uint8_t raw[4];

	ASSERT((probe.b[0] == 0x01) == (CONFIG_BIG_ENDIAN != 0));

	memcpy(&v16, bytes, sizeof(v16));
	memcpy(&v32, bytes, sizeof(v32));
	memcpy(&v64, bytes, sizeof(v64));
	ASSERT(to_le16(v16) == 0x2301);
	ASSERT(to_le32(v32) == 0x67452301UL);
	ASSERT(to_le64(v64) == 0xefcdab8967452301ULL);
	ASSERT(to_be16(v16) == 0x0123);
	ASSERT(to_be32(v32) == 0x01234567UL);
	ASSERT(to_be64(v64) == 0x0123456789abcdefULL);

	v32 = to_le32(0x11223344UL);
	memcpy(raw, &v32, 4);
	ASSERT(raw[0] == 0x44 && raw[1] == 0x33 && raw[2] == 0x22 &&
	       raw[3] == 0x11);
	v32 = to_be32(JBD_MAGIC_NUMBER);
	memcpy(raw, &v32, 4);
	ASSERT(raw[0] == 0xc0 && raw[1] == 0x3b && raw[2] == 0x39 &&
	       raw[3] == 0x98);
}

static void checksums(void)
{
	static const char check[] = "123456789";

	/* The CRC-32C check value is defined with a final inversion, the
	 * ext4 helper leaves it to the caller. */
	ASSERT(~ext4_crc32c(0xffffffffUL, check, 9) == 0xe3069283UL);
#if CONFIG_JOURNALING_ENABLE
	/* CRC-32 (journal checksum v1) */
	ASSERT(~ext4_crc32(0xffffffffUL, check, 9) == 0xcbf43926UL);
#endif
	/* CRC-16/MODBUS (reflected 0x8005, init 0xffff) */
	ASSERT(ext4_bg_crc16(0xffff, (const uint8_t *)check, 9) == 0x4b37);
	/* Checksums chain across calls. */
	ASSERT(ext4_crc32c(ext4_crc32c(0xffffffffUL, check, 4), check + 4,
			   5) == ext4_crc32c(0xffffffffUL, check, 9));
}

static void bitmaps(void)
{
	uint8_t bmap[40];
	uint32_t bit;

	memset(bmap, 0, sizeof(bmap));
	ext4_bmap_bit_set(bmap, 0);
	ext4_bmap_bit_set(bmap, 9);
	ext4_bmap_bit_set(bmap, 300);
	ASSERT(bmap[0] == 0x01 && bmap[1] == 0x02 && bmap[37] == 0x10);
	ASSERT(ext4_bmap_is_bit_set(bmap, 300));
	ASSERT(ext4_bmap_is_bit_clr(bmap, 301));
	ext4_bmap_bit_clr(bmap, 9);
	ASSERT(bmap[1] == 0);

	memset(bmap, 0xff, sizeof(bmap));
	ext4_bmap_bit_clr(bmap, 277);
	CHECK(ext4_bmap_bit_find_clr(bmap, 3, 320, &bit));
	ASSERT(bit == 277);
	ASSERT(ext4_bmap_bit_find_clr(bmap, 3, 270, &bit) == ENOSPC);

	ext4_bmap_bits_free(bmap, 17, 250);
	CHECK(ext4_bmap_bit_find_clr(bmap, 0, 320, &bit));
	ASSERT(bit == 17);
	ASSERT(ext4_bmap_is_bit_set(bmap, 16));
	ASSERT(ext4_bmap_is_bit_clr(bmap, 266));
	ASSERT(ext4_bmap_is_bit_set(bmap, 267));
}

struct hash_case {
	const char *name;
	uint8_t version;
	uint8_t seeded;
	uint32_t major;
	uint32_t minor;
};

#define HASH_CASE(n, v, s, ma, mi) {n, v, s, ma, mi},
static const struct hash_case hash_cases[] = {HASH_LIST(HASH_CASE)};

static void htree_hash(void)
{
	static const uint8_t seed_bytes[16] = HASH_SEED_BYTES;
	uint32_t seed[4];
	uint32_t major, minor;
	size_t i;

	/* The on-disk seed is used as native 32-bit words, like e2fsprogs. */
	memcpy(seed, seed_bytes, sizeof(seed));
	for (i = 0; i < sizeof(hash_cases) / sizeof(hash_cases[0]); i++) {
		const struct hash_case *c = &hash_cases[i];

		minor = 0;
		CHECK(ext2_htree_hash(c->name, (int)strlen(c->name),
				      c->seeded ? seed : NULL, c->version,
				      &major, &minor));
		if (major != c->major || minor != c->minor) {
			printf("hash case %u: version %u seeded %u: "
			       "%08lx/%08lx, e2fsprogs %08lx/%08lx\n",
			       (unsigned)i, c->version, c->seeded,
			       (unsigned long)major, (unsigned long)minor,
			       (unsigned long)c->major,
			       (unsigned long)c->minor);
			ASSERT(0);
		}
	}
}

void unit_tests(void)
{
	byte_order();
	checksums();
	bitmaps();
	htree_hash();
	printf("unit tests: ok\n");
}
