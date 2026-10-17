/*
 * Checksum of the first leaf block of a new indexed directory.
 *
 * On a filesystem with dir_index and metadata_csum, ext4_dir_dx_init()
 * creates every new directory as an htree: block 0 holds the index root,
 * block 1 an empty leaf. The leaf is taken from the block cache without
 * being read and its checksum was computed before the inode field of the
 * empty entry was cleared, so the checksum covered whatever the recycled
 * cache buffer held. With the journal running (buffers are reused sooner)
 * e2fsck then reported "directory passes checks but fails checksum" for
 * every directory lwext4 created.
 *
 * Create directories inside a journal session, remount so that the blocks
 * come from disk, and verify each leaf block's checksum.
 */
#include "test_util.h"

#include <ext4_dir.h>
#include <ext4_fs.h>
#include <ext4_inode.h>
#include <ext4_super.h>

#include <stddef.h>
#include <string.h>

#define NDIRS 16

int main(int argc, char **argv)
{
	const char *img = test_image_arg(argc, argv);
	char path[64];
	struct ext4_sblock *sb;
	struct ext4_fs *fs;
	int checked = 0;

	TEST_ASSERT_EQ(EOK, test_mount(img, false));
	TEST_ASSERT_EQ(EOK, ext4_recover(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_journal_start(TEST_MP));
	TEST_ASSERT_EQ(EOK, ext4_dir_mk(TEST_MP "d"));
	for (int i = 0; i < NDIRS; i++) {
		snprintf(path, sizeof(path), TEST_MP "d/sub%02d", i);
		TEST_ASSERT_EQ(EOK, ext4_dir_mk(path));
	}
	TEST_ASSERT_EQ(EOK, ext4_journal_stop(TEST_MP));
	test_umount();

	TEST_ASSERT_EQ(EOK, test_mount(img, true));
	TEST_ASSERT_EQ(EOK, ext4_get_sblock(TEST_MP, &sb));
	TEST_ASSERT(ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM));
	TEST_ASSERT(ext4_sb_feature_com(sb, EXT4_FCOM_DIR_INDEX));
	fs = (struct ext4_fs *)((char *)sb - offsetof(struct ext4_fs, sb));

	for (int i = 0; i < NDIRS; i++) {
		uint32_t ino;
		struct ext4_inode inode;
		struct ext4_inode_ref ref;
		ext4_fsblk_t fblock;
		struct ext4_block b;

		snprintf(path, sizeof(path), TEST_MP "d/sub%02d", i);
		TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(path, &ino, &inode));
		TEST_ASSERT_EQ(EOK, ext4_fs_get_inode_ref(fs, ino, &ref));
		TEST_ASSERT(ext4_inode_has_flag(ref.inode, EXT4_INODE_FLAG_INDEX));
		TEST_ASSERT_EQ(EOK,
			       ext4_fs_get_inode_dblk_idx(&ref, 1, &fblock, false));
		TEST_ASSERT_EQ(EOK, ext4_block_get(fs->bdev, &b, fblock));
		if (!ext4_dir_csum_verify(&ref, (void *)b.data)) {
			fprintf(stderr, "%s: leaf block checksum mismatch\n",
				path);
			exit(1);
		}
		checked++;
		TEST_ASSERT_EQ(EOK, ext4_block_set(fs->bdev, &b));
		TEST_ASSERT_EQ(EOK, ext4_fs_put_inode_ref(&ref));
	}
	TEST_ASSERT_EQ(NDIRS, checked);
	test_umount();
	return 0;
}
