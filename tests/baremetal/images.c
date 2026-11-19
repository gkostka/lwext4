/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Read-only tests on ext2/ext4 images made by mke2fs (see mkimages.py): the
 * images live in (flash) memory as a sparse sector list, are mounted
 * read-only and their content is checked. This covers superblock and group
 * descriptor parsing, metadata_csum verification (crc32c), indirect blocks,
 * extents and htree directory lookups on targets too small for a RAM disk.
 */

#include <ext4.h>

#include <stdint.h>
#include <string.h>

#include "check.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
/* The images may not all fit below 64 KiB of flash: use far addresses. */
#define IMG_MEM PROGMEM
typedef uint_farptr_t img_addr_t;
#define IMG_ADDR(sym) pgm_get_far_address(sym)
#define IMG_READ(dst, addr, n) memcpy_PF((dst), (addr), (n))
#define IMG_READ16(addr) pgm_read_word_far(addr)
#else
#define IMG_MEM
typedef uintptr_t img_addr_t;
#define IMG_ADDR(sym) ((img_addr_t)(sym))
#define IMG_READ(dst, addr, n) memcpy((dst), (const void *)(addr), (n))
#define IMG_READ16(addr) (*(const uint16_t *)(addr))
#endif

#include "testdata.h"

#define SECTOR 512u

struct sparse_image {
	uint32_t size;
	uint16_t count;
	img_addr_t idx;  /* uint16_t [count][2]: sector number, data index */
	img_addr_t data; /* uint8_t [][SECTOR] */
};

static struct sparse_image cur;
static uint16_t writes;

static int img_open(struct ext4_blockdev *bdev);
static int img_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		     uint32_t blk_cnt);
static int img_bwrite(struct ext4_blockdev *bdev, const void *buf,
		      uint64_t blk_id, uint32_t blk_cnt);
static int img_close(struct ext4_blockdev *bdev);

EXT4_BLOCKDEV_STATIC_INSTANCE(img_dev, SECTOR, 0, img_open, img_bread,
			      img_bwrite, img_close, 0, 0);

static int img_open(struct ext4_blockdev *bdev)
{
	bdev->part_offset = 0;
	bdev->part_size = cur.size;
	bdev->bdif->ph_bcnt = cur.size / SECTOR;
	return EOK;
}

/* Index of the data of sector n, or -1 for an all-zero sector. */
static int32_t img_lookup(uint32_t n)
{
	uint16_t lo = 0, hi = cur.count;

	while (lo < hi) {
		uint16_t mid = lo + (hi - lo) / 2;
		uint16_t s = IMG_READ16(cur.idx + (uint32_t)mid * 4u);

		if (s == n)
			return IMG_READ16(cur.idx + (uint32_t)mid * 4u + 2u);
		if (s < n)
			lo = mid + 1;
		else
			hi = mid;
	}
	return -1;
}

static int img_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		     uint32_t blk_cnt)
{
	uint8_t *p = buf;

	(void)bdev;
	if ((blk_id + blk_cnt) * SECTOR > cur.size)
		return EIO;
	while (blk_cnt--) {
		int32_t d = img_lookup((uint32_t)blk_id++);

		if (d < 0)
			memset(p, 0, SECTOR);
		else
			IMG_READ(p, cur.data + (uint32_t)d * SECTOR, SECTOR);
		p += SECTOR;
	}
	return EOK;
}

static int img_bwrite(struct ext4_blockdev *bdev, const void *buf,
		      uint64_t blk_id, uint32_t blk_cnt)
{
	(void)bdev;
	(void)buf;
	(void)blk_id;
	(void)blk_cnt;
	writes++;
	return EIO;
}

static int img_close(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

#define DEV "img"
#define MP "/img/"

static uint8_t pattern(uint32_t off)
{
	return (uint8_t)(off * 7u + (off >> 9));
}

static void check_files(uint32_t data_size)
{
	static uint8_t buf[200];
	ext4_file f;
	ext4_dir d;
	const ext4_direntry *de;
	size_t n;
	uint32_t off;
	uint16_t entries = 0;
	char path[160];
	int i;

	CHECK(ext4_fopen(&f, MP "hello.txt", "rb"));
	ASSERT(ext4_fsize(&f) == 13);
	CHECK(ext4_fread(&f, buf, sizeof(buf), &n));
	ASSERT(n == 13 && memcmp(buf, "hello lwext4\n", 13) == 0);
	CHECK(ext4_fclose(&f));

	/* Odd sized reads, so they straddle sector and block boundaries. */
	CHECK(ext4_fopen(&f, MP "data.bin", "rb"));
	ASSERT(ext4_fsize(&f) == data_size);
	for (off = 0; off < data_size; off += n) {
		CHECK(ext4_fread(&f, buf, 191, &n));
		ASSERT(n > 0);
		for (uint32_t j = 0; j < n; j++)
			ASSERT(buf[j] == pattern(off + j));
	}
	ASSERT(off == data_size);
	CHECK(ext4_fseek(&f, data_size - 10, SEEK_SET));
	CHECK(ext4_fread(&f, buf, sizeof(buf), &n));
	ASSERT(n == 10 && buf[9] == pattern(data_size - 1));
	CHECK(ext4_fclose(&f));

	/* Linear directory scan ... */
	CHECK(ext4_dir_open(&d, MP "dir"));
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		entries++;
	CHECK(ext4_dir_close(&d));
	ASSERT(entries == 2 + IMG_DIR_FILES);

	/* ... and htree lookups. */
	for (i = 0; i < IMG_DIR_FILES; i += 7) {
		strcpy(path, MP "dir/");
		snprintf(path + strlen(path), sizeof(path) - strlen(path),
			 IMG_NAME_FMT, i);
		CHECK(ext4_inode_exist(path, EXT4_DE_REG_FILE));
	}
	ASSERT(ext4_inode_exist(MP "dir/missing", EXT4_DE_REG_FILE) == ENOENT);
}

static void check_image(const char *name, uint32_t data_size)
{
	printf("image %s\n", name);
	writes = 0;
	CHECK(ext4_device_register(&img_dev, DEV));
	CHECK(ext4_mount(DEV, MP, true));
	check_files(data_size);
	CHECK(ext4_umount(MP));
	CHECK(ext4_device_unregister(DEV));
	ASSERT(writes == 0);
}

#define IMG_RUN(nm, sz, ns, dsz)                                              \
	cur.size = (sz);                                                       \
	cur.count = (ns);                                                      \
	cur.idx = IMG_ADDR(img_##nm##_idx);                                    \
	cur.data = IMG_ADDR(img_##nm##_data);                                  \
	check_image(#nm, (dsz));

void image_tests(void)
{
	IMG_LIST(IMG_RUN)
}
