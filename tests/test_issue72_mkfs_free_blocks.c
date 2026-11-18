/*
 * Regression test for issue #72: ext4_mkfs over-allocating blocks.
 *
 * Formats images of various sizes, block sizes and feature sets (including
 * sizes where the last block group is partial) and checks that the on-disk
 * free block accounting is consistent: the superblock free count does not
 * exceed the block count, the group descriptor free counts add up to the
 * superblock count and match the block bitmaps, and bitmap padding past the
 * end of each group is set. The image is then checked with e2fsck (if
 * available), mounted, written to and checked again.
 */

#include "test_util.h"

#include <ext4_mkfs.h>
#include "../../blockdev/linux/file_dev.h"

#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct mkfs_case {
	uint64_t size;
	uint32_t block_size;
	int fs_type;
};

static const struct mkfs_case cases[] = {
	/* The configuration from the issue report. */
	{200000000, 4096, F_SET_EXT4},
	{200000000, 2048, F_SET_EXT3},
	{200000000, 1024, F_SET_EXT2},
	{20000000, 1024, F_SET_EXT3},
	/* Exact multiples of the group size. */
	{134217728, 4096, F_SET_EXT4},
	{8388608, 1024, F_SET_EXT2},
	/* 32 groups, exactly filling one 1K descriptor block. */
	{(31 * 8192 + 1 + 5000) * 1024ULL, 1024, F_SET_EXT4},
	/* Last group too small to hold its metadata: must be dropped. */
	{(32768 + 10) * 4096ULL, 4096, F_SET_EXT4},
	{(32768 + 262) * 4096ULL, 4096, F_SET_EXT4},
	/* Last group exactly large enough for its metadata (263 blocks). */
	{(32768 + 263) * 4096ULL, 4096, F_SET_EXT4},
};

static FILE *image_file;
static uint64_t image_len;
static uint8_t sb_buf[1024];
static uint8_t blk_buf[2][4096];

static uint16_t le16(const uint8_t *p)
{
	return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	       (uint32_t)p[3] << 24;
}

static void image_read(uint64_t off, void *buf, size_t len)
{
	TEST_ASSERT(off + len <= image_len);
	TEST_ASSERT(fseeko(image_file, (off_t)off, SEEK_SET) == 0);
	TEST_ASSERT(fread(buf, 1, len, image_file) == len);
}

/* Read a block into one of two scratch buffers selected by slot. */
static const uint8_t *image_block(uint32_t block_size, uint64_t blk, int slot)
{
	TEST_ASSERT(block_size <= sizeof(blk_buf[slot]));
	image_read(blk * block_size, blk_buf[slot], block_size);
	return blk_buf[slot];
}

static void create_image(const char *image, uint64_t size)
{
	FILE *f = fopen(image, "wb");
	TEST_ASSERT(f != NULL);
	TEST_ASSERT(ftruncate(fileno(f), (off_t)size) == 0);
	fclose(f);
}

static void check_accounting(const char *image)
{
	image_file = fopen(image, "rb");
	TEST_ASSERT(image_file != NULL);
	TEST_ASSERT(fseeko(image_file, 0, SEEK_END) == 0);
	image_len = (uint64_t)ftello(image_file);
	image_read(1024, sb_buf, sizeof(sb_buf));

	const uint8_t *sb = sb_buf;
	TEST_ASSERT_EQ(0xEF53, le16(sb + 0x38));

	uint32_t block_size = 1024u << le32(sb + 0x18);
	uint32_t blocks = le32(sb + 0x04);
	uint32_t free_blocks = le32(sb + 0x0C);
	uint32_t first_data_block = le32(sb + 0x14);
	uint32_t blocks_per_group = le32(sb + 0x20);
	uint32_t incompat = le32(sb + 0x60);
	uint32_t desc_size = (incompat & 0x80) ? le16(sb + 0xFE) : 32;
	uint32_t groups = (blocks - first_data_block + blocks_per_group - 1) /
			  blocks_per_group;

	printf("  blocks=%u free=%u groups=%u\n", blocks, free_blocks, groups);

	TEST_ASSERT((uint64_t)blocks * block_size <= image_len);
	TEST_ASSERT(free_blocks <= blocks);

	uint64_t desc_free_sum = 0;
	uint32_t g;
	for (g = 0; g < groups; g++) {
		const uint8_t *gdt = image_block(block_size,
				first_data_block + 1 + g * desc_size / block_size,
				0);
		const uint8_t *desc = gdt + (g * desc_size) % block_size;

		uint32_t first = first_data_block + g * blocks_per_group;
		uint32_t group_blocks = blocks_per_group;
		if (g == groups - 1)
			group_blocks = blocks - first;

		uint32_t bmp_blk = le32(desc + 0x00);
		uint32_t desc_free = le16(desc + 0x0C);
		uint16_t flags = le16(desc + 0x12);

		TEST_ASSERT(bmp_blk >= first && bmp_blk < first + group_blocks);
		TEST_ASSERT(!(flags & 0x2)); /* BLOCK_UNINIT */
		TEST_ASSERT(desc_free <= group_blocks);

		const uint8_t *bmp = image_block(block_size, bmp_blk, 1);
		uint32_t bit, clear = 0;
		for (bit = 0; bit < block_size * 8; bit++) {
			bool set = bmp[bit / 8] & (1u << (bit % 8));
			if (bit < group_blocks) {
				if (!set)
					clear++;
			} else if (!set) {
				fprintf(stderr, "group %u: padding bit %u "
					"not set\n", g, bit);
				exit(1);
			}
		}
		if (clear != desc_free)
			fprintf(stderr, "group %u: bitmap free %u, "
				"descriptor free %u\n", g, clear, desc_free);
		TEST_ASSERT_EQ(clear, desc_free);
		desc_free_sum += desc_free;
	}

	TEST_ASSERT_EQ(free_blocks, desc_free_sum);
	fclose(image_file);
}

static void run_e2fsck(const char *image)
{
	char cmd[1024];
	int rc;

	snprintf(cmd, sizeof(cmd),
		 "PATH=\"$PATH:/sbin:/usr/sbin\"; "
		 "command -v e2fsck >/dev/null 2>&1 || exit 127; "
		 "e2fsck -fn '%s' > '%s.fsck' 2>&1", image, image);
	rc = system(cmd);
	TEST_ASSERT(rc != -1 && WIFEXITED(rc));
	rc = WEXITSTATUS(rc);
	if (rc == 127) {
		printf("  e2fsck not found, skipping\n");
		return;
	}
	if (rc != 0) {
		snprintf(cmd, sizeof(cmd), "cat '%s.fsck' >&2", image);
		(void)system(cmd);
	}
	TEST_ASSERT_EQ(0, rc);
}

static void write_data(const char *image)
{
	static uint8_t buf[64 * 1024];
	ext4_file f;
	size_t wcnt, rcnt;
	int i;

	memset(buf, 0xA5, sizeof(buf));

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "data.bin", "wb"));
	for (i = 0; i < 16; i++) {
		TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, buf, sizeof(buf), &wcnt));
		TEST_ASSERT_EQ(sizeof(buf), wcnt);
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "data.bin", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(sizeof(buf), rcnt);
	TEST_ASSERT_EQ(16 * sizeof(buf), ext4_fsize(&f));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	test_umount();
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const struct mkfs_case *c = &cases[i];
		struct ext4_mkfs_info info;
		struct ext4_fs fs;

		printf("mkfs ext%d size=%llu block_size=%u\n", c->fs_type,
		       (unsigned long long)c->size, c->block_size);

		create_image(image, c->size);

		memset(&info, 0, sizeof(info));
		memset(&fs, 0, sizeof(fs));
		info.block_size = c->block_size;
		info.journal = c->fs_type != F_SET_EXT2;

		file_dev_name_set(image);
		TEST_ASSERT_EQ(EOK, ext4_mkfs(&fs, file_dev_get(), &info,
					      c->fs_type));

		check_accounting(image);
		run_e2fsck(image);

		write_data(image);
		check_accounting(image);
		run_e2fsck(image);
	}

	return 0;
}
