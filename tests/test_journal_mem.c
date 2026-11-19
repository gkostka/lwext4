/*
 * Regression test for issue #68: heap usage keeps growing while a file is
 * written with journaling enabled.
 *
 * The journal inode (inode 8) lives in the first inode table block, together
 * with the root directory and the first few inodes handed out to new files.
 * Every transaction that modifies such an inode dirties that block. As long as
 * the buffer is never written back, those transactions cannot be checkpointed
 * and stay queued in memory (jbd_trans, jbd_buf and jbd_block_rec
 * allocations) until the journal wraps around.
 *
 * The test writes a file whose inode shares the block with the journal inode
 * and checks that the amount of heap in use does not grow with the number of
 * writes.
 */

#include "test_util.h"

#include <string.h>

#if defined(__SANITIZE_ADDRESS__)
#define TEST_HAVE_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define TEST_HAVE_ASAN 1
#endif
#endif

#if defined(TEST_HAVE_ASAN)
/* From <sanitizer/allocator_interface.h>, which is not always installed. */
size_t __sanitizer_get_current_allocated_bytes(void);
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

#define LINE "This is test of file operations functionality\n"
#define WARMUP_WRITES 32
#define TEST_WRITES 200
/* Any per write growth over TEST_WRITES writes ends far above this. */
#define MAX_GROWTH (4 * 1024)

/**@brief Bytes currently allocated on the heap, or -1 if unknown.*/
static long long heap_in_use(void)
{
#if defined(TEST_HAVE_ASAN)
	return (long long)__sanitizer_get_current_allocated_bytes();
#elif defined(__GLIBC__) && \
	(__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
	struct mallinfo2 mi = mallinfo2();
	return (long long)(mi.uordblks + mi.hblkhd);
#else
	return -1;
#endif
}

static void write_lines(ext4_file *f, int count)
{
	size_t wcnt;
	int i;

	for (i = 0; i < count; i++) {
		TEST_ASSERT_EQ(EOK, ext4_fwrite(f, LINE, strlen(LINE), &wcnt));
		TEST_ASSERT_EQ(strlen(LINE), wcnt);
	}
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	long long before, after;
	ext4_file f;

	if (heap_in_use() < 0) {
		printf("SKIP: no way to query heap usage on this platform\n");
		return 0;
	}

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "log.txt", "w+"));
	/* 256 byte inodes in 4 KiB blocks: inodes 1..16 share the block
	 * holding the journal inode. */
	TEST_ASSERT(f.inode <= 16);

	write_lines(&f, WARMUP_WRITES);
	before = heap_in_use();
	write_lines(&f, TEST_WRITES);
	after = heap_in_use();

	printf("heap in use: %lld -> %lld bytes after %d writes\n", before,
	       after, TEST_WRITES);
	TEST_ASSERT(after - before < MAX_GROWTH);

	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();
	return 0;
}
