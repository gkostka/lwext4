/*
 * Issue #75: the linux file device must be able to expose a window (partition
 * offset + size) of the backing file instead of always starting at byte 0.
 *
 * 1. Mount an ext4 filesystem placed 1 MiB into an image, read a known file,
 *    write a new one, remount and read it back, run e2fsck on the extracted
 *    partition and check that the bytes around the window are untouched.
 * 2. Direct block I/O outside the window and invalid windows are rejected.
 * 3. Without the new setters the device still covers the whole file.
 * 4. The use case from the issue: write an MBR with ext4_mbr_write, put a
 *    filesystem in the first partition, point the file device at it and use
 *    it without touching the MBR.
 */

#include "test_util.h"

#include <ext4_mbr.h>

#include "../blockdev/linux/file_dev.h"

#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MIB (1024 * 1024)
#define PAD_SIZE MIB
#define PART_SIZE (8 * MIB)
#define PATTERN 0xa5

static uint8_t buf[MIB];

static uint64_t file_size(const char *path)
{
	struct stat st;

	TEST_ASSERT(stat(path, &st) == 0);
	return st.st_size;
}

/* Every byte in [off, off + len) of path equals val. */
static void check_pattern(const char *path, long off, size_t len, uint8_t val)
{
	FILE *fp = fopen(path, "rb");
	size_t i;

	TEST_ASSERT(fp);
	TEST_ASSERT(len <= sizeof(buf));
	TEST_ASSERT(fseek(fp, off, SEEK_SET) == 0);
	TEST_ASSERT_EQ(len, fread(buf, 1, len, fp));
	fclose(fp);
	for (i = 0; i < len; i++)
		if (buf[i] != val) {
			fprintf(stderr, "%s: byte %ld changed to 0x%02x\n",
				path, off + (long)i, buf[i]);
			exit(1);
		}
}

/* Copy [off, off + len) of src into dst and run e2fsck -fn on it. */
static void fsck_window(const char *src, const char *dst, uint64_t off,
			uint64_t len)
{
	FILE *in = fopen(src, "rb");
	FILE *out = fopen(dst, "wb");
	char cmd[8192];

	TEST_ASSERT(in && out);
	TEST_ASSERT(fseek(in, off, SEEK_SET) == 0);
	while (len) {
		size_t n = len < sizeof(buf) ? len : sizeof(buf);

		TEST_ASSERT_EQ(n, fread(buf, 1, n, in));
		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
		len -= n;
	}
	fclose(in);
	fclose(out);

	snprintf(cmd, sizeof(cmd),
		 "PATH=\"$PATH:/sbin:/usr/sbin\" e2fsck -fn '%s'", dst);
	TEST_ASSERT_EQ(0, system(cmd));
}

static void read_file(const char *path, const char *expect)
{
	ext4_file f;
	char rbuf[64];
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, rbuf, sizeof(rbuf), &rcnt));
	TEST_ASSERT_EQ(strlen(expect), rcnt);
	TEST_ASSERT(memcmp(rbuf, expect, rcnt) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void write_file(const char *path, const char *data)
{
	ext4_file f;
	size_t wcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, data, strlen(data), &wcnt));
	TEST_ASSERT_EQ(strlen(data), wcnt);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void set_window(uint64_t off, uint64_t size)
{
	file_dev_part_offset_set(off);
	file_dev_part_size_set(size);
}

static void test_mount_window(const char *image)
{
	char check[4096];

	set_window(PAD_SIZE, PART_SIZE);
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(PAD_SIZE, file_dev_get()->part_offset);
	TEST_ASSERT_EQ(PART_SIZE, file_dev_get()->part_size);
	read_file(TEST_MP "hello.txt", "hello lwext4\n");
	write_file(TEST_MP "new.txt", "written through a partition window\n");
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	read_file(TEST_MP "new.txt", "written through a partition window\n");
	test_umount();

	check_pattern(image, 0, PAD_SIZE, PATTERN);
	check_pattern(image, PAD_SIZE + PART_SIZE, PAD_SIZE, PATTERN);

	snprintf(check, sizeof(check), "%s.fsck", image);
	fsck_window(image, check, PAD_SIZE, PART_SIZE);
}

static void test_out_of_range(const char *image)
{
	struct ext4_blockdev *bd = file_dev_get();
	/* Room for the two block request below: the device must reject it,
	 * but the buffer still has to be valid for the length asked for. */
	uint8_t blk[2 * 4096];
	const uint32_t bsize = 4096;
	const uint64_t nblk = PART_SIZE / bsize;

	file_dev_name_set(image);

	/* Direct I/O past the end of the window must fail. */
	set_window(PAD_SIZE, PART_SIZE);
	TEST_ASSERT_EQ(EOK, ext4_block_init(bd));
	ext4_block_set_lb_size(bd, bsize);
	memset(blk, 0, sizeof(blk));
	TEST_ASSERT_EQ(EOK, ext4_blocks_get_direct(bd, blk, nblk - 1, 1));
	TEST_ASSERT(ext4_blocks_get_direct(bd, blk, nblk, 1) != EOK);
	TEST_ASSERT(ext4_blocks_set_direct(bd, blk, nblk, 1) != EOK);
	TEST_ASSERT(ext4_blocks_set_direct(bd, blk, nblk - 1, 2) != EOK);
	TEST_ASSERT(ext4_block_writebytes(bd, PART_SIZE - 1, blk, 2) != EOK);
	TEST_ASSERT_EQ(EOK, ext4_block_fini(bd));
	check_pattern(image, PAD_SIZE + PART_SIZE, PAD_SIZE, PATTERN);

	/* The window must lie inside the file and be sector aligned. */
	set_window(PAD_SIZE, file_size(image));
	TEST_ASSERT(ext4_block_init(bd) != EOK);
	set_window(file_size(image) + 512, 0);
	TEST_ASSERT(ext4_block_init(bd) != EOK);
	set_window(PAD_SIZE + 1, PART_SIZE);
	TEST_ASSERT(ext4_block_init(bd) != EOK);
	check_pattern(image, 0, PAD_SIZE, PATTERN);
}

static void test_defaults(const char *image)
{
	struct ext4_blockdev *bd = file_dev_get();

	/* Offset only: the window extends to the end of the file. */
	file_dev_name_set(image);
	set_window(PAD_SIZE, 0);
	TEST_ASSERT_EQ(EOK, ext4_block_init(bd));
	TEST_ASSERT_EQ(PAD_SIZE, bd->part_offset);
	TEST_ASSERT_EQ(file_size(image) - PAD_SIZE, bd->part_size);
	TEST_ASSERT_EQ(EOK, ext4_block_fini(bd));

	/* Zero offset and size: the old whole-file behaviour. */
	set_window(0, 0);
	TEST_ASSERT_EQ(EOK, ext4_block_init(bd));
	TEST_ASSERT_EQ(0, bd->part_offset);
	TEST_ASSERT_EQ(file_size(image), bd->part_size);
	TEST_ASSERT_EQ(file_size(image) / 512, bd->bdif->ph_bcnt);
	TEST_ASSERT_EQ(EOK, ext4_block_fini(bd));
}

static void test_mbr_partition(const char *image)
{
	struct ext4_blockdev *bd = file_dev_get();
	struct ext4_mbr_parts parts = {.division = {100, 0, 0, 0}};
	struct ext4_mbr_bdevs bdevs;
	uint8_t mbr[512];
	uint64_t off, size, fs_size;
	char path[4096], fs_path[4096], check[4096];
	FILE *in, *out;

	snprintf(path, sizeof(path), "%s.mbr", image);
	out = fopen(path, "wb");
	TEST_ASSERT(out);
	TEST_ASSERT(ftruncate(fileno(out), 32 * MIB) == 0);
	fclose(out);

	file_dev_name_set(path);
	set_window(0, 0);
	TEST_ASSERT_EQ(EOK, ext4_mbr_write(bd, &parts, 0x75));
	TEST_ASSERT_EQ(EOK, ext4_mbr_scan(bd, &bdevs));
	TEST_ASSERT(bdevs.partitions[0].bdif);
	off = bdevs.partitions[0].part_offset;
	size = bdevs.partitions[0].part_size;
	TEST_ASSERT(off > 0);

	in = fopen(path, "rb");
	TEST_ASSERT(in);
	TEST_ASSERT_EQ(1, fread(mbr, sizeof(mbr), 1, in));
	fclose(in);

	/* Place the mke2fs image from the setup script in the partition. */
	snprintf(fs_path, sizeof(fs_path), "%s.fs", image);
	fs_size = file_size(fs_path);
	TEST_ASSERT(fs_size <= size);
	in = fopen(fs_path, "rb");
	out = fopen(path, "r+b");
	TEST_ASSERT(in && out);
	TEST_ASSERT(fseek(out, off, SEEK_SET) == 0);
	while (!feof(in)) {
		size_t n = fread(buf, 1, sizeof(buf), in);

		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
	}
	fclose(in);
	fclose(out);

	set_window(off, size);
	TEST_ASSERT_EQ(EOK, test_mount(path, false));
	read_file(TEST_MP "hello.txt", "hello lwext4\n");
	write_file(TEST_MP "mbr.txt", "inside the first MBR partition\n");
	test_umount();
	TEST_ASSERT_EQ(EOK, test_mount(path, true));
	read_file(TEST_MP "mbr.txt", "inside the first MBR partition\n");
	test_umount();

	in = fopen(path, "rb");
	TEST_ASSERT(in);
	TEST_ASSERT_EQ(1, fread(buf, sizeof(mbr), 1, in));
	fclose(in);
	TEST_ASSERT(memcmp(buf, mbr, sizeof(mbr)) == 0);
	check_pattern(path, sizeof(mbr), off - sizeof(mbr), 0);
	check_pattern(path, off + size, 32 * MIB - (off + size), 0);
	TEST_ASSERT_EQ(32 * MIB, file_size(path));

	snprintf(check, sizeof(check), "%s.fsck", path);
	fsck_window(path, check, off, fs_size);
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);

	TEST_ASSERT_EQ(PAD_SIZE + PART_SIZE + PAD_SIZE, file_size(image));

	test_mount_window(image);
	test_out_of_range(image);
	test_defaults(image);
	test_mbr_partition(image);

	set_window(0, 0);
	return 0;
}
