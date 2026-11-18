/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Byte order helpers: CONFIG_BIG_ENDIAN must match the machine the test runs
 * on, and the ext4 (little endian) / jbd2 (big endian) accessors must produce
 * the on-disk byte layout. Needs no image, so it runs on every target the
 * harness supports, including the qemu-user cross builds.
 */

#include "test_util.h"

#include <ext4_misc.h>
#include <ext4_types.h>

#include <string.h>

#if defined(CONFIG_BIG_ENDIAN) && (CONFIG_BIG_ENDIAN + 0)
#define CONFIGURED_BIG_ENDIAN 1
#else
#define CONFIGURED_BIG_ENDIAN 0
#endif

static const uint8_t bytes[8] = {0x01, 0x23, 0x45, 0x67,
				 0x89, 0xab, 0xcd, 0xef};

int main(void)
{
	union {
		uint32_t u32;
		uint8_t b[4];
	} probe = {.u32 = 0x01020304};
	uint16_t v16;
	uint32_t v32;
	uint64_t v64;
	struct ext4_sblock sb;
	struct jbd_bhdr jh;
	const uint8_t *raw;

	/* The configured byte order is the one of the machine. */
	TEST_ASSERT_EQ(probe.b[0] == 0x01, CONFIGURED_BIG_ENDIAN);

	/* Raw little endian bytes decode through to_le*() ... */
	memcpy(&v16, bytes, sizeof(v16));
	memcpy(&v32, bytes, sizeof(v32));
	memcpy(&v64, bytes, sizeof(v64));
	TEST_ASSERT_EQ(0x2301, to_le16(v16));
	TEST_ASSERT_EQ(0x67452301UL, to_le32(v32));
	TEST_ASSERT(to_le64(v64) == 0xefcdab8967452301ULL);

	/* ... and raw big endian bytes through to_be*(). */
	TEST_ASSERT_EQ(0x0123, to_be16(v16));
	TEST_ASSERT_EQ(0x01234567UL, to_be32(v32));
	TEST_ASSERT(to_be64(v64) == 0x0123456789abcdefULL);

	/* Round trips. */
	TEST_ASSERT_EQ(0xbeef, to_le16(to_le16(0xbeef)));
	TEST_ASSERT_EQ(0xdeadbeefUL, to_le32(to_le32(0xdeadbeefUL)));
	TEST_ASSERT(to_be64(to_be64(0x0011223344556677ULL)) ==
		    0x0011223344556677ULL);

	/* ext4 metadata is little endian on disk. */
	memset(&sb, 0, sizeof(sb));
	ext4_set16(&sb, magic, EXT4_SUPERBLOCK_MAGIC);
	ext4_set32(&sb, blocks_count_lo, 0x11223344UL);
	raw = (const uint8_t *)&sb.magic;
	TEST_ASSERT(raw[0] == 0x53 && raw[1] == 0xef);
	raw = (const uint8_t *)&sb.blocks_count_lo;
	TEST_ASSERT(raw[0] == 0x44 && raw[1] == 0x33 && raw[2] == 0x22 &&
		    raw[3] == 0x11);
	TEST_ASSERT_EQ(EXT4_SUPERBLOCK_MAGIC, ext4_get16(&sb, magic));
	TEST_ASSERT_EQ(0x11223344UL, ext4_get32(&sb, blocks_count_lo));

	/* jbd2 metadata is big endian on disk. */
	memset(&jh, 0, sizeof(jh));
	jbd_set32(&jh, magic, JBD_MAGIC_NUMBER);
	raw = (const uint8_t *)&jh.magic;
	TEST_ASSERT(raw[0] == 0xc0 && raw[1] == 0x3b && raw[2] == 0x39 &&
		    raw[3] == 0x98);
	TEST_ASSERT_EQ(JBD_MAGIC_NUMBER, jbd_get32(&jh, magic));

	printf("byte order: %s endian\n",
	       CONFIGURED_BIG_ENDIAN ? "big" : "little");
	return 0;
}
