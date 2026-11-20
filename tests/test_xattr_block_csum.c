/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * The metadata_csum checksum of an xattr block is crc32c over the
 * filesystem UUID, the block number as a little endian 64 bit value and
 * the block (with h_checksum zero), stored little endian in h_checksum.
 * ext4_xattr_block_checksum() hashed the block number in host byte order
 * and the checksum was stored in host byte order, so on big endian hosts
 * every xattr block lwext4 wrote failed verification (e2fsck: "extended
 * attribute block passes checks, but checksum does not match block").
 * Only big endian hosts are affected; run this test under qemu-user for
 * those (ci qemu-user matrix: s390x, powerpc, mips).
 *
 * red-green: guard on little endian hosts, where the base computes the
 * same checksum (the test is red against the base on big endian hosts).
 *
 * The expected checksum is computed here byte by byte from the on-disk
 * layout, independently of the xattr code.
 */

#include "test_util.h"

#include <ext4_crc32.h>
#include <ext4_fs.h>
#include <ext4_inode.h>
#include <ext4_super.h>

#include <stddef.h>
#include <string.h>

#define F TEST_MP "f"

/* struct ext4_xattr_header: h_magic, h_refcount, h_blocks, h_hash,
 * h_checksum */
#define H_CHECKSUM_OFFSET 16

static uint32_t get_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	       (uint32_t)p[3] << 24;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static uint8_t data[65536];
	struct ext4_inode inode;
	struct ext4_sblock *sb;
	struct ext4_block b;
	struct ext4_fs *fs;
	ext4_file f;
	uint8_t le_blk[8];
	uint32_t ino, bsize, stored, crc;
	uint64_t blk;
	char value[300];
	int i;

	memset(value, 'v', sizeof(value));
	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, F, "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_setxattr(F, "user.small", 10, "s", 1));
	/* Too large for the inode body: goes to the xattr block */
	TEST_ASSERT_EQ(EOK, ext4_setxattr(F, "user.large", 10, value,
					  sizeof(value)));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	TEST_ASSERT(ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM));
	fs = (struct ext4_fs *)((char *)sb - offsetof(struct ext4_fs, sb));
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(F, &ino, &inode));
	blk = ext4_inode_get_file_acl(&inode, sb);
	TEST_ASSERT(blk != 0);

	bsize = ext4_sb_get_block_size(sb);
	TEST_ASSERT(bsize <= sizeof(data));
	TEST_ASSERT_EQ(EOK, ext4_block_get(fs->bdev, &b, blk));
	memcpy(data, b.data, bsize);
	TEST_ASSERT_EQ(EOK, ext4_block_set(fs->bdev, &b));

	/* h_magic 0xEA020000, little endian */
	TEST_ASSERT_EQ(0xEA020000, get_le32(data));
	stored = get_le32(data + H_CHECKSUM_OFFSET);
	memset(data + H_CHECKSUM_OFFSET, 0, 4);
	for (i = 0; i < 8; i++)
		le_blk[i] = (uint8_t)(blk >> (8 * i));
	crc = ext4_crc32c(EXT4_CRC32_INIT, sb->uuid, sizeof(sb->uuid));
	crc = ext4_crc32c(crc, le_blk, sizeof(le_blk));
	crc = ext4_crc32c(crc, data, bsize);
	TEST_ASSERT_EQ(crc, stored);
	test_umount();
	return 0;
}
