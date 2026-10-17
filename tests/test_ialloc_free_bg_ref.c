/*
 * Regression test for block group reference leaks on error paths of the
 * inode allocator (gkostka/lwext4#57).
 *
 * The read of the inode bitmap is made to fail while a file is removed.
 * ext4_ialloc_free_inode() then returns an error, and it must release the
 * block group reference it took. A leaked reference leaves the group
 * descriptor block pinned in the block cache.
 */

#include "test_util.h"

#include "../../blockdev/linux/file_dev.h"

#include <ext4_bcache.h>
#include <ext4_block_group.h>
#include <ext4_blockdev.h>
#include <ext4_super.h>

static int (*real_bread)(struct ext4_blockdev *bdev, void *buf,
			 uint64_t blk_id, uint32_t blk_cnt);
static uint64_t fail_pba;
static int failed_reads;

static int failing_bread(struct ext4_blockdev *bdev, void *buf,
			 uint64_t blk_id, uint32_t blk_cnt)
{
	if (blk_id <= fail_pba && fail_pba < blk_id + blk_cnt) {
		failed_reads++;
		return EIO;
	}
	return real_bread(bdev, buf, blk_id, blk_cnt);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	struct ext4_blockdev *bdev = file_dev_get();
	struct ext4_sblock *sb;
	static uint8_t gdt[4096];
	ext4_file f;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	TEST_ASSERT_EQ(4096, ext4_sb_get_block_size(sb));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "empty.txt", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	/* The group descriptor table follows the superblock in block 1 and
	 * the file lives in group 0. */
	TEST_ASSERT_EQ(EOK, ext4_blocks_get_direct(bdev, gdt, 1, 1));
	uint64_t ibmp = ext4_bg_get_inode_bitmap((struct ext4_bgroup *)gdt, sb);
	TEST_ASSERT(ibmp != 0);

	fail_pba = ibmp * bdev->lg_bsize / bdev->bdif->ph_bsize;
	real_bread = bdev->bdif->bread;
	bdev->bdif->bread = failing_bread;

	TEST_ASSERT(ext4_fremove(TEST_MP "empty.txt") != EOK);
	TEST_ASSERT(failed_reads > 0);

	bdev->bdif->bread = real_bread;

	/* No operation is in flight, so the only reference to the group
	 * descriptor block must be the one taken here. */
	struct ext4_block b;
	TEST_ASSERT_EQ(EOK, ext4_block_get(bdev, &b, 1));
	TEST_ASSERT_EQ(1, b.buf->refctr);
	TEST_ASSERT_EQ(EOK, ext4_block_set(bdev, &b));

	test_umount();
	return 0;
}
