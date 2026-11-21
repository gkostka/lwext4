/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Journal checksums (journal_checksum_v2/v3) as lwext4 writes them. The
 * journal is big endian on disk, and so are its checksums, but lwext4
 * wrote the commit block checksum in host byte order, and hashed the
 * transaction sequence number in host byte order into the checksum of
 * every journalled block (and truncated that checksum to 16 bits the wrong
 * way for v2). On little endian machines every transaction lwext4 wrote
 * failed its checksums: e2fsck and Linux refuse to replay them ("journal
 * transaction was corrupt, replay was aborted"), lwext4 skipped them too,
 * so a power cut lost everything the journal was there to protect.
 *
 * lwext4 writes transactions with the write back cache, and the image is
 * copied while they are only in the journal, as if the power had been
 * cut. The copy's journal is checked here block by block against the
 * checksums as jbd2 defines them (crc32c, seeded with the journal UUID,
 * over big endian fields), then replayed with ext4_recover(): the files
 * must be there.
 *
 * Replaying checksummed transactions that debugfs wrote, with a revoke
 * record, worked before and must keep working.
 */

#include "test_util.h"

#include <ext4_crc32.h>

#include <string.h>

#define BS 1024
#define JBLOCKS 1024
#define FILES 8

static uint8_t journal[JBLOCKS][BS];

static uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static uint16_t be16(const uint8_t *p)
{
	return (uint16_t)(p[0] << 8 | p[1]);
}

static void copy_file(const char *from, const char *to)
{
	static char buf[65536];
	FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
	size_t n;

	TEST_ASSERT(in && out);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
	fclose(in);
	TEST_ASSERT_EQ(0, fclose(out));
}

/* The journal of copy, which is at the same place as in image */
static void read_journal(const char *copy, const char *image)
{
	char path[512];
	unsigned long start;
	FILE *f;

	snprintf(path, sizeof(path), "%s.jstart", image);
	f = fopen(path, "r");
	TEST_ASSERT(f && fscanf(f, "%lu", &start) == 1);
	fclose(f);
	f = fopen(copy, "rb");
	TEST_ASSERT(f);
	TEST_ASSERT(fseek(f, (long)start * BS, SEEK_SET) == 0);
	TEST_ASSERT_EQ(JBLOCKS, fread(journal, BS, JBLOCKS, f));
	fclose(f);
}

/* crc32c over a block with the 4 checksum bytes at off zeroed */
static uint32_t csum_zeroed(uint32_t seed, const uint8_t *blk, int off, int len)
{
	static uint8_t tmp[BS];

	memcpy(tmp, blk, BS);
	memset(tmp + off, 0, len);
	return ext4_crc32c(seed, tmp, BS);
}

/* Check every checksum of the transactions in the journal; returns the
 * number of complete transactions. */
static int check_journal(int ver)
{
	const uint8_t *sb = journal[0];
	uint32_t first = be32(sb + 20), seq = be32(sb + 24), blk = be32(sb + 28);
	uint32_t incompat = be32(sb + 40), seed;
	int tag_size, commits = 0;

	TEST_ASSERT_EQ(0xc03b3998, be32(sb));
	TEST_ASSERT_EQ(ver == 3 ? 0x10 : 0x08, incompat & 0x18);
	seed = ext4_crc32c(EXT4_CRC32_INIT, sb + 48, 16);
	/* journal_tag_bytes() of jbd2: v2 tags have 2 more bytes */
	if (ver == 3)
		tag_size = 16;
	else
		tag_size = (incompat & 0x2 ? 12 : 8) + 2;
	TEST_ASSERT(blk != 0); /* transactions to replay */

	for (;;) {
		const uint8_t *b = journal[blk];
		uint32_t type;

		if (be32(b) != 0xc03b3998 || be32(b + 8) != seq)
			break;
		type = be32(b + 4);
		if (type == 1) { /* descriptor: tags, then the tail */
			int off = 12;
			bool last = false;

			TEST_ASSERT_EQ(be32(b + BS - 4),
				       csum_zeroed(seed, b, BS - 4, 4));
			while (!last) {
				const uint8_t *tag = b + off;
				uint32_t flags = ver == 3 ? be32(tag + 4)
							  : be16(tag + 6);
				uint8_t data[BS];
				uint32_t c, sq = seq;
				uint8_t sqb[4] = {sq >> 24, sq >> 16, sq >> 8, sq};

				blk = blk + 1 >= JBLOCKS ? first : blk + 1;
				memcpy(data, journal[blk], BS);
				if (flags & 1) { /* escaped */
					data[0] = 0xc0; data[1] = 0x3b;
					data[2] = 0x39; data[3] = 0x98;
				}
				c = ext4_crc32c(ext4_crc32c(seed, sqb, 4),
						data, BS);
				if (ver == 3)
					TEST_ASSERT_EQ(c, be32(tag + 12));
				else
					TEST_ASSERT_EQ(c & 0xffff, be16(tag + 4));
				off += tag_size;
				if (!(flags & 2))
					off += 16; /* UUID */
				last = flags & 8;
				TEST_ASSERT(off <= BS - 4);
			}
		} else if (type == 2) { /* commit */
			static uint8_t tmp[BS];

			memcpy(tmp, b, BS);
			tmp[12] = tmp[13] = 0; /* checksum type and size */
			memset(tmp + 16, 0, 4);
			TEST_ASSERT_EQ(be32(b + 16), ext4_crc32c(seed, tmp, BS));
			commits++;
			seq++;
		} else if (type == 5) { /* revoke */
			TEST_ASSERT_EQ(be32(b + BS - 4),
				       csum_zeroed(seed, b, BS - 4, 4));
		} else {
			break;
		}
		blk = blk + 1 >= JBLOCKS ? first : blk + 1;
	}
	return commits;
}

static void run(const char *image, int ver)
{
	char crash[512], path[32];
	char data[3000], back[3000];
	ext4_file f;
	size_t cnt;

	/* Transactions only in the journal at the copy. */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, true));
	for (int i = 0; i < FILES; i++) {
		snprintf(path, sizeof(path), TEST_MP "f%d", i);
		memset(data, 'a' + i, sizeof(data));
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, data, sizeof(data), &cnt));
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	}
	snprintf(crash, sizeof(crash), "%s.crash", image);
	copy_file(image, crash);
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, false));
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();

	read_journal(crash, image);
	TEST_ASSERT(check_journal(ver) >= FILES);

	/* The copy's journal replays: the files are there. */
	TEST_ASSERT_EQ(EOK, test_mount(crash, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	for (int i = 0; i < FILES; i++) {
		snprintf(path, sizeof(path), TEST_MP "f%d", i);
		memset(data, 'a' + i, sizeof(data));
		TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, back, sizeof(back), &cnt));
		TEST_ASSERT_EQ(sizeof(back), cnt);
		TEST_ASSERT(memcmp(back, data, cnt) == 0);
		TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	}
	test_umount();
}

/* Transactions written by debugfs: see test_journal_csum_order.sh. */
static void replay_debugfs(const char *image)
{
	static const char expect[] = "YXZX";
	char path[512], buf[BS];
	ext4_file f;
	size_t cnt;

	snprintf(path, sizeof(path), "%s.replay", image);
	TEST_ASSERT_EQ(EOK, test_mount(path, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "target", "rb"));
	for (int blk = 0; blk < 4; blk++) {
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, BS, &cnt));
		TEST_ASSERT_EQ(BS, cnt);
		for (int i = 0; i < BS; i++)
			TEST_ASSERT_EQ(expect[blk], buf[i]);
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char v2[512];

	replay_debugfs(image);

	run(image, 3);
	snprintf(v2, sizeof(v2), "%s.v2", image);
	run(v2, 2);
	return 0;
}
