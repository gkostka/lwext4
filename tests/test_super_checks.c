/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * The superblock checks of ext4_mount() (ext4_sb_check(), the geometry
 * checks of ext4_sb_check_geometry() and the feature checks of
 * ext4_fs_check_features()), one superblock field at a time: every
 * inconsistent or unsupported value must make ext4_mount() fail cleanly
 * (no crash, no sanitizer report, the device can be registered again),
 * unsupported read-only compatible features must mount read-only, and the
 * unmodified images must still mount. Most of the geometry checks come
 * with #90/#92/#93 (the base this builds on); before them lwext4 mounted
 * many of these superblocks.
 */
#include "test_util.h"

#include <string.h>

#define SB 1024

/* Superblock field offsets */
#define S_INODES_COUNT 0x00
#define S_BLOCKS_COUNT 0x04
#define S_FIRST_DATA_BLOCK 0x14
#define S_LOG_BLOCK_SIZE 0x18
#define S_LOG_CLUSTER_SIZE 0x1c
#define S_BLOCKS_PER_GROUP 0x20
#define S_CLUSTERS_PER_GROUP 0x24
#define S_INODES_PER_GROUP 0x28
#define S_MAGIC 0x38
#define S_FIRST_INO 0x54
#define S_INODE_SIZE 0x58
#define S_FEATURE_INCOMPAT 0x60
#define S_FEATURE_RO_COMPAT 0x64
#define S_RESERVED_GDT_BLOCKS 0xce
#define S_DESC_SIZE 0xfe
#define S_FIRST_META_BG 0x104
#define S_CHECKSUM_TYPE 0x175
#define S_CHECKSUM 0x3fc

struct patch {
	const char *what;
	int off;
	int size; /* 1, 2 or 4 bytes; 0: OR val into the 32 bit field */
	uint32_t val;
};

static const struct patch bad[] = {
	{"magic", S_MAGIC, 2, 0x1234},
	{"no inodes", S_INODES_COUNT, 4, 0},
	{"no blocks", S_BLOCKS_COUNT, 4, 0},
	{"no blocks per group", S_BLOCKS_PER_GROUP, 4, 0},
	{"no inodes per group", S_INODES_PER_GROUP, 4, 0},
	{"inodes smaller than 128 bytes", S_INODE_SIZE, 2, 64},
	{"first inode below 11", S_FIRST_INO, 4, 5},
	{"descriptors larger than 64 bytes", S_DESC_SIZE, 2, 128},
	{"block size of 1 GiB", S_LOG_BLOCK_SIZE, 4, 20},
	{"cluster size differs from block size", S_LOG_CLUSTER_SIZE, 4, 2},
	{"more blocks per group than bits in a bitmap block",
	 S_BLOCKS_PER_GROUP, 4, 16384},
	{"inode size not a power of 2", S_INODE_SIZE, 2, 384},
	{"inodes larger than a block", S_INODE_SIZE, 2, 2048},
	{"fewer inodes per group than per block", S_INODES_PER_GROUP, 4, 2},
	{"more inodes per group than bits in a bitmap block",
	 S_INODES_PER_GROUP, 4, 9000},
	{"first data block beyond the end", S_FIRST_DATA_BLOCK, 4, 20000},
	{"more inodes than the groups hold", S_INODES_COUNT, 4, 1000000},
	{"too many reserved descriptor blocks", S_RESERVED_GDT_BLOCKS, 2, 300},
	{"unsupported incompatible feature (inline_data)", S_FEATURE_INCOMPAT,
	 0, 0x8000},
	{"unknown incompatible feature", S_FEATURE_INCOMPAT, 0, 0x80000000},
};

/* bigalloc (a read-only compatible feature lwext4 does not support) with
 * an inconsistent cluster geometry: the geometry check fails first. */
static const struct patch bad_bigalloc[][2] = {
	{{"bigalloc", S_FEATURE_RO_COMPAT, 0, 0x200},
	 {"cluster smaller than a block", S_LOG_CLUSTER_SIZE, 4, 0xffffffff}},
	{{"bigalloc", S_FEATURE_RO_COMPAT, 0, 0x200},
	 {"no clusters per group", S_CLUSTERS_PER_GROUP, 4, 0}},
	{{"bigalloc", S_FEATURE_RO_COMPAT, 0, 0x200},
	 {"clusters per group times cluster size != blocks per group",
	  S_LOG_CLUSTER_SIZE, 4, 2}},
	{{"bigalloc", S_FEATURE_RO_COMPAT, 0, 0x200},
	 {"cluster size of 2 GiB", S_LOG_CLUSTER_SIZE, 4, 21}},
};

static void copy_file(const char *from, const char *to)
{
	static char buf[65536];
	FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
	size_t n;

	TEST_ASSERT(in && out);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		TEST_ASSERT_EQ(n, fwrite(buf, 1, n, out));
	fclose(in);
	TEST_ASSERT_EQ(0, fclose(out));
}

static void apply(const char *image, const struct patch *p)
{
	uint8_t b[4];
	uint32_t v = p->val;
	int size = p->size ? p->size : 4;
	FILE *f = fopen(image, "r+b");

	TEST_ASSERT(f);
	TEST_ASSERT(fseek(f, SB + p->off, SEEK_SET) == 0);
	if (!p->size) {
		TEST_ASSERT_EQ(4, fread(b, 1, 4, f));
		v |= b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24;
		TEST_ASSERT(fseek(f, SB + p->off, SEEK_SET) == 0);
	}
	for (int i = 0; i < size; i++)
		b[i] = (uint8_t)(v >> (8 * i));
	TEST_ASSERT_EQ(size, fwrite(b, 1, size, f));
	TEST_ASSERT_EQ(0, fclose(f));
}

/* Mount must fail; the device must be usable afterwards. */
static void refused(const char *image, const char *what)
{
	int r = test_mount(image, false);

	if (r == EOK) {
		fprintf(stderr, "%s: mounted\n", what);
		exit(1);
	}
	TEST_ASSERT_EQ(EOK, ext4_device_unregister(TEST_DEV));
}

static void readable(const char *image)
{
	char buf[32];
	ext4_file f;
	size_t rcnt;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "file", "rb"));
	TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &rcnt));
	TEST_ASSERT_EQ(18, rcnt);
	TEST_ASSERT(memcmp(buf, "superblock checks\n", 18) == 0);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	(void)image;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	char scratch[512], orig[512];
	ext4_file f;

	/* The unmodified images mount. */
	snprintf(orig, sizeof(orig), "%s.csum", image);
	for (int i = 0; i < 3; i++) {
		const char *img = i == 0 ? image : orig;

		if (i == 2) {
			snprintf(scratch, sizeof(scratch), "%s.metabg", image);
			img = scratch;
		}
		TEST_ASSERT_EQ(EOK, test_mount(img, false));
		readable(img);
		test_umount();
	}

	snprintf(scratch, sizeof(scratch), "%s.scratch", image);
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		copy_file(image, scratch);
		apply(scratch, &bad[i]);
		refused(scratch, bad[i].what);
	}
	for (size_t i = 0; i < sizeof(bad_bigalloc) / sizeof(bad_bigalloc[0]);
	     i++) {
		copy_file(image, scratch);
		apply(scratch, &bad_bigalloc[i][0]);
		apply(scratch, &bad_bigalloc[i][1]);
		refused(scratch, bad_bigalloc[i][1].what);
	}

	/* meta_bg with its first meta group beyond the descriptor blocks */
	snprintf(orig, sizeof(orig), "%s.metabg", image);
	copy_file(orig, scratch);
	apply(scratch, &(struct patch){"first meta group beyond the end",
				       S_FIRST_META_BG, 4, 1000});
	refused(scratch, "first meta group beyond the end");

	/* metadata_csum: a wrong checksum, and a checksum type other than
	 * crc32c */
	snprintf(orig, sizeof(orig), "%s.csum", image);
	copy_file(orig, scratch);
	apply(scratch, &(struct patch){"checksum", S_CHECKSUM, 4, 0x12345678});
	refused(scratch, "wrong superblock checksum");
	copy_file(orig, scratch);
	apply(scratch, &(struct patch){"checksum type", S_CHECKSUM_TYPE, 1, 2});
	refused(scratch, "checksum type other than crc32c");

	/* An unknown read-only compatible feature: mounted read-only. */
	copy_file(image, scratch);
	apply(scratch, &(struct patch){"unknown ro_compat", S_FEATURE_RO_COMPAT,
				       0, 0x80000000});
	TEST_ASSERT_EQ(EOK, test_mount(scratch, false));
	readable(scratch);
	TEST_ASSERT_EQ(EROFS, ext4_fopen(&f, TEST_MP "new", "wb"));
	TEST_ASSERT_EQ(EROFS, ext4_dir_mk(TEST_MP "dir"));
	test_umount();
	return 0;
}
