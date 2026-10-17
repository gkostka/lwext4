/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * On-disk fields that were read or written without byte order conversion
 * (or with the wrong width). Most of them only break on big endian hosts,
 * run this test under qemu-user for those (ci qemu-user matrix):
 *
 *  - ext4_create_hardlink() took the child inode number straight from the
 *    directory entry: creating a file in a subdirectory whose entry is not
 *    in the first cached block failed (ENOENT) or crashed.
 *  - ext2_htree_hash() copied s_hash_seed without conversion: lookups in
 *    htree directories built by e2fsprogs missed entries.
 *  - ext4_dir_dx_split_index() stored the fake entry length of new index
 *    nodes without conversion: e2fsck rejected two level htrees.
 *  - ext4_xattr_get() read the 16 bit e_value_offs as 32 bit: xattr values
 *    were read from the wrong place (or crashed).
 *  - ext4_ext_split_node() used extent first_block values unconverted:
 *    splitting an extent leaf corrupted the tree ("short write").
 *  - uid/gid (also on little endian hosts): the 32 bit ids were written as
 *    32 bit values into the 16 bit low fields, the high 16 bits were lost.
 */

#include "test_util.h"

#include <ext4_inode.h>
#include <ext4_misc.h>

#include <string.h>

#define NBIG 3000

static void write_file(const char *path, char c, size_t len)
{
	static char buf[4096];
	ext4_file f;
	size_t n;

	memset(buf, c, sizeof(buf));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "ab"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, buf, len, &n));
	TEST_ASSERT_EQ(len, n);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

static void check_file(const char *path, char c, size_t len)
{
	static char buf[4096];
	ext4_file f;
	size_t n, i, done = 0;

	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, path, "rb"));
	TEST_ASSERT_EQ(len, ext4_fsize(&f));
	while (done < len) {
		TEST_ASSERT_EQ(EOK, ext4_fread(&f, buf, sizeof(buf), &n));
		TEST_ASSERT(n > 0);
		for (i = 0; i < n; i++)
			TEST_ASSERT(buf[i] == c);
		done += n;
	}
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	struct ext4_inode inode;
	char path[64], buf[16];
	uint32_t ino, uid, gid;
	size_t len;
	int i;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));

	/* htree built by e2fsck -D (setup script), s_hash_seed from mke2fs */
	for (i = 0; i < NBIG; i += 7) {
		snprintf(path, sizeof(path), TEST_MP "big/entry_%d", i);
		TEST_ASSERT_EQ(EOK, ext4_inode_exist(path, EXT4_DE_REG_FILE));
	}

	/* xattr written by debugfs (setup script) */
	TEST_ASSERT_EQ(EOK, ext4_getxattr(TEST_MP "x", "user.color", 10, buf,
					  sizeof(buf), &len));
	TEST_ASSERT_EQ(4, len);
	TEST_ASSERT(memcmp(buf, "blue", 4) == 0);

	/* hardlink and rename in a subdirectory behind a big file */
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d"));
	for (i = 0; i < 64; i++)
		write_file(TEST_MP "d/big", 'b', 4096);
	write_file(TEST_MP "d/old", 'o', 100);
	TEST_ASSERT_EQ(EOK, ext4_flink(TEST_MP "d/old", TEST_MP "d/link"));
	TEST_ASSERT_EQ(EOK, ext4_frename(TEST_MP "d/old", TEST_MP "d/new"));
	check_file(TEST_MP "d/link", 'o', 100);

	/* two files growing block by block: fragmented extents, leaf split */
	for (i = 0; i < 400; i++) {
		write_file(TEST_MP "a", 'a', 1024);
		write_file(TEST_MP "b", 'b', 1024);
	}
	check_file(TEST_MP "a", 'a', 400 * 1024);
	check_file(TEST_MP "b", 'b', 400 * 1024);

	/* more entries in the htree: new index nodes */
	for (i = 0; i < 4000; i++) {
		snprintf(path, sizeof(path), TEST_MP "big/another_entry_%d", i);
		write_file(path, 'n', 1);
	}

	/* 32 bit uid/gid */
	TEST_ASSERT_EQ(EOK, ext4_owner_set(TEST_MP "a", 100000, 70000));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	TEST_ASSERT_EQ(EOK, ext4_owner_get(TEST_MP "a", &uid, &gid));
	TEST_ASSERT_EQ(100000, uid);
	TEST_ASSERT_EQ(70000, gid);
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "a", &ino, &inode));
	/* on disk: low 16 bits in i_uid/i_gid, high 16 bits in osd2 */
	TEST_ASSERT_EQ(100000 & 0xffff, to_le16(inode.uid));
	TEST_ASSERT_EQ(100000 >> 16, to_le16(inode.osd2.linux2.uid_high));
	TEST_ASSERT_EQ(70000 & 0xffff, to_le16(inode.gid));
	TEST_ASSERT_EQ(70000 >> 16, to_le16(inode.osd2.linux2.gid_high));
	check_file(TEST_MP "a", 'a', 400 * 1024);
	for (i = 0; i < 4000; i += 13) {
		snprintf(path, sizeof(path), TEST_MP "big/another_entry_%d", i);
		TEST_ASSERT_EQ(EOK, ext4_inode_exist(path, EXT4_DE_REG_FILE));
	}
	test_umount();
	return 0;
}
