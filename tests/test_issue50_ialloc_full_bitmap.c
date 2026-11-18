/*
 * Issue #50: ext4_ialloc_alloc_inode looped forever when a block group
 * descriptor reported free inodes but the group's inode bitmap had no clear
 * bit, because the ENOSPC path went back to the top of the loop without
 * moving on to the next block group.
 *
 * Creating a file must terminate: it has to succeed using a later group
 * when one has room, and fail with ENOSPC when no group has a free inode.
 * An alarm turns a hang into a test failure.
 */

#include "test_util.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>

#define INODES_PER_GROUP 16

static void on_alarm(int sig)
{
	static const char msg[] = "timed out: inode allocation hangs\n";

	(void)sig;
	if (write(2, msg, sizeof(msg) - 1) < 0)
		_exit(1);
	_exit(1);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char full_image[4096];
	ext4_file f;
	struct ext4_inode inode;
	uint32_t ino;
	size_t wcnt;

	signal(SIGALRM, on_alarm);
	alarm(10);

	/* Group 0 bitmap full, later groups free: allocation moves on. */
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "new.txt", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "abc", 3, &wcnt));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "new.txt", &ino, &inode));
	TEST_ASSERT(ino > INODES_PER_GROUP);
	test_umount();

	/* Every bitmap full: allocation fails cleanly. */
	snprintf(full_image, sizeof(full_image), "%s.full", image);
	TEST_ASSERT_EQ(EOK, test_mount(full_image, false));
	TEST_ASSERT_EQ(ENOSPC, ext4_fopen(&f, TEST_MP "new.txt", "wb"));
	test_umount();

	return 0;
}
