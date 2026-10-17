/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Moving a directory to another parent rewrites its ".." entry. Without
 * dir_index (a linear directory) the directory block's checksum was not
 * updated, so on metadata_csum filesystems the block failed verification
 * (e2fsck: "directory passes checks but fails checksum").
 */

#include "test_util.h"

#include <ext4_dir.h>
#include <ext4_fs.h>

#include <stddef.h>

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	struct ext4_inode_ref ref;
	struct ext4_inode inode;
	struct ext4_sblock *sb;
	struct ext4_block b;
	struct ext4_fs *fs;
	ext4_fsblk_t fblk;
	uint32_t ino, parent, dotdot;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "a"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "b"));
	TEST_ASSERT_EQ(EOK, ext4_dir_mv(TEST_MP "a", TEST_MP "b/a"));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(image, true));
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "b", &parent, &inode));
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "b/a/..", &dotdot,
						&inode));
	TEST_ASSERT_EQ(parent, dotdot);
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "b/a", &ino, &inode));

	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	fs = (struct ext4_fs *)((char *)sb - offsetof(struct ext4_fs, sb));
	TEST_ASSERT(ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM));
	TEST_ASSERT_EQ(EOK, ext4_fs_get_inode_ref(fs, ino, &ref));
	TEST_ASSERT_EQ(EOK, ext4_fs_get_inode_dblk_idx(&ref, 0, &fblk, false));
	TEST_ASSERT_EQ(EOK, ext4_block_get(fs->bdev, &b, fblk));
	TEST_ASSERT(ext4_dir_csum_verify(&ref, (void *)b.data));
	TEST_ASSERT_EQ(EOK, ext4_block_set(fs->bdev, &b));
	TEST_ASSERT_EQ(EOK, ext4_fs_put_inode_ref(&ref));
	test_umount();
	return 0;
}
