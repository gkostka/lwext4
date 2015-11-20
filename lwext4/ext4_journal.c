/**
 * @file  ext4_journal.c
 * @brief Journalling
 */

#include "ext4_config.h"
#include "ext4_types.h"
#include "ext4_fs.h"
#include "ext4_super.h"
#include "ext4_errno.h"
#include "ext4_blockdev.h"
#include "ext4_crc32c.h"
#include "ext4_debug.h"

#include <string.h>

int jbd_inode_bmap(struct jbd_fs *jbd_fs,
		   ext4_lblk_t iblock,
		   ext4_fsblk_t *fblock);

int jbd_sb_write(struct jbd_fs *jbd_fs, struct jbd_sb *s)
{
	int rc;
	struct ext4_fs *fs = jbd_fs->inode_ref.fs;
	uint64_t offset;
	ext4_fsblk_t fblock;
	rc = jbd_inode_bmap(jbd_fs, 0, &fblock);
	if (rc != EOK)
		return rc;

	offset = fblock * ext4_sb_get_block_size(&fs->sb);
	return ext4_block_writebytes(fs->bdev, offset, s,
				     EXT4_SUPERBLOCK_SIZE);
}

int jbd_sb_read(struct jbd_fs *jbd_fs, struct jbd_sb *s)
{
	int rc;
	struct ext4_fs *fs = jbd_fs->inode_ref.fs;
	uint64_t offset;
	ext4_fsblk_t fblock;
	rc = jbd_inode_bmap(jbd_fs, 0, &fblock);
	if (rc != EOK)
		return rc;

	offset = fblock * ext4_sb_get_block_size(&fs->sb);
	return ext4_block_readbytes(fs->bdev, offset, s,
				    EXT4_SUPERBLOCK_SIZE);
}

int jbd_get_fs(struct ext4_fs *fs,
	       struct jbd_fs *jbd_fs)
{
	int rc;
	uint32_t journal_ino;

	memset(jbd_fs, 0, sizeof(struct jbd_fs));
	journal_ino = ext4_get32(&fs->sb, journal_inode_number);

	rc = ext4_fs_get_inode_ref(fs,
				   journal_ino,
				   &jbd_fs->inode_ref);
	if (rc != EOK) {
		memset(jbd_fs, 0, sizeof(struct jbd_fs));
		return rc;
	}
	rc = jbd_sb_read(jbd_fs, &jbd_fs->sb);
	if (rc != EOK) {
		memset(jbd_fs, 0, sizeof(struct jbd_fs));
		ext4_fs_put_inode_ref(&jbd_fs->inode_ref);
	}
	return rc;
}

int jbd_put_fs(struct jbd_fs *jbd_fs)
{
	int rc;
	rc = ext4_fs_put_inode_ref(&jbd_fs->inode_ref);
	return rc;
}

int jbd_inode_bmap(struct jbd_fs *jbd_fs,
		   ext4_lblk_t iblock,
		   ext4_fsblk_t *fblock)
{
	int rc = ext4_fs_get_inode_data_block_index(
			&jbd_fs->inode_ref,
			iblock,
			fblock,
			false);
	return rc;
}

int jbd_block_get(struct jbd_fs *jbd_fs,
		  struct ext4_block *block,
		  ext4_fsblk_t fblock)
{
	/* TODO: journal device. */
	int rc;
	ext4_lblk_t iblock = (ext4_lblk_t)fblock;
	rc = jbd_inode_bmap(jbd_fs, iblock,
			    &fblock);
	if (rc != EOK)
		return rc;

	struct ext4_blockdev *bdev = jbd_fs->inode_ref.fs->bdev;
	rc = ext4_block_get(bdev, block, fblock);
	return rc;
}

int jbd_block_get_noread(struct jbd_fs *jbd_fs,
			 struct ext4_block *block,
			 ext4_fsblk_t fblock)
{
	/* TODO: journal device. */
	int rc;
	ext4_lblk_t iblock = (ext4_lblk_t)fblock;
	rc = jbd_inode_bmap(jbd_fs, iblock,
			    &fblock);
	if (rc != EOK)
		return rc;

	struct ext4_blockdev *bdev = jbd_fs->inode_ref.fs->bdev;
	rc = ext4_block_get_noread(bdev, block, fblock);
	return rc;
}

int jbd_block_set(struct jbd_fs *jbd_fs,
		  struct ext4_block *block)
{
	return ext4_block_set(jbd_fs->inode_ref.fs->bdev,
			      block);
}

int jbd_recovery(struct jbd_fs *jbd_fs)
{
	struct jbd_sb *sb = &jbd_fs->sb;
	if (!sb->start)
		return EOK;

}
