/*
 * Regression test for issue #86: journal replay must not walk from the
 * transactions of the current journal session into stale transactions left
 * in the log by an earlier session.
 *
 * Every session writes the log from its first block again, so the only thing
 * that separates new transactions from stale ones behind them is the
 * transaction ID. IDs therefore have to keep increasing across sessions,
 * otherwise a stale transaction that happens to carry the next expected ID is
 * replayed on top of newer metadata.
 *
 * Scenario:
 *   A: empty journal session (a plain mount/unmount cycle).
 *   B: remove "other", create some files, clean shutdown. B's transactions
 *      stay in the log after they have been checkpointed.
 *   C: remove "victim" (same shape as the first transaction of B), then the
 *      power is cut before the journal is stopped.
 * Replaying the journal of the power cut image must remove "victim" and must
 * not resurrect it by replaying B's stale transactions.
 */

#include "test_util.h"

#include <string.h>

#define CREATED_FILES 8

static void copy_file(const char *from, const char *to)
{
	char buf[4096];
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

static void journal_mount(const char *image)
{
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
}

static void journal_umount(void)
{
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();
}

static void create_file(const char *path)
{
	ext4_file f;
	size_t wcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "data\n", 5, &wcnt));
	TEST_ASSERT_EQ(5, wcnt);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void check_state(void)
{
	char path[64];
	int i;

	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "victim",
						EXT4_DE_REG_FILE));
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "other",
						EXT4_DE_REG_FILE));
	for (i = 0; i < CREATED_FILES; i++) {
		snprintf(path, sizeof(path), TEST_MP "new%d", i);
		TEST_ASSERT_EQ(EOK, ext4_inode_exist(path, EXT4_DE_REG_FILE));
	}
}

/**@brief Run e2fsck -fn on @image if it is available.*/
static void fsck_image(const char *image)
{
	char cmd[1024];
	int r;

	snprintf(cmd, sizeof(cmd),
		 "PATH=\"$PATH:/sbin:/usr/sbin\"; "
		 "command -v e2fsck >/dev/null || exit 0; "
		 "e2fsck -fn '%s' >/dev/null 2>&1", image);
	r = system(cmd);
	if (r != 0)
		fprintf(stderr, "e2fsck -fn %s failed (%d)\n", image, r);
	TEST_ASSERT_EQ(0, r);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char crash[1024];
	char path[64];
	int i;

	snprintf(crash, sizeof(crash), "%s.crash", image);

	/* A: a journal session without any transaction. */
	journal_mount(image);
	journal_umount();

	/* B: some transactions, clean shutdown. */
	journal_mount(image);
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "other"));
	for (i = 0; i < CREATED_FILES; i++) {
		snprintf(path, sizeof(path), TEST_MP "new%d", i);
		create_file(path);
	}
	journal_umount();

	/* C: remove "victim", then cut the power before the journal is
	 * stopped. */
	journal_mount(image);
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "victim"));
	copy_file(image, crash);
	journal_umount();

	printf("checking the cleanly unmounted image\n");
	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	check_state();
	test_umount();
	fsck_image(image);

	/* Replay the journal of the power cut image. */
	printf("checking the power cut image after journal replay\n");
	TEST_ASSERT_EQ(EOK, test_mount(crash, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	check_state();
	test_umount();
	fsck_image(crash);

	return 0;
}
