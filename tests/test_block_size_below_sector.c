/*
 * Filesystem block size smaller than the device sector size.
 *
 * lwext4 maps every filesystem block onto whole physical blocks of the
 * device (ext4_block_set_lb_size() only asserts lb % ph == 0). On a device
 * with 4 KiB sectors - SPI NOR flash erase sectors, 4Kn disks - a
 * filesystem with 1 KiB blocks used to be mounted anyway: with assertions
 * off, block addresses were truncated and every read of one 1 KiB block
 * transferred a whole 4 KiB sector into a 1 KiB cache buffer (heap
 * overflow); with assertions on it aborted. ext4_mkfs had the same problem.
 * Both must refuse such a combination with an error instead.
 */
#include "test_util.h"

#include <ext4_mkfs.h>

#include <stdio.h>
#include <string.h>

#define SECTOR 4096u

static FILE *img;
static uint64_t img_sectors;

static int sd_open(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

static int sd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		    uint32_t blk_cnt)
{
	(void)bdev;
	if (blk_id + blk_cnt > img_sectors)
		return EIO;
	if (fseeko(img, (off_t)(blk_id * SECTOR), SEEK_SET) ||
	    fread(buf, SECTOR, blk_cnt, img) != blk_cnt)
		return EIO;
	return EOK;
}

static int sd_bwrite(struct ext4_blockdev *bdev, const void *buf,
		     uint64_t blk_id, uint32_t blk_cnt)
{
	(void)bdev;
	if (blk_id + blk_cnt > img_sectors)
		return EIO;
	if (fseeko(img, (off_t)(blk_id * SECTOR), SEEK_SET) ||
	    fwrite(buf, SECTOR, blk_cnt, img) != blk_cnt)
		return EIO;
	return EOK;
}

static int sd_close(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

static uint8_t ph_bbuf[SECTOR];
static struct ext4_blockdev_iface iface = {
	.open = sd_open,
	.bread = sd_bread,
	.bwrite = sd_bwrite,
	.close = sd_close,
	.ph_bsize = SECTOR,
	.ph_bbuf = ph_bbuf,
};
static struct ext4_blockdev bd = { .bdif = &iface };

int main(int argc, char **argv)
{
	const char *path = test_image_arg(argc, argv);
	struct ext4_fs fs;
	struct ext4_mkfs_info info;
	ext4_file f;

	img = fopen(path, "r+b");
	TEST_ASSERT(img);
	fseeko(img, 0, SEEK_END);
	img_sectors = (uint64_t)ftello(img) / SECTOR;
	iface.ph_bcnt = img_sectors;
	bd.part_size = img_sectors * SECTOR;

	/* The image has 1 KiB blocks (made by mke2fs, see the .sh file). */
	TEST_ASSERT_EQ(EOK, ext4_device_register(&bd, TEST_DEV));
	TEST_ASSERT(ext4_mount(TEST_DEV, TEST_MP, false) != EOK);
	TEST_ASSERT(ext4_mount(TEST_DEV, TEST_MP, true) != EOK);

	/* ext4_mkfs must not format 1 or 2 KiB blocks onto 4 KiB sectors. */
	memset(&fs, 0, sizeof(fs));
	memset(&info, 0, sizeof(info));
	info.block_size = 1024;
	info.journal = true;
	TEST_ASSERT(ext4_mkfs(&fs, &bd, &info, F_SET_EXT4) != EOK);
	memset(&fs, 0, sizeof(fs));
	memset(&info, 0, sizeof(info));
	info.block_size = 2048;
	TEST_ASSERT(ext4_mkfs(&fs, &bd, &info, F_SET_EXT4) != EOK);

	/* A matching block size still works on the same device. */
	memset(&fs, 0, sizeof(fs));
	memset(&info, 0, sizeof(info));
	info.block_size = 4096;
	info.journal = true;
	TEST_ASSERT_EQ(EOK, ext4_mkfs(&fs, &bd, &info, F_SET_EXT4));
	TEST_ASSERT_EQ(EOK, ext4_mount(TEST_DEV, TEST_MP, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "file", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, "data", 4, NULL));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_umount(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_device_unregister(TEST_DEV));
	fclose(img);
	return 0;
}
