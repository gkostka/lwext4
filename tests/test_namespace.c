/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Namespace operations: hard links and link counts, rename of files and
 * directories within and across directories, rename onto an existing name,
 * recursive directory removal, symlinks (fast and slow), special files,
 * inode attributes (mode, owner, times), seek/tell on open files and the
 * refusals of a read-only mount. The check script verifies the result with
 * debugfs and e2fsck.
 *
 * The image has no metadata_csum, and paths through "..", moves into the
 * own subtree, ext4_fremove() of a directory and ext4_dir_rm() of a tree
 * with hard links to the outside are left out: they fail on the base and
 * are covered by the regression tests of their fixes.
 *
 * red-green: guard (coverage test, passes on the base)
 */
#include "test_util.h"

#include <ext4_inode.h>

#include <string.h>

static void create(const char *path, const char *data)
{
	ext4_file f;
	size_t w;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, data, strlen(data), &w));
	TEST_ASSERT_EQ(strlen(data), w);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void expect_data(const char *path, const char *data)
{
	ext4_file f;
	char buf[256];
	size_t r;

	int rc = ext4_fopen(&f, path, "rb");

	if (rc != EOK)
		fprintf(stderr, "open %s: %d\n", path, rc);
	TEST_ASSERT_EQ(EOK, rc);
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &r));
	TEST_ASSERT_EQ(strlen(data), r);
	TEST_ASSERT(memcmp(buf, data, r) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static uint32_t links(const char *path)
{
	uint32_t ino;
	struct ext4_inode inode;

	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
	return ext4_inode_get_links_cnt(&inode);
}

static uint32_t ino_of(const char *path)
{
	uint32_t ino;
	struct ext4_inode inode;

	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
	return ino;
}

static int count_entries(const char *path)
{
	ext4_dir d;
	const ext4_direntry *de;
	int n = 0;

	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, path));
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		n++;
	ext4_dir_entry_rewind(&d);
	TEST_ASSERT(ext4_dir_entry_next(&d) != NULL);
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));
	return n;
}

static void hard_links(void)
{
	create(TEST_MP "a/file", "hard link data");
	TEST_ASSERT_EQ(1, links(TEST_MP "a/file"));
	TEST_ASSERT_EQ(EOK, ext4_flink(TEST_MP "a/file", TEST_MP "a/link1"));
	TEST_ASSERT_EQ(EOK, ext4_flink(TEST_MP "a/file", TEST_MP "b/link2"));
	TEST_ASSERT_EQ(3, links(TEST_MP "a/file"));
	TEST_ASSERT_EQ(ino_of(TEST_MP "a/file"), ino_of(TEST_MP "b/link2"));
	expect_data(TEST_MP "b/link2", "hard link data");

	/* Existing target, directory source, missing source. */
	TEST_ASSERT_EQ(EEXIST, ext4_flink(TEST_MP "a/file", TEST_MP "a/link1"));
	TEST_ASSERT_EQ(EINVAL, ext4_flink(TEST_MP "a", TEST_MP "alink"));
	TEST_ASSERT_EQ(ENOENT, ext4_flink(TEST_MP "a/nope", TEST_MP "x"));
	TEST_ASSERT_EQ(ENOENT, ext4_flink(TEST_MP "a/file",
					  TEST_MP "nodir/x"));

	/* Removing one name keeps the inode and the data. */
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "a/link1"));
	TEST_ASSERT_EQ(2, links(TEST_MP "a/file"));
	expect_data(TEST_MP "a/file", "hard link data");
	TEST_ASSERT_EQ(ENOENT, ext4_fremove(TEST_MP "a/link1"));
}

static void renames(void)
{
	uint32_t ino;

	/* Within a directory. */
	create(TEST_MP "a/r1", "rename me");
	ino = ino_of(TEST_MP "a/r1");
	TEST_ASSERT_EQ(EOK, ext4_frename(TEST_MP "a/r1", TEST_MP "a/r2"));
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "a/r1",
						EXT4_DE_REG_FILE));
	TEST_ASSERT_EQ(ino, ino_of(TEST_MP "a/r2"));
	TEST_ASSERT_EQ(1, links(TEST_MP "a/r2"));

	/* Across directories. */
	TEST_ASSERT_EQ(EOK, ext4_frename(TEST_MP "a/r2", TEST_MP "b/r3"));
	TEST_ASSERT_EQ(ino, ino_of(TEST_MP "b/r3"));
	expect_data(TEST_MP "b/r3", "rename me");

	/* Onto an existing name: refused, both stay. */
	create(TEST_MP "b/other", "other");
	TEST_ASSERT_EQ(EEXIST, ext4_frename(TEST_MP "b/r3", TEST_MP "b/other"));
	expect_data(TEST_MP "b/r3", "rename me");
	expect_data(TEST_MP "b/other", "other");
	TEST_ASSERT_EQ(ENOENT, ext4_frename(TEST_MP "b/nope", TEST_MP "b/x"));

	/* A directory with contents, across directories: ".." and the
	 * parents' link counts follow. */
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "a/sub"));
	create(TEST_MP "a/sub/inner", "inner");
	TEST_ASSERT_EQ(3, links(TEST_MP "a"));
	TEST_ASSERT_EQ(2, links(TEST_MP "b"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mv(TEST_MP "a/sub", TEST_MP "b/moved"));
	TEST_ASSERT_EQ(2, links(TEST_MP "a"));
	TEST_ASSERT_EQ(3, links(TEST_MP "b"));
	expect_data(TEST_MP "b/moved/inner", "inner");

	/* A directory created by mke2fs (not indexed) likewise. */
	TEST_ASSERT_EQ(EOK, ext4_dir_mv(TEST_MP "pre/child", TEST_MP "b/child"));
	expect_data(TEST_MP "b/child/f", "made by mke2fs\n");
}

static void dir_remove(void)
{
	char path[64];

	/* A tree: files, nested directories, a symlink. */
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "tree"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "tree/x"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "tree/x/y"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "tree/empty"));
	for (int i = 0; i < 20; i++) {
		snprintf(path, sizeof(path), TEST_MP "tree/x/y/f%d", i);
		create(path, "leaf");
	}
	create(TEST_MP "tree/x/g", "g");
	TEST_ASSERT_EQ(22, count_entries(TEST_MP "tree/x/y"));
	/* An existing directory is not an error (like mkdir -p). */
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "tree/x"));
	TEST_ASSERT_EQ(EOK, ext4_dir_rm(TEST_MP "tree"));
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "tree", EXT4_DE_DIR));
	TEST_ASSERT_EQ(ENOENT, ext4_dir_rm(TEST_MP "tree"));
}

static void symlinks(void)
{
	char buf[1024];
	char target[600];
	size_t r;

	/* Fast (in the inode) and slow (in a block) symlinks. */
	TEST_ASSERT_EQ(EOK, ext4_fsymlink("a/file", TEST_MP "fast"));
	memset(target, 't', sizeof(target) - 1);
	target[sizeof(target) - 1] = 0;
	memcpy(target, "slow/", 5);
	TEST_ASSERT_EQ(EOK, ext4_fsymlink(target, TEST_MP "slow"));
	TEST_ASSERT_EQ(EOK, ext4_readlink(TEST_MP "slow", buf, sizeof(buf),
					  &r));
	TEST_ASSERT_EQ(strlen(target), r);
	TEST_ASSERT(memcmp(buf, target, r) == 0);
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "fast", EXT4_DE_SYMLINK));
	/* An existing symlink gets the new target (slow to fast here). */
	TEST_ASSERT_EQ(EOK, ext4_fsymlink(target, TEST_MP "was_slow"));
	TEST_ASSERT_EQ(EOK, ext4_fsymlink("now/fast", TEST_MP "was_slow"));
	TEST_ASSERT_EQ(ENOENT, ext4_readlink(TEST_MP "nope", buf, sizeof(buf),
					     &r));
	TEST_ASSERT_EQ(EINVAL, ext4_readlink(TEST_MP "slow", NULL, 0, &r));
}

static void special_files(void)
{
	TEST_ASSERT_EQ(EOK, ext4_mknod(TEST_MP "chr", EXT4_DE_CHRDEV,
				       (1u << 8) | 3));
	TEST_ASSERT_EQ(EOK, ext4_mknod(TEST_MP "blk", EXT4_DE_BLKDEV,
				       (8u << 8) | 1));
	TEST_ASSERT_EQ(EOK, ext4_mknod(TEST_MP "fifo", EXT4_DE_FIFO, 0));
	TEST_ASSERT_EQ(EOK, ext4_mknod(TEST_MP "sock", EXT4_DE_SOCK, 0));
	TEST_ASSERT_EQ(EINVAL, ext4_mknod(TEST_MP "reg", EXT4_DE_REG_FILE, 0));
	TEST_ASSERT_EQ(EINVAL, ext4_mknod(TEST_MP "dir", EXT4_DE_DIR, 0));
	TEST_ASSERT_EQ(EINVAL, ext4_mknod(TEST_MP "bad", 99, 0));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "chr", EXT4_DE_CHRDEV));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "blk", EXT4_DE_BLKDEV));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "fifo", EXT4_DE_FIFO));
	TEST_ASSERT_EQ(EOK, ext4_inode_exist(TEST_MP "sock", EXT4_DE_SOCK));
	TEST_ASSERT_EQ(ENOENT, ext4_inode_exist(TEST_MP "fifo",
						EXT4_DE_REG_FILE));
}

static void attributes(void)
{
	uint32_t v, uid, gid;

	TEST_ASSERT_EQ(EOK, ext4_mode_set(TEST_MP "a/file", 0640));
	TEST_ASSERT_EQ(EOK, ext4_mode_get(TEST_MP "a/file", &v));
	TEST_ASSERT_EQ(0100640, v);
	TEST_ASSERT_EQ(EOK, ext4_owner_set(TEST_MP "a/file", 1234, 5678));
	TEST_ASSERT_EQ(EOK, ext4_owner_get(TEST_MP "a/file", &uid, &gid));
	TEST_ASSERT_EQ(1234, uid);
	TEST_ASSERT_EQ(5678, gid);
	TEST_ASSERT_EQ(EOK, ext4_atime_set(TEST_MP "a/file", 1000000001));
	TEST_ASSERT_EQ(EOK, ext4_mtime_set(TEST_MP "a/file", 1000000002));
	TEST_ASSERT_EQ(EOK, ext4_ctime_set(TEST_MP "a/file", 1000000003));
	TEST_ASSERT_EQ(EOK, ext4_atime_get(TEST_MP "a/file", &v));
	TEST_ASSERT_EQ(1000000001, v);
	TEST_ASSERT_EQ(EOK, ext4_mtime_get(TEST_MP "a/file", &v));
	TEST_ASSERT_EQ(1000000002, v);
	TEST_ASSERT_EQ(EOK, ext4_ctime_get(TEST_MP "a/file", &v));
	TEST_ASSERT_EQ(1000000003, v);

	TEST_ASSERT_EQ(ENOENT, ext4_mode_set(TEST_MP "nope", 0));
	TEST_ASSERT_EQ(ENOENT, ext4_mode_get(TEST_MP "nope", &v));
	TEST_ASSERT_EQ(ENOENT, ext4_owner_set(TEST_MP "nope", 0, 0));
	TEST_ASSERT_EQ(ENOENT, ext4_owner_get(TEST_MP "nope", &uid, &gid));
	TEST_ASSERT_EQ(ENOENT, ext4_atime_set(TEST_MP "nope", 0));
	TEST_ASSERT_EQ(ENOENT, ext4_mtime_set(TEST_MP "nope", 0));
	TEST_ASSERT_EQ(ENOENT, ext4_ctime_set(TEST_MP "nope", 0));
	TEST_ASSERT_EQ(ENOENT, ext4_atime_get(TEST_MP "nope", &v));
	TEST_ASSERT_EQ(ENOENT, ext4_mtime_get(TEST_MP "nope", &v));
	TEST_ASSERT_EQ(ENOENT, ext4_ctime_get(TEST_MP "nope", &v));
	TEST_ASSERT_EQ(ENOENT, ext4_mode_set("/elsewhere/x", 0));
}

static void seek_tell(void)
{
	ext4_file f;
	char buf[8];
	size_t n;

	create(TEST_MP "seek", "0123456789");
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "seek", "r+b"));
	TEST_ASSERT_EQ(10, ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, 4, SEEK_SET));
	TEST_ASSERT_EQ(4, ext4_ftell(&f));
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, 2, SEEK_CUR));
	TEST_ASSERT_EQ(6, ext4_ftell(&f));
	/* lwext4's SEEK_END takes the distance back from the end. */
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, 3, SEEK_END));
	TEST_ASSERT_EQ(7, ext4_ftell(&f));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, 3, &n));
	TEST_ASSERT_EQ(3, n);
	TEST_ASSERT(memcmp(buf, "789", 3) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fseek(&f, -2, SEEK_CUR));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "AB", 2, &n));
	TEST_ASSERT_EQ(10, ext4_fsize(&f));
	TEST_ASSERT_EQ(EINVAL, ext4_fseek(&f, 11, SEEK_SET));
	TEST_ASSERT_EQ(EINVAL, ext4_fseek(&f, -1, SEEK_SET));
	TEST_ASSERT_EQ(EINVAL, ext4_fseek(&f, -1, SEEK_END));
	TEST_ASSERT_EQ(EINVAL, ext4_fseek(&f, 11, SEEK_END));
	TEST_ASSERT_EQ(EINVAL, ext4_fseek(&f, 20, SEEK_CUR));
	TEST_ASSERT_EQ(EINVAL, ext4_fseek(&f, -20, SEEK_CUR));
	TEST_ASSERT_EQ(EINVAL, ext4_fseek(&f, 0, 42));
	/* Append mode writes at the end. */
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "seek", "ab"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "!", 1, &n));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	expect_data(TEST_MP "seek", "01234567AB!");

	/* Opening directories as files and vice versa. */
	TEST_ASSERT_EQ(ENOENT, ext4_fopen(&f, TEST_MP "nope", "rb"));
	TEST_ASSERT(ext4_fopen(&f, TEST_MP "a", "rb") != EOK);
	TEST_ASSERT(ext4_fopen(&f, TEST_MP "a/file/x", "wb") != EOK);
	TEST_ASSERT_EQ(EINVAL, ext4_fopen(&f, TEST_MP "seek", "bogus"));
}

static void read_only(const char *image)
{
	ext4_file f;
	char buf[16];
	size_t n;
	uint32_t v;

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	expect_data(TEST_MP "a/file", "hard link data");
	TEST_ASSERT_EQ(EROFS, ext4_fopen(&f, TEST_MP "new", "wb"));
	/* Opening for update works, writing through the handle does not. */
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "a/file", "r+b"));
	TEST_ASSERT_EQ(EROFS, ext4_fwrite(&f, "x", 1, &n));
	TEST_ASSERT_EQ(EROFS, ext4_ftruncate(&f, 0));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EROFS, ext4_fremove(TEST_MP "a/file"));
	TEST_ASSERT_EQ(EROFS, ext4_flink(TEST_MP "a/file", TEST_MP "l"));
	TEST_ASSERT_EQ(EROFS, ext4_frename(TEST_MP "a/file", TEST_MP "l"));
	TEST_ASSERT_EQ(EROFS, ext4_dir_mk(TEST_MP "newdir"));
	TEST_ASSERT_EQ(EROFS, ext4_dir_rm(TEST_MP "b"));
	TEST_ASSERT_EQ(EROFS, ext4_dir_mv(TEST_MP "b", TEST_MP "c"));
	TEST_ASSERT_EQ(EROFS, ext4_fsymlink("x", TEST_MP "sl"));
	TEST_ASSERT_EQ(EROFS, ext4_mknod(TEST_MP "n", EXT4_DE_FIFO, 0));
	TEST_ASSERT_EQ(EROFS, ext4_mode_set(TEST_MP "a/file", 0600));
	TEST_ASSERT_EQ(EROFS, ext4_owner_set(TEST_MP "a/file", 1, 1));
	TEST_ASSERT_EQ(EROFS, ext4_atime_set(TEST_MP "a/file", 1));
	TEST_ASSERT_EQ(EROFS, ext4_mtime_set(TEST_MP "a/file", 1));
	TEST_ASSERT_EQ(EROFS, ext4_ctime_set(TEST_MP "a/file", 1));
	/* Journal start/stop are no-ops on a read-only mount. */
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	/* Reading still works. */
	TEST_ASSERT_EQ(EOK, ext4_mode_get(TEST_MP "a/file", &v));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "a/file", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &n));
	TEST_ASSERT_EQ(EROFS, ext4_fwrite(&f, "x", 1, &n));
	TEST_ASSERT_EQ(EROFS, ext4_ftruncate(&f, 0));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "a"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "b"));
	/* First, while every block is still unused: a slow symlink's block
	 * is only zero padded with the fix (test_symlink_slow_zero). */
	symlinks();
	hard_links();
	renames();
	dir_remove();
	special_files();
	attributes();
	seek_tell();
	test_umount();

	read_only(image);
	return 0;
}
