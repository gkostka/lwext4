/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * RAM footprint of lwext4 on a Cortex-M (README.md "Memory footprint"),
 * measured on a QEMU MPS2 board: peak heap and peak stack while an ext2
 * (FOOTPRINT_EXT=2) or ext4 with journal and extents (FOOTPRINT_EXT=4)
 * RAM disk is formatted, mounted and used. Linked with
 * -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free and
 * tests/baremetal/startup.c; prints "footprint: heap <n> stack <n>".
 */

#include <ext4.h>
#include <ext4_mkfs.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* tests/baremetal/startup.c: semihosting console and exit */
void platform_init(void);
void platform_exit(int status) __attribute__((noreturn));

#ifndef FOOTPRINT_EXT
#define FOOTPRINT_EXT 4
#endif

/* ------------------------------------------------------------------------ */
/* Heap accounting */

void *__real_malloc(size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);

#define HDR 8u
static size_t heap_now, heap_peak;
static int counting;

static void account(long delta)
{
	if (!counting)
		return;
	heap_now += delta;
	if (heap_now > heap_peak)
		heap_peak = heap_now;
}

void *__wrap_malloc(size_t size)
{
	uint8_t *p = __real_malloc(size + HDR);

	if (!p)
		return NULL;
	*(size_t *)p = counting ? size : 0;
	account((long)size);
	return p + HDR;
}

void *__wrap_calloc(size_t n, size_t size)
{
	void *p = __wrap_malloc(n * size);

	if (p)
		memset(p, 0, n * size);
	return p;
}

void __wrap_free(void *ptr)
{
	uint8_t *p = ptr;

	if (!p)
		return;
	p -= HDR;
	account(-(long)*(size_t *)p);
	__real_free(p);
}

void *__wrap_realloc(void *ptr, size_t size)
{
	uint8_t *p;
	size_t old;

	if (!ptr)
		return __wrap_malloc(size);
	p = (uint8_t *)ptr - HDR;
	old = *(size_t *)p;
	p = __real_realloc(p, size + HDR);
	if (!p)
		return NULL;
	*(size_t *)p = counting ? size : 0;
	account((long)size - (long)old);
	return p + HDR;
}

/* ------------------------------------------------------------------------ */
/* Stack high water mark: paint the stack below the current frame */

#define STACK_PAINT 65536u
#define PAINT 0xdeadbeefu

static volatile uint32_t *paint_lo, *paint_hi;

static void __attribute__((noinline)) stack_paint(void)
{
	volatile uint32_t *p;

	/* Below this function's frame: main calls workload() at the same
	 * depth afterwards. */
	paint_hi = (volatile uint32_t *)__builtin_frame_address(0) - 16;
	paint_lo = paint_hi - STACK_PAINT / 4;
	for (p = paint_lo; p < paint_hi; p++)
		*p = PAINT;
}

static size_t stack_used(void)
{
	volatile uint32_t *p = paint_lo;

	while (p < paint_hi && *p == PAINT)
		p++;
	return (size_t)(paint_hi - p) * 4;
}

/* ------------------------------------------------------------------------ */
/* RAM disk */

#define RAMDISK_SIZE (2u * 1024u * 1024u)
#define RAMDISK_BSIZE 512u

static uint8_t ramdisk[RAMDISK_SIZE] __attribute__((aligned(8)));

static int rd_open(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

static int rd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
		    uint32_t blk_cnt)
{
	(void)bdev;
	if ((blk_id + blk_cnt) * RAMDISK_BSIZE > RAMDISK_SIZE)
		return EIO;
	memcpy(buf, ramdisk + blk_id * RAMDISK_BSIZE, blk_cnt * RAMDISK_BSIZE);
	return EOK;
}

static int rd_bwrite(struct ext4_blockdev *bdev, const void *buf,
		     uint64_t blk_id, uint32_t blk_cnt)
{
	(void)bdev;
	if ((blk_id + blk_cnt) * RAMDISK_BSIZE > RAMDISK_SIZE)
		return EIO;
	memcpy(ramdisk + blk_id * RAMDISK_BSIZE, buf, blk_cnt * RAMDISK_BSIZE);
	return EOK;
}

static int rd_close(struct ext4_blockdev *bdev)
{
	(void)bdev;
	return EOK;
}

EXT4_BLOCKDEV_STATIC_INSTANCE(ramdisk_dev, RAMDISK_BSIZE,
			      RAMDISK_SIZE / RAMDISK_BSIZE, rd_open, rd_bread,
			      rd_bwrite, rd_close, 0, 0);

/* ------------------------------------------------------------------------ */

#define CHECK(expr)                                                            \
	do {                                                                   \
		int r_ = (expr);                                               \
		if (r_ != EOK) {                                               \
			printf("FAIL %s:%d: %s = %d\n", __FILE__, __LINE__,    \
			       #expr, r_);                                     \
			platform_exit(1);                                      \
		}                                                              \
	} while (0)

#define MP "/mp/"

static struct ext4_fs fs;
static uint8_t buf[4096];

static void workload(void)
{
	char path[32];
	ext4_file f;
	size_t n;
	int i;

	CHECK(ext4_device_register(&ramdisk_dev, "rd"));
	CHECK(ext4_mount("rd", MP, false));
#if FOOTPRINT_EXT == 4
	CHECK(ext4_recover(MP));
	CHECK(ext4_journal_start(MP));
#endif
	CHECK(ext4_dir_mk(MP "dir"));
	/* no snprintf: its stack use is not lwext4's */
	strcpy(path, MP "dir/f00");
	for (i = 0; i < 50; i++) {
		path[sizeof(MP "dir/f") - 1] = (char)('0' + i / 10);
		path[sizeof(MP "dir/f")] = (char)('0' + i % 10);
		CHECK(ext4_fopen(&f, path, "wb"));
		CHECK(ext4_fwrite(&f, buf, 100 + i * 50, &n));
		CHECK(ext4_fclose(&f));
	}
	CHECK(ext4_fopen(&f, MP "big", "wb"));
	for (i = 0; i < 64; i++)
		CHECK(ext4_fwrite(&f, buf, sizeof(buf), &n));
	CHECK(ext4_fclose(&f));
	CHECK(ext4_fopen(&f, MP "big", "rb"));
	for (i = 0; i < 64; i++)
		CHECK(ext4_fread(&f, buf, sizeof(buf), &n));
	CHECK(ext4_fclose(&f));
	CHECK(ext4_fremove(MP "big"));
	CHECK(ext4_dir_rm(MP "dir"));
#if FOOTPRINT_EXT == 4
	CHECK(ext4_journal_stop(MP));
#endif
	CHECK(ext4_umount(MP));
	CHECK(ext4_device_unregister("rd"));
}

int main(void)
{
	struct ext4_mkfs_info info = {
		.block_size = 1024,
		.journal = FOOTPRINT_EXT == 4,
	};
	size_t stack;

	platform_init();
	printf("lwext4 footprint, ext%d\n", FOOTPRINT_EXT);
	memset(buf, 0x5a, sizeof(buf));
	CHECK(ext4_mkfs(&fs, &ramdisk_dev, &info,
			FOOTPRINT_EXT == 4 ? F_SET_EXT4 : F_SET_EXT2));

	stack_paint();
	counting = 1;
	workload();
	counting = 0;
	stack = stack_used();

	printf("footprint: heap %u stack %u\n", (unsigned)heap_peak,
	       (unsigned)stack);
	platform_exit(0);
}
