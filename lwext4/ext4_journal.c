/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * Copyright (c) 2015 Kaho Ng (ngkaho1234@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_journal.c
 * @brief Journal handle functions
 */

#include "ext4_config.h"
#include "ext4_types.h"
#include "ext4_fs.h"
#include "ext4_super.h"
#include "ext4_errno.h"
#include "ext4_blockdev.h"
#include "ext4_crc32c.h"
#include "ext4_debug.h"
#include "tree.h"

#include <string.h>
#include <stdlib.h>

struct revoke_entry {
	ext4_fsblk_t block;
	uint32_t trans_id;
	RB_ENTRY(revoke_entry) revoke_node;
};

struct recover_info {
	uint32_t start_trans_id;
	uint32_t last_trans_id;
	uint32_t this_trans_id;
	RB_HEAD(jbd_revoke, revoke_entry) revoke_root;
};

struct replay_arg {
	struct recover_info *info;
	uint32_t *this_block;
	uint32_t this_trans_id;
};

static int
jbd_revoke_entry_cmp(struct revoke_entry *a, struct revoke_entry *b)
{
	if (a->block > b->block)
		return 1;
	else if (a->block < b->block)
		return -1;
	return 0;
}

RB_GENERATE_INTERNAL(jbd_revoke, revoke_entry, revoke_node,
		     jbd_revoke_entry_cmp, static inline)

#define jbd_alloc_revoke_entry() calloc(1, sizeof(struct revoke_entry))
#define jbd_free_revoke_entry(addr) free(addr)

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
	struct jbd_bhdr *header = &sb->header;
	if (jbd_get32(header, magic) != JBD_MAGIC_NUMBER)
		return false;

	if (jbd_get32(header, blocktype) != JBD_SUPERBLOCK &&
	    jbd_get32(header, blocktype) != JBD_SUPERBLOCK_V2)
		return false;

	return true;
}

static int jbd_write_sb(struct jbd_fs *jbd_fs)
{
	int rc = EOK;
	if (jbd_fs->dirty) {
		rc = jbd_sb_write(jbd_fs, &jbd_fs->sb);
		if (rc != EOK)
			return rc;

		jbd_fs->dirty = false;
	}
	return rc;
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
		return rc;
	}
	if (!jbd_verify_sb(&jbd_fs->sb)) {
		memset(jbd_fs, 0, sizeof(struct jbd_fs));
		ext4_fs_put_inode_ref(&jbd_fs->inode_ref);
		rc = EIO;
	}

	return rc;
}

int jbd_put_fs(struct jbd_fs *jbd_fs)
{
	int rc = EOK;
	rc = jbd_write_sb(jbd_fs);

	ext4_fs_put_inode_ref(&jbd_fs->inode_ref);
	return rc;
}

int jbd_inode_bmap(struct jbd_fs *jbd_fs,
		   ext4_lblk_t iblock,
		   ext4_fsblk_t *fblock)
{
	int rc = ext4_fs_get_inode_dblk_idx(
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
	if (rc == EOK)
		ext4_bcache_set_flag(block->buf, BC_FLUSH);

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
	if (rc == EOK)
		ext4_bcache_set_flag(block->buf, BC_FLUSH);

	return rc;
}

int jbd_block_set(struct jbd_fs *jbd_fs,
		  struct ext4_block *block)
{
	return ext4_block_set(jbd_fs->inode_ref.fs->bdev,
			      block);
}

/*
 * helper functions to deal with 32 or 64bit block numbers.
 */
int jbd_tag_bytes(struct jbd_fs *jbd_fs)
{
	int size;

	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V3))
		return sizeof(struct jbd_block_tag3);

	size = sizeof(struct jbd_block_tag);

	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V2))
		size += sizeof(uint16_t);

	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_64BIT))
		return size;

	return size - sizeof(uint32_t);
}

/**@brief: tag information. */
struct tag_info {
	int tag_bytes;
	ext4_fsblk_t block;
	bool uuid_exist;
	uint8_t uuid[UUID_SIZE];
	bool last_tag;
};

static int
jbd_extract_block_tag(struct jbd_fs *jbd_fs,
		      void *__tag,
		      int tag_bytes,
		      int32_t remain_buf_size,
		      struct tag_info *tag_info)
{
	char *uuid_start;
	tag_info->tag_bytes = tag_bytes;
	tag_info->uuid_exist = false;
	tag_info->last_tag = false;

	if (remain_buf_size - tag_bytes < 0)
		return EINVAL;

	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V3)) {
		struct jbd_block_tag3 *tag = __tag;
		tag_info->block = jbd_get32(tag, blocknr);
		if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
					     JBD_FEATURE_INCOMPAT_64BIT))
			 tag_info->block |=
				 (uint64_t)jbd_get32(tag, blocknr_high) << 32;

		if (jbd_get32(tag, flags) & JBD_FLAG_ESCAPE)
			tag_info->block = 0;

		if (!(jbd_get32(tag, flags) & JBD_FLAG_SAME_UUID)) {
			if (remain_buf_size - tag_bytes < UUID_SIZE)
				return EINVAL;

			uuid_start = (char *)tag + tag_bytes;
			tag_info->uuid_exist = true;
			tag_info->tag_bytes += UUID_SIZE;
			memcpy(tag_info->uuid, uuid_start, UUID_SIZE);
		}

		if (jbd_get32(tag, flags) & JBD_FLAG_LAST_TAG)
			tag_info->last_tag = true;

	} else {
		struct jbd_block_tag *tag = __tag;
		tag_info->block = jbd_get32(tag, blocknr);
		if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
					     JBD_FEATURE_INCOMPAT_64BIT))
			 tag_info->block |=
				 (uint64_t)jbd_get32(tag, blocknr_high) << 32;

		if (jbd_get16(tag, flags) & JBD_FLAG_ESCAPE)
			tag_info->block = 0;

		if (!(jbd_get16(tag, flags) & JBD_FLAG_SAME_UUID)) {
			if (remain_buf_size - tag_bytes < UUID_SIZE)
				return EINVAL;

			uuid_start = (char *)tag + tag_bytes;
			tag_info->uuid_exist = true;
			tag_info->tag_bytes += UUID_SIZE;
			memcpy(tag_info->uuid, uuid_start, UUID_SIZE);
		}

		if (jbd_get16(tag, flags) & JBD_FLAG_LAST_TAG)
			tag_info->last_tag = true;

	}
	return EOK;
}

static int
jbd_write_block_tag(struct jbd_fs *jbd_fs,
		    void *__tag,
		    int32_t remain_buf_size,
		    struct tag_info *tag_info)
{
	char *uuid_start;
	int tag_bytes = jbd_tag_bytes(jbd_fs);

	tag_info->tag_bytes = tag_bytes;

	if (remain_buf_size - tag_bytes < 0)
		return EINVAL;

	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V3)) {
		struct jbd_block_tag3 *tag = __tag;
		jbd_set32(tag, blocknr, tag_info->block);
		if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
					     JBD_FEATURE_INCOMPAT_64BIT))
			jbd_set32(tag, blocknr_high, tag_info->block >> 32);

		if (tag_info->uuid_exist) {
			if (remain_buf_size - tag_bytes < UUID_SIZE)
				return EINVAL;

			uuid_start = (char *)tag + tag_bytes;
			tag_info->tag_bytes += UUID_SIZE;
			memcpy(uuid_start, tag_info->uuid, UUID_SIZE);
		} else
			jbd_set32(tag, flags,
				  jbd_get32(tag, flags) | JBD_FLAG_SAME_UUID);

		if (tag_info->last_tag)
			jbd_set32(tag, flags,
				  jbd_get32(tag, flags) | JBD_FLAG_LAST_TAG);

	} else {
		struct jbd_block_tag *tag = __tag;
		jbd_set32(tag, blocknr, tag_info->block);
		if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
					     JBD_FEATURE_INCOMPAT_64BIT))
			jbd_set32(tag, blocknr_high, tag_info->block >> 32);

		if (tag_info->uuid_exist) {
			if (remain_buf_size - tag_bytes < UUID_SIZE)
				return EINVAL;

			uuid_start = (char *)tag + tag_bytes;
			tag_info->tag_bytes += UUID_SIZE;
			memcpy(uuid_start, tag_info->uuid, UUID_SIZE);
		} else
			jbd_set16(tag, flags,
				  jbd_get16(tag, flags) | JBD_FLAG_SAME_UUID);

		if (tag_info->last_tag)
			jbd_set16(tag, flags,
				  jbd_get16(tag, flags) | JBD_FLAG_LAST_TAG);

	}
	return EOK;
}

static void
jbd_iterate_block_table(struct jbd_fs *jbd_fs,
			void *__tag_start,
			int32_t tag_tbl_size,
			void (*func)(struct jbd_fs * jbd_fs,
					ext4_fsblk_t block,
					uint8_t *uuid,
					void *arg),
			void *arg)
{
	char *tag_start, *tag_ptr;
	int tag_bytes = jbd_tag_bytes(jbd_fs);
	tag_start = __tag_start;
	tag_ptr = tag_start;

	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V2) ||
	    JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V3))
		tag_tbl_size -= sizeof(struct jbd_block_tail);

	while (tag_tbl_size) {
		struct tag_info tag_info;
		int rc = jbd_extract_block_tag(jbd_fs,
				      tag_ptr,
				      tag_bytes,
				      tag_tbl_size,
				      &tag_info);
		if (rc != EOK)
			break;

		if (func)
			func(jbd_fs, tag_info.block, tag_info.uuid, arg);

		if (tag_info.last_tag)
			break;

		tag_ptr += tag_info.tag_bytes;
		tag_tbl_size -= tag_info.tag_bytes;
	}
}

static void jbd_display_block_tags(struct jbd_fs *jbd_fs,
				   ext4_fsblk_t block,
				   uint8_t *uuid,
				   void *arg)
{
	uint32_t *iblock = arg;
	ext4_dbg(DEBUG_JBD, "Block in block_tag: %" PRIu64 "\n", block);
	(*iblock)++;
	(void)jbd_fs;
	(void)uuid;
	return;
}

static struct revoke_entry *
jbd_revoke_entry_lookup(struct recover_info *info, ext4_fsblk_t block)
{
	struct revoke_entry tmp = {
		.block = block
	};

	return RB_FIND(jbd_revoke, &info->revoke_root, &tmp);
}

static void jbd_replay_block_tags(struct jbd_fs *jbd_fs,
				  ext4_fsblk_t block,
				  uint8_t *uuid __unused,
				  void *__arg)
{
	int r;
	struct replay_arg *arg = __arg;
	struct recover_info *info = arg->info;
	uint32_t *this_block = arg->this_block;
	struct revoke_entry *revoke_entry;
	struct ext4_block journal_block, ext4_block;
	struct ext4_fs *fs = jbd_fs->inode_ref.fs;

	(*this_block)++;

	revoke_entry = jbd_revoke_entry_lookup(info, block);
	if (revoke_entry &&
	    arg->this_trans_id < revoke_entry->trans_id)
		return;

	ext4_dbg(DEBUG_JBD,
		 "Replaying block in block_tag: %" PRIu64 "\n",
		 block);

	r = jbd_block_get(jbd_fs, &journal_block, *this_block);
	if (r != EOK)
		return;

	if (block) {
		r = ext4_block_get_noread(fs->bdev, &ext4_block, block);
		if (r != EOK) {
			jbd_block_set(jbd_fs, &journal_block);
			return;
		}

		memcpy(ext4_block.data,
			journal_block.data,
			jbd_get32(&jbd_fs->sb, blocksize));

		ext4_bcache_set_dirty(ext4_block.buf);
		ext4_block_set(fs->bdev, &ext4_block);
	} else {
		uint16_t mount_count, state;
		mount_count = ext4_get16(&fs->sb, mount_count);
		state = ext4_get16(&fs->sb, state);

		memcpy(&fs->sb,
			journal_block.data + EXT4_SUPERBLOCK_OFFSET,
			EXT4_SUPERBLOCK_SIZE);

		/* Mark system as mounted */
		ext4_set16(&fs->sb, state, state);
		r = ext4_sb_write(fs->bdev, &fs->sb);
		if (r != EOK)
			return;

		/*Update mount count*/
		ext4_set16(&fs->sb, mount_count, mount_count);
	}

	jbd_block_set(jbd_fs, &journal_block);
	
	return;
}

static void jbd_add_revoke_block_tags(struct recover_info *info,
				      ext4_fsblk_t block)
{
	struct revoke_entry *revoke_entry;

	ext4_dbg(DEBUG_JBD, "Add block %" PRIu64 " to revoke tree\n", block);
	revoke_entry = jbd_revoke_entry_lookup(info, block);
	if (revoke_entry) {
		revoke_entry->trans_id = info->this_trans_id;
		return;
	}

	revoke_entry = jbd_alloc_revoke_entry();
	ext4_assert(revoke_entry);
	revoke_entry->block = block;
	revoke_entry->trans_id = info->this_trans_id;
	RB_INSERT(jbd_revoke, &info->revoke_root, revoke_entry);

	return;
}

static void jbd_destroy_revoke_tree(struct recover_info *info)
{
	while (!RB_EMPTY(&info->revoke_root)) {
		struct revoke_entry *revoke_entry =
			RB_MIN(jbd_revoke, &info->revoke_root);
		ext4_assert(revoke_entry);
		RB_REMOVE(jbd_revoke, &info->revoke_root, revoke_entry);
		jbd_free_revoke_entry(revoke_entry);
	}
}

/* Make sure we wrap around the log correctly! */
#define wrap(sb, var)						\
do {									\
	if (var >= jbd_get32((sb), maxlen))					\
		var -= (jbd_get32((sb), maxlen) - jbd_get32((sb), first));	\
} while (0)

#define ACTION_SCAN 0
#define ACTION_REVOKE 1
#define ACTION_RECOVER 2


static void jbd_build_revoke_tree(struct jbd_fs *jbd_fs,
				  struct jbd_bhdr *header,
				  struct recover_info *info)
{
	char *blocks_entry;
	struct jbd_revoke_header *revoke_hdr =
		(struct jbd_revoke_header *)header;
	uint32_t i, nr_entries, record_len = 4;
	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_64BIT))
		record_len = 8;

	nr_entries = (jbd_get32(revoke_hdr, count) -
			sizeof(struct jbd_revoke_header)) /
			record_len;

	blocks_entry = (char *)(revoke_hdr + 1);

	for (i = 0;i < nr_entries;i++) {
		if (record_len == 8) {
			uint64_t *blocks =
				(uint64_t *)blocks_entry;
			jbd_add_revoke_block_tags(info, to_be64(*blocks));
		} else {
			uint32_t *blocks =
				(uint32_t *)blocks_entry;
			jbd_add_revoke_block_tags(info, to_be32(*blocks));
		}
		blocks_entry += record_len;
	}
}

static void jbd_debug_descriptor_block(struct jbd_fs *jbd_fs,
				       struct jbd_bhdr *header,
				       uint32_t *iblock)
{
	jbd_iterate_block_table(jbd_fs,
				header + 1,
				jbd_get32(&jbd_fs->sb, blocksize) -
					sizeof(struct jbd_bhdr),
				jbd_display_block_tags,
				iblock);
}

static void jbd_replay_descriptor_block(struct jbd_fs *jbd_fs,
					struct jbd_bhdr *header,
					struct replay_arg *arg)
{
	jbd_iterate_block_table(jbd_fs,
				header + 1,
				jbd_get32(&jbd_fs->sb, blocksize) -
					sizeof(struct jbd_bhdr),
				jbd_replay_block_tags,
				arg);
}

int jbd_iterate_log(struct jbd_fs *jbd_fs,
		    struct recover_info *info,
		    int action)
{
	int r = EOK;
	bool log_end = false;
	struct jbd_sb *sb = &jbd_fs->sb;
	uint32_t start_trans_id, this_trans_id;
	uint32_t start_block, this_block;

	start_trans_id = this_trans_id = jbd_get32(sb, sequence);
	start_block = this_block = jbd_get32(sb, start);

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
		if (jbd_get32(header, magic) != JBD_MAGIC_NUMBER) {
			jbd_block_set(jbd_fs, &block);
			log_end = true;
			continue;
		}

		if (jbd_get32(header, sequence) != this_trans_id) {
			if (action != ACTION_SCAN)
				r = EIO;

			jbd_block_set(jbd_fs, &block);
			log_end = true;
			continue;
		}

		switch (jbd_get32(header, blocktype)) {
		case JBD_DESCRIPTOR_BLOCK:
			ext4_dbg(DEBUG_JBD, "Descriptor block: %" PRIu32", "
					    "trans_id: %" PRIu32"\n",
					    this_block, this_trans_id);
			if (action == ACTION_RECOVER) {
				struct replay_arg replay_arg;
				replay_arg.info = info;
				replay_arg.this_block = &this_block;
				replay_arg.this_trans_id = this_trans_id;

				jbd_replay_descriptor_block(jbd_fs,
						header, &replay_arg);
			} else
				jbd_debug_descriptor_block(jbd_fs,
						header, &this_block);

			break;
		case JBD_COMMIT_BLOCK:
			ext4_dbg(DEBUG_JBD, "Commit block: %" PRIu32", "
					    "trans_id: %" PRIu32"\n",
					    this_block, this_trans_id);
			this_trans_id++;
			break;
		case JBD_REVOKE_BLOCK:
			ext4_dbg(DEBUG_JBD, "Revoke block: %" PRIu32", "
					    "trans_id: %" PRIu32"\n",
					    this_block, this_trans_id);
			if (action == ACTION_REVOKE) {
				info->this_trans_id = this_trans_id;
				jbd_build_revoke_tree(jbd_fs,
						header, info);
			}
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

	RB_INIT(&info.revoke_root);

	r = jbd_iterate_log(jbd_fs, &info, ACTION_SCAN);
	if (r != EOK)
		return r;

	r = jbd_iterate_log(jbd_fs, &info, ACTION_REVOKE);
	if (r != EOK)
		return r;

	r = jbd_iterate_log(jbd_fs, &info, ACTION_RECOVER);
	if (r == EOK) {
		uint32_t features_incompatible =
			ext4_get32(&jbd_fs->inode_ref.fs->sb,
				   features_incompatible);
		jbd_set32(&jbd_fs->sb, start, 0);
		features_incompatible &= ~EXT4_FINCOM_RECOVER;
		ext4_set32(&jbd_fs->inode_ref.fs->sb,
			   features_incompatible,
			   features_incompatible);
		jbd_fs->dirty = true;
		r = ext4_sb_write(jbd_fs->inode_ref.fs->bdev,
				  &jbd_fs->inode_ref.fs->sb);
	}
	jbd_destroy_revoke_tree(&info);
	return r;
}

void jbd_journal_write_sb(struct jbd_journal *journal)
{
	struct jbd_fs *jbd_fs = journal->jbd_fs;
	jbd_set32(&jbd_fs->sb, start, journal->start);
	jbd_set32(&jbd_fs->sb, sequence, journal->trans_id);
	jbd_fs->dirty = true;
}

int jbd_journal_start(struct jbd_fs *jbd_fs,
		      struct jbd_journal *journal)
{
	int r;
	uint32_t features_incompatible =
			ext4_get32(&jbd_fs->inode_ref.fs->sb,
				   features_incompatible);
	features_incompatible |= EXT4_FINCOM_RECOVER;
	ext4_set32(&jbd_fs->inode_ref.fs->sb,
			features_incompatible,
			features_incompatible);
	r = ext4_sb_write(jbd_fs->inode_ref.fs->bdev,
			&jbd_fs->inode_ref.fs->sb);
	if (r != EOK)
		return r;

	journal->first = jbd_get32(&jbd_fs->sb, first);
	journal->start = journal->first;
	journal->last = journal->first;
	journal->trans_id = 1;
	journal->alloc_trans_id = 1;

	journal->block_size = jbd_get32(&jbd_fs->sb, blocksize);

	TAILQ_INIT(&journal->trans_queue);
	TAILQ_INIT(&journal->cp_queue);
	journal->jbd_fs = jbd_fs;
	jbd_journal_write_sb(journal);
	return jbd_write_sb(jbd_fs);
}

int jbd_journal_stop(struct jbd_journal *journal)
{
	int r;
	struct jbd_fs *jbd_fs = journal->jbd_fs;
	uint32_t features_incompatible =
			ext4_get32(&jbd_fs->inode_ref.fs->sb,
				   features_incompatible);
	features_incompatible &= ~EXT4_FINCOM_RECOVER;
	ext4_set32(&jbd_fs->inode_ref.fs->sb,
			features_incompatible,
			features_incompatible);
	r = ext4_sb_write(jbd_fs->inode_ref.fs->bdev,
			&jbd_fs->inode_ref.fs->sb);
	if (r != EOK)
		return r;

	journal->start = 0;
	journal->trans_id = 0;
	jbd_journal_write_sb(journal);
	return jbd_write_sb(journal->jbd_fs);
}

static uint32_t jbd_journal_alloc_block(struct jbd_journal *journal,
					struct jbd_trans *trans)
{
	uint32_t start_block;

	start_block = journal->last++;
	trans->alloc_blocks++;
	wrap(&journal->jbd_fs->sb, journal->last);
	if (journal->last == journal->start)
		ext4_block_cache_flush(journal->jbd_fs->inode_ref.fs->bdev);

	return start_block;
}

struct jbd_trans *
jbd_journal_new_trans(struct jbd_journal *journal)
{
	struct jbd_trans *trans = calloc(1, sizeof(struct jbd_trans));
	if (!trans)
		return NULL;

	/* We will assign a trans_id to this transaction,
	 * once it has been committed.*/
	trans->journal = journal;
	trans->error = EOK;
	return trans;
}

static void jbd_trans_end_write(struct ext4_bcache *bc __unused,
			  struct ext4_buf *buf __unused,
			  int res,
			  void *arg);

int jbd_trans_add_block(struct jbd_trans *trans,
			struct ext4_block *block)
{
	struct jbd_buf *buf;
	/* We do not need to add those unmodified buffer to
	 * a transaction. */
	if (!ext4_bcache_test_flag(block->buf, BC_DIRTY))
		return EOK;

	buf = calloc(1, sizeof(struct jbd_buf));
	if (!buf)
		return ENOMEM;

	buf->trans = trans;
	buf->block = *block;
	ext4_bcache_inc_ref(block->buf);

	block->buf->end_write = jbd_trans_end_write;
	block->buf->end_write_arg = trans;

	trans->data_cnt++;
	LIST_INSERT_HEAD(&trans->buf_list, buf, buf_node);
	return EOK;
}

int jbd_trans_revoke_block(struct jbd_trans *trans,
			   ext4_fsblk_t lba)
{
	struct jbd_revoke_rec *rec =
		calloc(1, sizeof(struct jbd_revoke_rec));
	if (!rec)
		return ENOMEM;

	rec->lba = lba;
	LIST_INSERT_HEAD(&trans->revoke_list, rec, revoke_node);
	return EOK;
}

void jbd_journal_free_trans(struct jbd_journal *journal,
			    struct jbd_trans *trans,
			    bool abort)
{
	struct jbd_buf *jbd_buf, *tmp;
	struct jbd_revoke_rec *rec, *tmp2;
	struct ext4_fs *fs = journal->jbd_fs->inode_ref.fs;
	LIST_FOREACH_SAFE(jbd_buf, &trans->buf_list, buf_node,
			  tmp) {
		if (abort)
			ext4_block_set(fs->bdev, &jbd_buf->block);

		LIST_REMOVE(jbd_buf, buf_node);
		free(jbd_buf);
	}
	LIST_FOREACH_SAFE(rec, &trans->revoke_list, revoke_node,
			  tmp2) {
		LIST_REMOVE(rec, revoke_node);
		free(rec);
	}

	free(trans);
}

static int jbd_trans_write_commit_block(struct jbd_trans *trans)
{
	int rc;
	struct jbd_commit_header *header;
	uint32_t commit_iblock = 0;
	struct ext4_block commit_block;
	struct jbd_journal *journal = trans->journal;

	commit_iblock = jbd_journal_alloc_block(journal, trans);
	rc = jbd_block_get_noread(journal->jbd_fs,
			&commit_block, commit_iblock);
	if (rc != EOK)
		return rc;

	header = (struct jbd_commit_header *)commit_block.data;
	jbd_set32(&header->header, magic, JBD_MAGIC_NUMBER);
	jbd_set32(&header->header, blocktype, JBD_COMMIT_BLOCK);
	jbd_set32(&header->header, sequence, trans->trans_id);

	ext4_bcache_set_dirty(commit_block.buf);
	rc = jbd_block_set(journal->jbd_fs, &commit_block);
	if (rc != EOK)
		return rc;

	return EOK;
}

static int jbd_journal_prepare(struct jbd_journal *journal,
			       struct jbd_trans *trans)
{
	int rc = EOK, i = 0;
	int32_t tag_tbl_size;
	uint32_t desc_iblock = 0;
	uint32_t data_iblock = 0;
	char *tag_start = NULL, *tag_ptr = NULL;
	struct jbd_buf *jbd_buf;
	struct ext4_block desc_block, data_block;

	LIST_FOREACH(jbd_buf, &trans->buf_list, buf_node) {
		struct tag_info tag_info;
		bool uuid_exist = false;
again:
		if (!desc_iblock) {
			struct jbd_bhdr *bhdr;
			desc_iblock = jbd_journal_alloc_block(journal, trans);
			rc = jbd_block_get_noread(journal->jbd_fs,
					   &desc_block, desc_iblock);
			if (rc != EOK)
				break;

			ext4_bcache_set_dirty(desc_block.buf);

			bhdr = (struct jbd_bhdr *)desc_block.data;
			jbd_set32(bhdr, magic, JBD_MAGIC_NUMBER);
			jbd_set32(bhdr, blocktype, JBD_DESCRIPTOR_BLOCK);
			jbd_set32(bhdr, sequence, trans->trans_id);

			tag_start = (char *)(bhdr + 1);
			tag_ptr = tag_start;
			uuid_exist = true;
			tag_tbl_size = journal->block_size -
				sizeof(struct jbd_bhdr);

			if (!trans->start_iblock)
				trans->start_iblock = desc_iblock;

		}
		tag_info.block = jbd_buf->block.lb_id;
		tag_info.uuid_exist = uuid_exist;
		if (i == trans->data_cnt - 1)
			tag_info.last_tag = true;

		if (uuid_exist)
			memcpy(tag_info.uuid, journal->jbd_fs->sb.uuid,
					UUID_SIZE);

		rc = jbd_write_block_tag(journal->jbd_fs,
				tag_ptr,
				tag_tbl_size,
				&tag_info);
		if (rc != EOK) {
			jbd_block_set(journal->jbd_fs, &desc_block);
			desc_iblock = 0;
			goto again;
		}

		data_iblock = jbd_journal_alloc_block(journal, trans);
		rc = jbd_block_get_noread(journal->jbd_fs,
				&data_block, data_iblock);
		if (rc != EOK)
			break;

		ext4_bcache_set_dirty(data_block.buf);

		memcpy(data_block.data, jbd_buf->block.data,
			journal->block_size);

		rc = jbd_block_set(journal->jbd_fs, &data_block);
		if (rc != EOK)
			break;

		tag_ptr += tag_info.tag_bytes;
		tag_tbl_size -= tag_info.tag_bytes;

		i++;
	}
	if (rc == EOK && desc_iblock)
		jbd_block_set(journal->jbd_fs, &desc_block);

	return rc;
}

static int
jbd_journal_prepare_revoke(struct jbd_journal *journal,
			   struct jbd_trans *trans)
{
	int rc = EOK, i = 0;
	int32_t tag_tbl_size;
	uint32_t desc_iblock = 0;
	char *blocks_entry = NULL;
	struct jbd_revoke_rec *rec, *tmp;
	struct ext4_block desc_block;
	struct jbd_revoke_header *header = NULL;
	int32_t record_len = 4;

	if (JBD_HAS_INCOMPAT_FEATURE(&journal->jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_64BIT))
		record_len = 8;

	LIST_FOREACH_SAFE(rec, &trans->revoke_list, revoke_node,
			  tmp) {
again:
		if (!desc_iblock) {
			struct jbd_bhdr *bhdr;
			desc_iblock = jbd_journal_alloc_block(journal, trans);
			rc = jbd_block_get_noread(journal->jbd_fs,
					   &desc_block, desc_iblock);
			if (rc != EOK) {
				break;
			}

			ext4_bcache_set_dirty(desc_block.buf);

			bhdr = (struct jbd_bhdr *)desc_block.data;
			jbd_set32(bhdr, magic, JBD_MAGIC_NUMBER);
			jbd_set32(bhdr, blocktype, JBD_REVOKE_BLOCK);
			jbd_set32(bhdr, sequence, trans->trans_id);
			
			header = (struct jbd_revoke_header *)bhdr;
			blocks_entry = (char *)(header + 1);
			tag_tbl_size = journal->block_size -
				sizeof(struct jbd_revoke_header);

			if (!trans->start_iblock)
				trans->start_iblock = desc_iblock;

		}

		if (tag_tbl_size < record_len) {
			jbd_set32(header, count,
				  journal->block_size - tag_tbl_size);
			jbd_block_set(journal->jbd_fs, &desc_block);
			desc_iblock = 0;
			header = NULL;
			goto again;
		}
		if (record_len == 8) {
			uint64_t *blocks =
				(uint64_t *)blocks_entry;
			*blocks = to_be64(rec->lba);
		} else {
			uint32_t *blocks =
				(uint32_t *)blocks_entry;
			*blocks = to_be32(rec->lba);
		}
		blocks_entry += record_len;
		tag_tbl_size -= record_len;

		i++;
	}
	if (rc == EOK && desc_iblock) {
		if (header != NULL)
			jbd_set32(header, count,
				  journal->block_size - tag_tbl_size);

		jbd_block_set(journal->jbd_fs, &desc_block);
	}

	return rc;
}

void
jbd_journal_submit_trans(struct jbd_journal *journal,
			 struct jbd_trans *trans)
{
	TAILQ_INSERT_TAIL(&journal->trans_queue,
			  trans,
			  trans_node);
}

void jbd_journal_cp_trans(struct jbd_journal *journal, struct jbd_trans *trans)
{
	struct jbd_buf *jbd_buf, *tmp;
	struct ext4_fs *fs = journal->jbd_fs->inode_ref.fs;
	LIST_FOREACH_SAFE(jbd_buf, &trans->buf_list, buf_node,
			tmp) {
		struct ext4_block block = jbd_buf->block;
		ext4_block_set(fs->bdev, &block);
	}
}

static void jbd_trans_end_write(struct ext4_bcache *bc __unused,
			  struct ext4_buf *buf __unused,
			  int res,
			  void *arg)
{
	struct jbd_trans *trans = arg;
	struct jbd_journal *journal = trans->journal;
	bool first_in_queue =
		trans == TAILQ_FIRST(&journal->cp_queue);
	if (res != EOK)
		trans->error = res;

	trans->written_cnt++;
	if (trans->written_cnt == trans->data_cnt) {
		TAILQ_REMOVE(&journal->cp_queue, trans, trans_node);

		if (first_in_queue) {
			journal->start = trans->start_iblock +
				trans->alloc_blocks;
			journal->trans_id = trans->trans_id + 1;
		}
		jbd_journal_free_trans(journal, trans, false);

		if (first_in_queue) {
			while ((trans = TAILQ_FIRST(&journal->cp_queue))) {
				if (!trans->data_cnt) {
					TAILQ_REMOVE(&journal->cp_queue,
						     trans,
						     trans_node);
					journal->start = trans->start_iblock +
						trans->alloc_blocks;
					journal->trans_id = trans->trans_id + 1;
					jbd_journal_free_trans(journal,
							       trans, false);
				} else {
					journal->start = trans->start_iblock;
					journal->trans_id = trans->trans_id;
					break;
				}
			}
			jbd_journal_write_sb(journal);
			jbd_write_sb(journal->jbd_fs);
		}
	}
}

/*
 * XXX: one should disable cache writeback first.
 */
void jbd_journal_commit_one(struct jbd_journal *journal)
{
	int rc = EOK;
	uint32_t last = journal->last;
	struct jbd_trans *trans;

	if ((trans = TAILQ_FIRST(&journal->trans_queue))) {
		TAILQ_REMOVE(&journal->trans_queue, trans, trans_node);

		trans->trans_id = journal->alloc_trans_id;
		rc = jbd_journal_prepare(journal, trans);
		if (rc != EOK)
			goto Finish;

		rc = jbd_journal_prepare_revoke(journal, trans);
		if (rc != EOK)
			goto Finish;

		rc = jbd_trans_write_commit_block(trans);
		if (rc != EOK)
			goto Finish;

		journal->alloc_trans_id++;
		if (TAILQ_EMPTY(&journal->cp_queue)) {
			if (trans->data_cnt) {
				journal->start = trans->start_iblock;
				journal->trans_id = trans->trans_id;
				jbd_journal_write_sb(journal);
				jbd_write_sb(journal->jbd_fs);
				TAILQ_INSERT_TAIL(&journal->cp_queue, trans,
						trans_node);
				jbd_journal_cp_trans(journal, trans);
			} else {
				journal->start = trans->start_iblock +
					trans->alloc_blocks;
				journal->trans_id = trans->trans_id + 1;
				jbd_journal_write_sb(journal);
				jbd_journal_free_trans(journal, trans, false);
			}
		} else {
			TAILQ_INSERT_TAIL(&journal->cp_queue, trans,
					trans_node);
			if (trans->data_cnt)
				jbd_journal_cp_trans(journal, trans);

		}
	}
Finish:
	if (rc != EOK) {
		journal->last = last;
		jbd_journal_free_trans(journal, trans, true);
	}
}

/**
 * @}
 */
