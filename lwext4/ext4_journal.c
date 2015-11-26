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

static bool jbd_verify_sb(struct jbd_sb *sb)
{
	struct jbd_bhdr *bhdr = &sb->header;
	if (bhdr->magic != to_be32(JBD_MAGIC_NUMBER))
		return false;

	if (bhdr->blocktype != to_be32(JBD_SUPERBLOCK) &&
	    bhdr->blocktype != to_be32(JBD_SUPERBLOCK_V2))
		return false;

	return true;
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

/* Make sure we wrap around the log correctly! */
#define wrap(sb, var)						\
do {									\
	if (var >= to_be32((sb)->maxlen))					\
		var -= (to_be32((sb)->maxlen) - to_be32((sb)->first));	\
} while (0)

#define ACTION_SCAN 0
#define ACTION_REVOKE 1
#define ACTION_RECOVER 2

struct recover_info {
	uint32_t start_trans_id;
	uint32_t last_trans_id;
};

int jbd_iterate_log(struct jbd_fs *jbd_fs,
		    struct recover_info *info,
		    int action)
{
	int r = EOK;
	bool log_end = false;
	struct jbd_sb *sb = &jbd_fs->sb;
	uint32_t start_trans_id, this_trans_id;
	uint32_t start_block, this_block;

	start_trans_id = this_trans_id = to_be32(sb->sequence);
	start_block = this_block = to_be32(sb->start);

	ext4_dbg(DEBUG_JBD, "Start of journal at trans id: %" PRIu32 "\n",
			    start_trans_id);

	while (!log_end) {
		struct ext4_block block;
		struct jbd_bhdr *header;
		if (action != ACTION_SCAN)
			if (this_trans_id > info->last_trans_id) {
				log_end = true;
				continue;
			}

		r = jbd_block_get(jbd_fs, &block, this_block);
		if (r != EOK)
			break;

		header = (struct jbd_bhdr *)block.data;
		if (header->magic != to_be32(JBD_MAGIC_NUMBER)) {
			jbd_block_set(jbd_fs, &block);
			log_end = true;
			continue;
		}

		if (header->sequence != to_be32(this_trans_id)) {
			if (this_trans_id <= info->last_trans_id)
				r = EIO;

			jbd_block_set(jbd_fs, &block);
			log_end = true;
			continue;
		}

		switch (header->blocktype) {
		case JBD_DESCRIPTOR_BLOCK:
			ext4_dbg(DEBUG_JBD, "Descriptor block: %u, "
					    "trans_id: %u\n",
					    this_block, this_trans_id);
			break;
		case JBD_COMMIT_BLOCK:
			ext4_dbg(DEBUG_JBD, "Commit block: %u, "
					    "trans_id: %u\n",
					    this_block, this_trans_id);
			this_trans_id++;
			break;
		case JBD_REVOKE_BLOCK:
			ext4_dbg(DEBUG_JBD, "Revoke block: %u, "
					    "trans_id: %u\n",
					    this_block, this_trans_id);
			break;
		default:
			log_end = true;
			break;
		}
		jbd_block_set(jbd_fs, &block);
		this_block++;
		wrap(sb, this_block);
		if (this_block == start_block)
			log_end = true;

	}
	ext4_dbg(DEBUG_JBD, "End of journal.\n");
	if (r == EOK && action == ACTION_SCAN) {
		info->start_trans_id = start_trans_id;
		if (this_trans_id > start_trans_id)
			info->last_trans_id = this_trans_id - 1;
		else
			info->last_trans_id = this_trans_id;
	}

	return r;
}

int jbd_recover(struct jbd_fs *jbd_fs)
{
	int r;
	struct recover_info info;
	struct jbd_sb *sb = &jbd_fs->sb;
	if (!sb->start)
		return EOK;

	r = jbd_iterate_log(jbd_fs, &info, ACTION_SCAN);
	return r;
}
