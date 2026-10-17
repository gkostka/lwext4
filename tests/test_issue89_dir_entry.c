/*
 * Issue #89: NULL pointer dereference in ext4_dir_en_get_name_len() called
 * from ext4_dir_entry_next().
 *
 * A directory whose i_size is 0 made ext4_dir_iterator_init() succeed
 * without an entry, which ext4_dir_entry_next() then dereferenced. Corrupted
 * directory entries (rec_len 0, entries straddling the block end, directory
 * holes) must likewise make directory operations fail cleanly instead of
 * crashing, hanging or reading past the block buffer.
 */

#include "test_util.h"

#include <string.h>
#include <unistd.h>

/* Read a directory to the end; returns the number of entries seen. */
static int read_dir(const char *path)
{
	ext4_dir d;
	const ext4_direntry *de;
	int n = 0;

	TEST_ASSERT_EQ(EOK, ext4_dir_open(&d, path));
	while ((de = ext4_dir_entry_next(&d)) != NULL) {
		TEST_ASSERT(de->name_length <= sizeof(de->name));
		TEST_ASSERT(n < 100);
		n++;
	}
	/* Reading past the end keeps returning NULL */
	TEST_ASSERT(ext4_dir_entry_next(&d) == NULL);
	TEST_ASSERT_EQ(EOK, ext4_dir_close(&d));
	return n;
}

static int try_open(const char *path, const char *flags)
{
	ext4_file f;
	int r = ext4_fopen(&f, path, flags);

	if (r == EOK)
		ext4_fclose(&f);
	return r;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	/* Corrupted entries used to send some walkers into endless loops */
	alarm(20);

	TEST_ASSERT_EQ(EOK, test_mount(image, false));

	/* Control: ".", "..", "a" */
	TEST_ASSERT_EQ(3, read_dir(TEST_MP "ok"));
	TEST_ASSERT_EQ(EOK, try_open(TEST_MP "ok/a", "rb"));

	/* i_size == 0: no entries at all (this is the reported crash) */
	TEST_ASSERT_EQ(0, read_dir(TEST_MP "zero"));
	TEST_ASSERT(try_open(TEST_MP "zero/a", "rb") != EOK);

	/* Directory hole: only the entries of the mapped block */
	TEST_ASSERT_EQ(3, read_dir(TEST_MP "hole"));
	TEST_ASSERT_EQ(EOK, try_open(TEST_MP "hole/a", "rb"));
	TEST_ASSERT(try_open(TEST_MP "hole/nonexistent", "rb") != EOK);

	/* rec_len 0 on "..": iteration stops after "." */
	TEST_ASSERT_EQ(1, read_dir(TEST_MP "reclen0"));
	TEST_ASSERT(try_open(TEST_MP "reclen0/nonexistent", "rb") != EOK);
	TEST_ASSERT(try_open(TEST_MP "reclen0/new", "wb") != EOK);

	/* Entry after "a" would straddle the block end */
	TEST_ASSERT_EQ(3, read_dir(TEST_MP "overrun"));
	TEST_ASSERT(try_open(TEST_MP "overrun/nonexistent", "rb") != EOK);
	TEST_ASSERT(try_open(TEST_MP "overrun/new", "wb") != EOK);

	/* Recursive removal walks the directories with the same iterator */
	TEST_ASSERT(ext4_dir_rm(TEST_MP "reclen0") != EOK);

	/* The control directory is still fine */
	TEST_ASSERT_EQ(3, read_dir(TEST_MP "ok"));

	/* Every block reference taken while failing must have been dropped */
	TEST_ASSERT_EQ(EOK, ext4_umount(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_device_unregister(TEST_DEV));
	return 0;
}
