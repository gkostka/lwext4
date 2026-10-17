/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * ext4_trunc_inode() always committed the caller's journal transaction
 * before truncating, so removing a directory (ext4_dir_rm) or a file
 * (ext4_fremove) was split over several transactions: the directory entry
 * disappeared in one, the inode was released in a later one. A power loss
 * in between left an allocated inode that no directory refers to (e2fsck:
 * "Unconnected directory inode").
 *
 * Simulate a power loss after every single block write of the removal (a
 * forked child stops writing and exits), let ext4_recover() replay the
 * journal and check that the entry and its inode are either both still
 * there or both gone.
 */

#include "test_util.h"

#include "../blockdev/linux/file_dev.h"

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int (*orig_bwrite)(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt);
static long write_limit = -1;
static long writes;

static int crash_bwrite(struct ext4_blockdev *bdev, const void *buf,
			uint64_t blk_id, uint32_t blk_cnt)
{
	if (write_limit >= 0 && writes >= write_limit)
		_exit(0); /* power loss: this and all later writes are lost */
	writes++;
	return orig_bwrite(bdev, buf, blk_id, blk_cnt);
}

static void copy_file(const char *from, const char *to)
{
	static char buf[65536];
	FILE *in = fopen(from, "rb");
	FILE *out = fopen(to, "wb");
	size_t n;

	TEST_ASSERT(in && out);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
	fclose(in);
	TEST_ASSERT_EQ(0, fclose(out));
}

static uint32_t free_inodes(void)
{
	struct ext4_mount_stats stats;

	TEST_ASSERT_EQ(EOK, ext4_mount_point_stats(TEST_MP, &stats));
	return stats.free_inodes_count;
}

static void mount_journal(const char *image)
{
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
}

static void umount_journal(void)
{
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();
}

struct removal {
	const char *path;
	int type;
	int (*remove)(const char *path);
};

/* Remove r->path with a power loss after `limit` block writes (-1: none).
 * Returns the number of block writes the removal needed. */
static long remove_with_power_loss(const char *image,
				   const struct removal *r, long limit)
{
	pid_t pid;
	int status;
	int fd[2];
	long n = -1;

	TEST_ASSERT_EQ(0, pipe(fd));
	pid = fork();
	TEST_ASSERT(pid >= 0);
	if (pid == 0) {
		close(fd[0]);
		mount_journal(image);
		writes = 0;
		write_limit = limit;
		TEST_ASSERT_EQ(EOK, r->remove(r->path));
		umount_journal();
		n = writes;
		TEST_ASSERT_EQ(sizeof(n), write(fd[1], &n, sizeof(n)));
		_exit(0);
	}
	close(fd[1]);
	if (read(fd[0], &n, sizeof(n)) != sizeof(n))
		n = -1;
	close(fd[0]);
	TEST_ASSERT_EQ(pid, waitpid(pid, &status, 0));
	TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return n;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static const struct removal removals[] = {
	    {TEST_MP "d/e", EXT4_DE_DIR, ext4_dir_rm},
	    {TEST_MP "d/f", EXT4_DE_REG_FILE, ext4_fremove},
	};
	char crashed[512];
	ext4_file f;
	uint32_t free_before;
	size_t i, n;
	long total, limit;

	snprintf(crashed, sizeof(crashed), "%s.crashed", image);
	file_dev_name_set(image);
	orig_bwrite = file_dev_get()->bdif->bwrite;
	file_dev_get()->bdif->bwrite = crash_bwrite;

	mount_journal(image);
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d/e"));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "d/f", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, crashed, sizeof(crashed), &n));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	free_before = free_inodes();
	umount_journal();

	for (i = 0; i < sizeof(removals) / sizeof(removals[0]); i++) {
		const struct removal *r = &removals[i];

		copy_file(image, crashed);
		total = remove_with_power_loss(crashed, r, -1);
		TEST_ASSERT(total > 0);

		for (limit = 0; limit <= total; limit++) {
			bool exists;
			uint32_t free_now;

			copy_file(image, crashed);
			remove_with_power_loss(crashed, r, limit);

			mount_journal(crashed);
			exists = ext4_inode_exist(r->path, r->type) == EOK;
			free_now = free_inodes();
			umount_journal();

			if (free_now != free_before + (exists ? 0 : 1)) {
				fprintf(stderr,
					"%s: power loss after %ld of %ld writes: "
					"entry %s, %u free inodes (before: %u)\n",
					r->path, limit, total,
					exists ? "exists" : "is gone",
					free_now, free_before);
				return 1;
			}
		}
		printf("%s: consistent after power loss at all %ld writes\n",
		       r->path, total + 1);
	}
	remove(crashed);
	return 0;
}
