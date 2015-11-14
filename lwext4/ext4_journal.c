/**
 * @file  ext4_journal.c
 * @brief Journalling
 */

#include "ext4_config.h"
#include "ext4_types.h"
#include "ext4_fs.h"
#include "ext4_errno.h"
#include "ext4_blockdev.h"
#include "ext4_crc32c.h"
#include "ext4_debug.h"

#include <string.h>

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
