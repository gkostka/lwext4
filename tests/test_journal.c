/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Journalled operations and their replay. One journal session on an ext4
 * image with a small (1 MiB) journal runs transactions of several kinds:
 * creating, writing, renaming and removing files, extended attributes in
 * the inode and in xattr blocks (removing their files frees the blocks:
 * revoke records), truncation; far more transactions than fit into the
 * journal, so that it wraps and is checkpointed. The first round runs with
 * the write through cache, where every transaction is checkpointed at
 * once; the others with the write back cache, where committed transactions
 * stay in the journal until their blocks are written back. Twice in that
 * window the image is copied, as if the power had been cut there: the
 * copies have unattached inodes and missing entries until their journal is
 * replayed. Afterwards each copy is mounted, its journal replayed with
 * ext4_recover() and the files (names, sizes, data, attributes) are
 * compared with the state at the moment of the copy. The check script runs
 * e2fsck on the cleanly unmounted image and on both replayed copies.
 *
 * One session on a fresh image: stale transactions of earlier sessions are
 * the subject of test_journal_replay (issue #86).
 *
 * red-green: guard (coverage test, passes on the base)
 */
#include "test_util.h"

#include <string.h>

#define FILES 48
#define ROUNDS 3

static void copy_file(const char *from, const char *to)
{
	static char buf[65536];
	size_t n;
	FILE *in = fopen(from, "rb");
	FILE *out = fopen(to, "wb");

	TEST_ASSERT(in && out);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
	TEST_ASSERT(!ferror(in));
	TEST_ASSERT_EQ(0, fclose(out));
	fclose(in);
}

static void name(char *path, size_t size, const char *what, int i)
{
	snprintf(path, size, TEST_MP "%s%02d", what, i);
}

static void fill(char *buf, size_t len, int seed)
{
	for (size_t k = 0; k < len; k++)
		buf[k] = (char)('A' + (seed * 7 + k) % 23);
}

static size_t file_len(int i)
{
	/* Small, then some that need an indirect/extent block's worth */
	return i % 4 == 3 ? 5000 + (size_t)i * 300 : 10 + (size_t)i;
}

static void write_file(const char *path, int seed, size_t len)
{
	static char buf[65536];
	ext4_file f;
	size_t n;

	TEST_ASSERT(len <= sizeof(buf));
	fill(buf, len, seed);
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, buf, len, &n));
	TEST_ASSERT_EQ(len, n);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void expect_file(const char *path, int seed, size_t len)
{
	static char want[65536], got[65536];
	ext4_file f;
	size_t n;
	int r = ext4_fopen(&f, path, "rb");

	if (r != EOK)
		fprintf(stderr, "open %s: %d\n", path, r);
	TEST_ASSERT_EQ(EOK, r);
	fill(want, len, seed);
	TEST_ASSERT_EQ(len, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, got, sizeof(got), &n));
	TEST_ASSERT_EQ(len, n);
	TEST_ASSERT(memcmp(want, got, len) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void truncate_to(const char *path, uint64_t size)
{
	ext4_file f;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "r+b"));
	TEST_ASSERT_EQ(EOK, ext4_ftruncate(&f, size));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

/*
 * Round r: create FILES files, give every fifth a small attribute (inode
 * body) and a large one (xattr block), rename the even ones, remove every
 * third (freeing xattr blocks: revoke records), truncate every fourth (the
 * large ones) to 100 bytes. Each call is its own transaction.
 */
static void round_ops(int r)
{
	char path[64], path2[64], what[16];
	static char big[300];

	snprintf(what, sizeof(what), "r%d-", r);
	for (int i = 0; i < FILES; i++) {
		name(path, sizeof(path), what, i);
		write_file(path, r * 100 + i, file_len(i));
	}
	for (int i = 0; i < FILES; i++) {
		name(path, sizeof(path), what, i);
		if (i % 5 == 0) {
			fill(big, sizeof(big), r * 100 + i);
			TEST_ASSERT_EQ(EOK, ext4_setxattr(path, "user.round",
							  10, what,
							  strlen(what)));
			TEST_ASSERT_EQ(EOK, ext4_setxattr(path, "user.big", 8,
							  big, sizeof(big)));
		}
		if (i % 2 == 0) {
			snprintf(path2, sizeof(path2), "%s.moved", path);
			TEST_ASSERT_EQ(EOK, ext4_frename(path, path2));
			strcpy(path, path2);
		}
		if (i % 3 == 0) {
			TEST_ASSERT_EQ(EOK, ext4_fremove(path));
			continue;
		}
		if (i % 4 == 3)
			truncate_to(path, 100);
	}
}

static void check_round(int r)
{
	char path[64], what[16], buf[300];
	static char big[300];
	size_t len;

	snprintf(what, sizeof(what), "r%d-", r);
	for (int i = 0; i < FILES; i++) {
		name(path, sizeof(path), what, i);
		if (i % 2 == 0)
			strcat(path, ".moved");
		if (i % 3 == 0) {
			TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(path,
							EXT4_DE_REG_FILE));
			continue;
		}
		expect_file(path, r * 100 + i,
			    i % 4 == 3 ? 100 : file_len(i));
		if (i % 5 == 0) {
			TEST_ASSERT_EQ(EOK, ext4_getxattr(path, "user.round",
							  10, buf, sizeof(buf),
							  &len));
			TEST_ASSERT_EQ(strlen(what), len);
			TEST_ASSERT(memcmp(buf, what, len) == 0);
			fill(big, sizeof(big), r * 100 + i);
			TEST_ASSERT_EQ(EOK, ext4_getxattr(path, "user.big", 8,
							  buf, sizeof(buf),
							  &len));
			TEST_ASSERT_EQ(sizeof(big), len);
			TEST_ASSERT(memcmp(buf, big, len) == 0);
		}
	}
}

/* The state after @rounds complete rounds and nothing of the next one */
static void check_state(int rounds)
{
	char path[64], what[16];

	for (int r = 0; r < rounds; r++)
		check_round(r);
	snprintf(what, sizeof(what), "r%d-", rounds);
	name(path, sizeof(path), what, 1);
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(path, EXT4_DE_REG_FILE));
}

static void replay(const char *image, int rounds)
{
	printf("== replaying %s\n", image);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	check_state(rounds);
	test_umount();

	/* Nothing left to replay */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	check_state(rounds);
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char crash1[1024], crash2[1024];

	snprintf(crash1, sizeof(crash1), "%s.crash1", image);
	snprintf(crash2, sizeof(crash2), "%s.crash2", image);

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	round_ops(0);

	/* With the write back cache, committed transactions stay in the
	 * journal until their blocks are written back (checkpointed): the
	 * copies need their journal replayed. */
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, true));
	round_ops(1);
	copy_file(image, crash1);
	for (int r = 2; r < ROUNDS; r++)
		round_ops(r);
	copy_file(image, crash2);
	TEST_ASSERT_EQ(EOK, ext4_cache_write_back(TEST_MP, false));
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	check_state(ROUNDS);
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check_state(ROUNDS);
	test_umount();

	replay(crash1, 2);
	replay(crash2, ROUNDS);
	return 0;
}
