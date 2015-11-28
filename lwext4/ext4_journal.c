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
#include "tree.h"

#include <string.h>
#include <malloc.h>

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
	if (jbd_fs->dirty)
		jbd_sb_write(jbd_fs, &jbd_fs->sb);

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

static void
jbd_extract_block_tag(struct jbd_fs *jbd_fs,
		      uint32_t tag_bytes,
		      void *__tag,
		      ext4_fsblk_t *block,
		      bool *uuid_exist,
		      uint8_t *uuid,
		      bool *last_tag)
{
	char *uuid_start;
	*uuid_exist = false;
	*last_tag = false;
	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V3)) {
		struct jbd_block_tag3 *tag = __tag;
		*block = jbd_get32(tag, blocknr);
		if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
					     JBD_FEATURE_INCOMPAT_64BIT))
			 *block |= (uint64_t)jbd_get32(tag, blocknr_high) << 32;

		if (jbd_get32(tag, flags) & JBD_FLAG_ESCAPE)
			*block = 0;

		if (!(jbd_get32(tag, flags) & JBD_FLAG_SAME_UUID)) {
			uuid_start = (char *)tag + tag_bytes;
			*uuid_exist = true;
			memcpy(uuid, uuid_start, UUID_SIZE);
		}

		if (jbd_get32(tag, flags) & JBD_FLAG_LAST_TAG)
			*last_tag = true;

	} else {
		struct jbd_block_tag *tag = __tag;
		*block = jbd_get32(tag, blocknr);
		if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
					     JBD_FEATURE_INCOMPAT_64BIT))
			 *block |= (uint64_t)jbd_get32(tag, blocknr_high) << 32;

		if (jbd_get16(tag, flags) & JBD_FLAG_ESCAPE)
			*block = 0;

		if (!(jbd_get16(tag, flags) & JBD_FLAG_SAME_UUID)) {
			uuid_start = (char *)tag + tag_bytes;
			*uuid_exist = true;
			memcpy(uuid, uuid_start, UUID_SIZE);
		}

		if (jbd_get16(tag, flags) & JBD_FLAG_LAST_TAG)
			*last_tag = true;

	}
}

static void
jbd_iterate_block_table(struct jbd_fs *jbd_fs,
			void *__tag_start,
			uint32_t tag_tbl_size,
			void (*func)(struct jbd_fs * jbd_fs,
					ext4_fsblk_t block,
					uint8_t *uuid,
					void *arg),
			void *arg)
{
	ext4_fsblk_t block = 0;
	uint8_t uuid[UUID_SIZE];
	char *tag_start, *tag_ptr;
	uint32_t tag_bytes = jbd_tag_bytes(jbd_fs);
	tag_start = __tag_start;
	tag_ptr = tag_start;

	if (JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V2) ||
	    JBD_HAS_INCOMPAT_FEATURE(&jbd_fs->sb,
				     JBD_FEATURE_INCOMPAT_CSUM_V3))
		tag_tbl_size -= sizeof(struct jbd_block_tail);

	while (tag_ptr - tag_start + tag_bytes <= tag_tbl_size) {
		bool uuid_exist;
		bool last_tag;
		jbd_extract_block_tag(jbd_fs,
				      tag_bytes,
				      tag_ptr,
				      &block,
				      &uuid_exist,
				      uuid,
				      &last_tag);
		if (func)
			func(jbd_fs, block, uuid, arg);

		if (last_tag)
			break;

		tag_ptr += tag_bytes;
		if (uuid_exist)
			tag_ptr += UUID_SIZE;

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
	ext4_dbg(DEBUG_JBD,
		 "Replaying block in block_tag: %" PRIu64 "\n",
		 block);
	(*this_block)++;

	revoke_entry = jbd_revoke_entry_lookup(info, block);
	if (revoke_entry)
		return;

	r = jbd_block_get(jbd_fs, &journal_block, *this_block);
	if (r != EOK)
		return;

	r = ext4_block_get_noread(jbd_fs->bdev, &ext4_block, block);
	if (r != EOK) {
		jbd_block_set(jbd_fs, &journal_block);
		return;
	}

	memcpy(ext4_block.data,
	       journal_block.data,
	       jbd_get32(&jbd_fs->sb, blocksize));

	ext4_block.dirty = true;
	ext4_block_set(jbd_fs->bdev, &ext4_block);
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

	nr_entries = (revoke_hdr->count -
			sizeof(struct jbd_revoke_header)) /
			record_len;

	blocks_entry = (char *)(revoke_hdr + 1);

	for (i = 0;i < nr_entries;i++) {
		if (record_len == 8) {
			uint64_t *blocks =
				(uint64_t *)blocks_entry;
			jbd_add_revoke_block_tags(info, *blocks);
		} else {
			uint32_t *blocks =
				(uint32_t *)blocks_entry;
			jbd_add_revoke_block_tags(info, *blocks);
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
			ext4_dbg(DEBUG_JBD, "Descriptor block: %u, "
					    "trans_id: %u\n",
					    this_block, this_trans_id);
			if (action == ACTION_SCAN)
				jbd_debug_descriptor_block(jbd_fs,
						header, &this_block);
			else if (action == ACTION_RECOVER) {
				struct replay_arg replay_arg;
				replay_arg.info = info;
				replay_arg.this_block = &this_block;
				jbd_replay_descriptor_block(jbd_fs,
						header, &replay_arg);
			}

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
		jbd_set32(&jbd_fs->sb, start, 0);
		jbd_fs->dirty = true;
	}
	jbd_destroy_revoke_tree(&info);
	return r;
}
