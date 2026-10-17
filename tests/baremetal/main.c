/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * lwext4 test firmware for bare-metal and simulated targets (Cortex-M on
 * QEMU MPS2 boards, arm-sim on qemu-arm, AVR on simavr, MSP430 on the GDB
 * simulator).
 *
 * 1. unit.c:   byte order, checksums, bitmaps, htree hash
 * 2. images.c: read-only mounts of mke2fs made ext2/ext4 images
 * 3. here:     if the target has the RAM for it (LWEXT4_TEST_RAMDISK_SIZE),
 *              ext4_mkfs on a RAM disk, mount it and exercise the
 *              directory/file API, then remount and check that the data
 *              survived.
 *
 * Every failure prints "FAIL ..." and stops with a non-zero status (where
 * the platform can report one); success ends with a "PASS" line.
 */

#include <ext4.h>
#include <ext4_mkfs.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "check.h"

#ifndef LWEXT4_TEST_RAMDISK_SIZE
#define LWEXT4_TEST_RAMDISK_SIZE (2ul * 1024ul * 1024ul)
#endif
#ifndef LWEXT4_TEST_JOURNAL
#define LWEXT4_TEST_JOURNAL 1
#endif

void test_fail(const char *file, int line, const char *expr, long value)
{
#ifdef __AVR__
	printf_P(PSTR("FAIL %S:%d: %S (%ld)\n"), file, line, expr, value);
#else
	printf("FAIL %s:%d: %s (%ld)\n", file, line, expr, value);
#endif
	platform_exit(1);
}

#if LWEXT4_TEST_RAMDISK_SIZE

#define RAMDISK_SIZE LWEXT4_TEST_RAMDISK_SIZE
#define RAMDISK_BSIZE 512u

static uint8_t ramdisk[RAMDISK_SIZE] __attribute__((aligned(8)));

static int ramdisk_open(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

static int ramdisk_bread(struct ext4_blockdev *bdev, void *buf,
			 uint64_t blk_id, uint32_t blk_cnt)
{
	(void)bdev;
	if ((blk_id + blk_cnt) * RAMDISK_BSIZE > RAMDISK_SIZE)
		return EIO;
	memcpy(buf, ramdisk + blk_id * RAMDISK_BSIZE, blk_cnt * RAMDISK_BSIZE);
	return EOK;
}

static int ramdisk_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt)
{
	(void)bdev;
	if ((blk_id + blk_cnt) * RAMDISK_BSIZE > RAMDISK_SIZE)
		return EIO;
	memcpy(ramdisk + blk_id * RAMDISK_BSIZE, buf, blk_cnt * RAMDISK_BSIZE);
	return EOK;
}

static int ramdisk_close(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

EXT4_BLOCKDEV_STATIC_INSTANCE(ramdisk_dev, RAMDISK_BSIZE,
			      RAMDISK_SIZE / RAMDISK_BSIZE, ramdisk_open,
			      ramdisk_bread, ramdisk_bwrite, ramdisk_close, 0,
			      0);

#define DEV "ramdisk"
#define MP "/mp/"
#define FILE_SIZE (40u * 1024u + 123u)

static uint8_t wbuf[4096];
static uint8_t rbuf[4096];
static struct ext4_fs fs;

static uint8_t pattern(uint32_t off)
{
	return (uint8_t)(off * 7u + (off >> 9));
}

static void write_file(const char *path)
{
	ext4_file f;
	size_t n;
	uint32_t off = 0;

	CHECK(ext4_fopen(&f, path, "wb"));
	while (off < FILE_SIZE) {
		uint32_t len = FILE_SIZE - off;
		if (len > sizeof(wbuf))
			len = sizeof(wbuf);
		for (uint32_t i = 0; i < len; i++)
			wbuf[i] = pattern(off + i);
		CHECK(ext4_fwrite(&f, wbuf, len, &n));
		ASSERT(n == len);
		off += len;
	}
	ASSERT(ext4_fsize(&f) == FILE_SIZE);
	CHECK(ext4_fclose(&f));
}

static void verify_file(const char *path)
{
	ext4_file f;
	size_t n;
	uint32_t off = 0;

	CHECK(ext4_fopen(&f, path, "rb"));
	ASSERT(ext4_fsize(&f) == FILE_SIZE);
	while (off < FILE_SIZE) {
		CHECK(ext4_fread(&f, rbuf, sizeof(rbuf), &n));
		ASSERT(n > 0);
		for (uint32_t i = 0; i < n; i++)
			ASSERT(rbuf[i] == pattern(off + i));
		off += n;
	}
	ASSERT(off == FILE_SIZE);

	/* Unaligned seek + short read. */
	CHECK(ext4_fseek(&f, 1027, SEEK_SET));
	CHECK(ext4_fread(&f, rbuf, 13, &n));
	ASSERT(n == 13);
	for (uint32_t i = 0; i < 13; i++)
		ASSERT(rbuf[i] == pattern(1027 + i));
	CHECK(ext4_fclose(&f));
}

static void mount(void)
{
	CHECK(ext4_device_register(&ramdisk_dev, DEV));
	CHECK(ext4_mount(DEV, MP, false));
	int r = ext4_recover(MP);
	ASSERT(r == EOK || r == ENOTSUP); /* ENOTSUP: no journal (ext2) */
	CHECK(ext4_journal_start(MP));
}

static void umount(void)
{
	CHECK(ext4_journal_stop(MP));
	CHECK(ext4_umount(MP));
	CHECK(ext4_device_unregister(DEV));
}

static void run(int fs_type)
{
	struct ext4_mkfs_info info = {
		.block_size = 1024,
		.journal = LWEXT4_TEST_JOURNAL,
	};
	ext4_dir d;
	const ext4_direntry *de;
	int entries = 0;
	char path[64];

	printf("-- ext%d\n", fs_type);
	memset(ramdisk, 0xa5, sizeof(ramdisk));
	memset(&fs, 0, sizeof(fs));
	CHECK(ext4_mkfs(&fs, &ramdisk_dev, &info, fs_type));

	mount();
	CHECK(ext4_dir_mk(MP "dir"));
	for (int i = 0; i < 20; i++) {
		snprintf(path, sizeof(path), MP "dir/sub%02d", i);
		CHECK(ext4_dir_mk(path));
	}
	write_file(MP "dir/data.bin");
	verify_file(MP "dir/data.bin");
	write_file(MP "scratch.bin");
	CHECK(ext4_fremove(MP "scratch.bin"));
	umount();

	/* Everything must survive a remount. */
	mount();
	verify_file(MP "dir/data.bin");
	ASSERT(ext4_inode_exist(MP "scratch.bin", EXT4_DE_REG_FILE) != EOK);
	CHECK(ext4_dir_open(&d, MP "dir"));
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		entries++;
	CHECK(ext4_dir_close(&d));
	ASSERT(entries == 2 + 20 + 1);
	CHECK(ext4_dir_rm(MP "dir"));
	ASSERT(ext4_inode_exist(MP "dir", EXT4_DE_DIR) != EOK);
	umount();
}

static void rw_tests(void)
{
	run(F_SET_EXT2);
	run(F_SET_EXT3);
	run(F_SET_EXT4);
}

#endif /* LWEXT4_TEST_RAMDISK_SIZE */

int main(void)
{
	platform_init();
	printf("lwext4 embedded test: %u bit int, %s endian\n",
	       (unsigned)(sizeof(int) * 8), CONFIG_BIG_ENDIAN ? "big" : "little");
	unit_tests();
	image_tests();
#if LWEXT4_TEST_RAMDISK_SIZE
	rw_tests();
#else
	printf("no RAM disk on this target, skipping ext4_mkfs tests\n");
#endif
	printf("PASS\n");
	platform_exit(0);
}
