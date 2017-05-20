/*
 * Copyright (c) 2017 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * Copyright (c) 2017 Kaho Ng (ngkaho1234@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_xattr.c
 * @brief Extended Attribute manipulation.
 */

#include <ext4_config.h>
#include <ext4_debug.h>
#include <ext4_errno.h>
#include <ext4_misc.h>
#include <ext4_types.h>

#include <ext4_balloc.h>
#include <ext4_block_group.h>
#include <ext4_blockdev.h>
#include <ext4_crc32.h>
#include <ext4_fs.h>
#include <ext4_inode.h>
#include <ext4_super.h>
#include <ext4_trans.h>
#include <ext4_xattr.h>

#include <stdlib.h>
#include <string.h>

#if CONFIG_XATTR_ENABLE

/**
 * @file  ext4_xattr.c
 * @brief Extended Attribute Manipulation
 */

/* Extended Attribute(EA) */

/* Magic value in attribute blocks */
#define EXT4_XATTR_MAGIC        0xEA020000

/* Maximum number of references to one attribute block */
#define EXT4_XATTR_REFCOUNT_MAX     1024

/* Name indexes */
#define EXT4_XATTR_INDEX_USER           1
#define EXT4_XATTR_INDEX_POSIX_ACL_ACCESS   2
#define EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT  3
#define EXT4_XATTR_INDEX_TRUSTED        4
#define EXT4_XATTR_INDEX_LUSTRE         5
#define EXT4_XATTR_INDEX_SECURITY           6
#define EXT4_XATTR_INDEX_SYSTEM         7
#define EXT4_XATTR_INDEX_RICHACL        8
#define EXT4_XATTR_INDEX_ENCRYPTION     9

#define EXT4_XATTR_PAD_BITS 2
#define EXT4_XATTR_PAD (1 << EXT4_XATTR_PAD_BITS)
#define EXT4_XATTR_ROUND (EXT4_XATTR_PAD - 1)
#define EXT4_XATTR_LEN(name_len)                                               \
    (((name_len) + EXT4_XATTR_ROUND + sizeof(struct ext4_xattr_entry)) &   \
     ~EXT4_XATTR_ROUND)
#define EXT4_XATTR_NEXT(entry)                                                 \
    ((struct ext4_xattr_entry *)((char *)(entry) +                         \
                     EXT4_XATTR_LEN((entry)->e_name_len)))
#define EXT4_XATTR_SIZE(size) (((size) + EXT4_XATTR_ROUND) & ~EXT4_XATTR_ROUND)
#define EXT4_XATTR_NAME(entry) ((char *)((entry) + 1))

#define EXT4_XATTR_IHDR(sb, raw_inode)                                         \
    ((struct ext4_xattr_ibody_header *)((char *)raw_inode +                \
                        EXT4_GOOD_OLD_INODE_SIZE +         \
                        ext4_inode_get_extra_isize(        \
                        sb, raw_inode)))
#define EXT4_XATTR_IFIRST(hdr) ((struct ext4_xattr_entry *)((hdr) + 1))

#define EXT4_XATTR_BHDR(block) ((struct ext4_xattr_header *)((block)->data))
#define EXT4_XATTR_ENTRY(ptr) ((struct ext4_xattr_entry *)(ptr))
#define EXT4_XATTR_BFIRST(block) EXT4_XATTR_ENTRY(EXT4_XATTR_BHDR(block) + 1)
#define EXT4_XATTR_IS_LAST_ENTRY(entry) (*(uint32_t *)(entry) == 0)

#define EXT4_ZERO_XATTR_VALUE ((void *)-1)

#pragma pack(push, 1)

struct ext4_xattr_header {
    uint32_t h_magic;   /* magic number for identification */
    uint32_t h_refcount;    /* reference count */
    uint32_t h_blocks;  /* number of disk blocks used */
    uint32_t h_hash;        /* hash value of all attributes */
    uint32_t h_checksum;    /* crc32c(uuid+id+xattrblock) */
                /* id = inum if refcount=1, blknum otherwise */
    uint32_t h_reserved[3]; /* zero right now */
};

struct ext4_xattr_ibody_header {
    uint32_t h_magic;   /* magic number for identification */
};

struct ext4_xattr_entry {
    uint8_t e_name_len; /* length of name */
    uint8_t e_name_index;   /* attribute name index */
    uint16_t e_value_offs;  /* offset in disk block of value */
    uint32_t e_value_block; /* disk block attribute is stored on (n/i) */
    uint32_t e_value_size;  /* size of attribute value */
    uint32_t e_hash;        /* hash value of name and value */
};

#pragma pack(pop)


#define NAME_HASH_SHIFT 5
#define VALUE_HASH_SHIFT 16

static inline void ext4_xattr_compute_hash(struct ext4_xattr_header *header,
					   struct ext4_xattr_entry *entry)
{
	uint32_t hash = 0;
	char *name = EXT4_XATTR_NAME(entry);
	int n;

	for (n = 0; n < entry->e_name_len; n++) {
		hash = (hash << NAME_HASH_SHIFT) ^
		       (hash >> (8 * sizeof(hash) - NAME_HASH_SHIFT)) ^ *name++;
	}

	if (entry->e_value_block == 0 && entry->e_value_size != 0) {
		uint32_t *value =
		    (uint32_t *)((char *)header + to_le16(entry->e_value_offs));
		for (n = (to_le32(entry->e_value_size) + EXT4_XATTR_ROUND) >>
			 EXT4_XATTR_PAD_BITS;
		     n; n--) {
			hash = (hash << VALUE_HASH_SHIFT) ^
			       (hash >> (8 * sizeof(hash) - VALUE_HASH_SHIFT)) ^
			       to_le32(*value++);
		}
	}
	entry->e_hash = to_le32(hash);
}

#define BLOCK_HASH_SHIFT 16

/*
 * ext4_xattr_rehash()
 *
 * Re-compute the extended attribute hash value after an entry has changed.
 */
static void ext4_xattr_rehash(struct ext4_xattr_header *header,
			      struct ext4_xattr_entry *entry)
{
	struct ext4_xattr_entry *here;
	uint32_t hash = 0;

	ext4_xattr_compute_hash(header, entry);
	here = EXT4_XATTR_ENTRY(header + 1);
	while (!EXT4_XATTR_IS_LAST_ENTRY(here)) {
		if (!here->e_hash) {
			/* Block is not shared if an entry's hash value == 0 */
			hash = 0;
			break;
		}
		hash = (hash << BLOCK_HASH_SHIFT) ^
		       (hash >> (8 * sizeof(hash) - BLOCK_HASH_SHIFT)) ^
		       to_le32(here->e_hash);
		here = EXT4_XATTR_NEXT(here);
	}
	header->h_hash = to_le32(hash);
}

#if CONFIG_META_CSUM_ENABLE
static uint32_t ext4_xattr_block_checksum(struct ext4_inode_ref *inode_ref,
					  ext4_fsblk_t blocknr,
					  struct ext4_xattr_header *header)
{
	uint32_t checksum = 0;
	uint64_t le64_blocknr = blocknr;
	struct ext4_sblock *sb = &inode_ref->fs->sb;

	if (ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM)) {
		uint32_t orig_checksum;

		/* Preparation: temporarily set bg checksum to 0 */
		orig_checksum = header->h_checksum;
		header->h_checksum = 0;
		/* First calculate crc32 checksum against fs uuid */
		checksum =
		    ext4_crc32c(EXT4_CRC32_INIT, sb->uuid, sizeof(sb->uuid));
		/* Then calculate crc32 checksum block number */
		checksum =
		    ext4_crc32c(checksum, &le64_blocknr, sizeof(le64_blocknr));
		/* Finally calculate crc32 checksum against
		 * the entire xattr block */
		checksum =
		    ext4_crc32c(checksum, header, ext4_sb_get_block_size(sb));
		header->h_checksum = orig_checksum;
	}
	return checksum;
}
#else
#define ext4_xattr_block_checksum(...) 0
#endif

static void ext4_xattr_set_block_checksum(struct ext4_inode_ref *inode_ref,
					  ext4_fsblk_t blocknr __unused,
					  struct ext4_xattr_header *header)
{
	struct ext4_sblock *sb = &inode_ref->fs->sb;
	if (!ext4_sb_feature_ro_com(sb, EXT4_FRO_COM_METADATA_CSUM))
		return;

	header->h_checksum =
	    ext4_xattr_block_checksum(inode_ref, blocknr, header);
}

struct xattr_prefix {
	const char *prefix;
	uint8_t name_index;
};

static const struct xattr_prefix prefix_tbl[] = {
    {"user.", EXT4_XATTR_INDEX_USER},
    {"system.posix_acl_access", EXT4_XATTR_INDEX_POSIX_ACL_ACCESS},
    {"system.posix_acl_default", EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT},
    {"trusted.", EXT4_XATTR_INDEX_TRUSTED},
    {"security.", EXT4_XATTR_INDEX_SECURITY},
    {"system.", EXT4_XATTR_INDEX_SYSTEM},
    {"system.richacl", EXT4_XATTR_INDEX_RICHACL},
    {NULL, 0},
};

const char *ext4_extract_xattr_name(const char *full_name, size_t full_name_len,
				    uint8_t *name_index, size_t *name_len,
				    bool *found)
{
	int i;
	ext4_assert(name_index);
	ext4_assert(found);

	*found = false;

	if (!full_name_len) {
		if (name_len)
			*name_len = 0;

		return NULL;
	}

	for (i = 0; prefix_tbl[i].prefix; i++) {
		size_t prefix_len = strlen(prefix_tbl[i].prefix);
		if (full_name_len >= prefix_len &&
		    !memcmp(full_name, prefix_tbl[i].prefix, prefix_len)) {
			bool require_name =
			    prefix_tbl[i].prefix[prefix_len - 1] == '.';
			*name_index = prefix_tbl[i].name_index;
			if (name_len)
				*name_len = full_name_len - prefix_len;

			if (!(full_name_len - prefix_len) && require_name)
				return NULL;

			*found = true;
			if (require_name)
				return full_name + prefix_len;

			return NULL;
		}
	}
	if (name_len)
		*name_len = 0;

	return NULL;
}

const char *ext4_get_xattr_name_prefix(uint8_t name_index,
				       size_t *ret_prefix_len)
{
	int i;

	for (i = 0; prefix_tbl[i].prefix; i++) {
		size_t prefix_len = strlen(prefix_tbl[i].prefix);
		if (prefix_tbl[i].name_index == name_index) {
			if (ret_prefix_len)
				*ret_prefix_len = prefix_len;

			return prefix_tbl[i].prefix;
		}
	}
	if (ret_prefix_len)
		*ret_prefix_len = 0;

	return NULL;
}

static const char ext4_xattr_empty_value;

/**
 * @brief Insert/Remove/Modify the given entry
 *
 * @param i The information of the given EA entry
 * @param s Search context block
 * @param dry_run Do not modify the content of the buffer
 *
 * @return Return EOK when finished, ENOSPC when there is no enough space
 */
static int ext4_xattr_set_entry(struct ext4_xattr_info *i,
				struct ext4_xattr_search *s, bool dry_run)
{
	struct ext4_xattr_entry *last;
	size_t free, min_offs = (char *)s->end - (char *)s->base,
		     name_len = i->name_len;

	/*
	 * If the entry is going to be removed but not found, return 0 to
	 * indicate success.
	 */
	if (!i->value && s->not_found)
		return EOK;

	/* Compute min_offs and last. */
	last = s->first;
	for (; !EXT4_XATTR_IS_LAST_ENTRY(last); last = EXT4_XATTR_NEXT(last)) {
		if (last->e_value_size) {
			size_t offs = to_le16(last->e_value_offs);
			if (offs < min_offs)
				min_offs = offs;
		}
	}

	/* Calculate free space in the block. */
	free = min_offs - ((char *)last - (char *)s->base) - sizeof(uint32_t);
	if (!s->not_found)
		free += EXT4_XATTR_SIZE(s->here->e_value_size) +
			EXT4_XATTR_LEN(s->here->e_name_len);

	if (i->value) {
		/* See whether there is enough space to hold new entry */
		if (free <
		    EXT4_XATTR_SIZE(i->value_len) + EXT4_XATTR_LEN(name_len))
			return ENOSPC;
	}

	/* Return EOK now if we do not intend to modify the content. */
	if (dry_run)
		return EOK;

	/* First remove the old entry's data part */
	if (!s->not_found) {
		size_t value_offs = to_le16(s->here->e_value_offs);
		void *value = (char *)s->base + value_offs;
		void *first_value = (char *)s->base + min_offs;
		size_t value_size =
		    EXT4_XATTR_SIZE(to_le32(s->here->e_value_size));

		if (value_offs) {
			/* Remove the data part. */
			memmove((char *)first_value + value_size, first_value,
				(char *)value - (char *)first_value);

			/* Zero the gap created */
			memset(first_value, 0, value_size);

			/*
			 * Calculate the new min_offs after removal of the old
			 * entry's data part
			 */
			min_offs += value_size;
		}

		/*
		 * Adjust the value offset of entries which has value offset
		 * prior to the s->here. The offset of these entries won't be
		 * shifted if the size of the entry we removed is zero.
		 */
		for (last = s->first; !EXT4_XATTR_IS_LAST_ENTRY(last);
		     last = EXT4_XATTR_NEXT(last)) {
			size_t offs = to_le16(last->e_value_offs);

			/* For zero-value-length entry, offs will be zero. */
			if (offs < value_offs)
				last->e_value_offs = to_le16(offs + value_size);
		}
	}

	/* If caller wants us to insert... */
	if (i->value) {
		size_t value_offs;
		if (i->value_len)
			value_offs = min_offs - EXT4_XATTR_SIZE(i->value_len);
		else
			value_offs = 0;

		if (!s->not_found) {
			struct ext4_xattr_entry *here = s->here;

			/* Reuse the current entry we have got */
			here->e_value_offs = to_le16(value_offs);
			here->e_value_size = to_le32(i->value_len);
		} else {
			/* Insert a new entry */
			last->e_name_len = (uint8_t)name_len;
			last->e_name_index = i->name_index;
			last->e_value_offs = to_le16(value_offs);
			last->e_value_block = 0;
			last->e_value_size = to_le32(i->value_len);
			memcpy(EXT4_XATTR_NAME(last), i->name, name_len);

			/* Set valid last entry indicator */
			*(uint32_t *)EXT4_XATTR_NEXT(last) = 0;

			s->here = last;
		}

		/* Insert the value's part */
		if (value_offs) {
			memcpy((char *)s->base + value_offs, i->value,
			       i->value_len);

			/* Clear the padding bytes if there is */
			if (EXT4_XATTR_SIZE(i->value_len) != i->value_len)
				memset((char *)s->base + value_offs +
					   i->value_len,
				       0, EXT4_XATTR_SIZE(i->value_len) -
					      i->value_len);
		}
	} else {
		size_t shift_offs;

		/* Remove the whole entry */
		shift_offs = (char *)EXT4_XATTR_NEXT(s->here) - (char *)s->here;
		memmove(s->here, EXT4_XATTR_NEXT(s->here),
			(char *)last + sizeof(uint32_t) -
			    (char *)EXT4_XATTR_NEXT(s->here));

		/* Zero the gap created */
		memset((char *)last - shift_offs + sizeof(uint32_t), 0,
		       shift_offs);

		s->here = NULL;
	}

	return EOK;
}

static inline bool ext4_xattr_is_empty(struct ext4_xattr_search *s)
{
	if (!EXT4_XATTR_IS_LAST_ENTRY(s->first))
		return false;

	return true;
}

/**
 * @brief Find the entry according to given information
 *
 * @param i The information of the EA entry to be found,
 * 	    including name_index, name and the length of name
 * @param s Search context block
 */
static void ext4_xattr_find_entry(struct ext4_xattr_info *i,
				  struct ext4_xattr_search *s)
{
	struct ext4_xattr_entry *entry = NULL;

	s->not_found = true;
	s->here = NULL;

	/*
	 * Find the wanted EA entry by simply comparing the namespace,
	 * name and the length of name.
	 */
	for (entry = s->first; !EXT4_XATTR_IS_LAST_ENTRY(entry);
	     entry = EXT4_XATTR_NEXT(entry)) {
		size_t name_len = entry->e_name_len;
		const char *name = EXT4_XATTR_NAME(entry);
		if (name_len == i->name_len &&
		    entry->e_name_index == i->name_index &&
		    !memcmp(name, i->name, name_len)) {
			s->here = entry;
			s->not_found = false;
			i->value_len = to_le32(entry->e_value_size);
			if (i->value_len)
				i->value = (char *)s->base +
					   to_le16(entry->e_value_offs);
			else
				i->value = NULL;

			return;
		}
	}
}

/**
 * @brief Check whether the xattr block's content is valid
 *
 * @param inode_ref Inode reference
 * @param block     The block buffer to be validated
 *
 * @return true if @block is valid, false otherwise.
 */
static bool ext4_xattr_is_block_valid(struct ext4_inode_ref *inode_ref,
				      struct ext4_block *block)
{

	void *base = block->data,
	     *end = block->data + ext4_sb_get_block_size(&inode_ref->fs->sb);
	size_t min_offs = (char *)end - (char *)base;
	struct ext4_xattr_header *header = EXT4_XATTR_BHDR(block);
	struct ext4_xattr_entry *entry = EXT4_XATTR_BFIRST(block);

	/*
	 * Check whether the magic number in the header is correct.
	 */
	if (header->h_magic != to_le32(EXT4_XATTR_MAGIC))
		return false;

	/*
	 * The in-kernel filesystem driver only supports 1 block currently.
	 */
	if (header->h_blocks != to_le32(1))
		return false;

	/*
	 * Check if those entries are maliciously corrupted to inflict harm
	 * upon us.
	 */
	for (; !EXT4_XATTR_IS_LAST_ENTRY(entry);
	     entry = EXT4_XATTR_NEXT(entry)) {
		if (!to_le32(entry->e_value_size) &&
		    to_le16(entry->e_value_offs))
			return false;

		if ((char *)base + to_le16(entry->e_value_offs) +
			to_le32(entry->e_value_size) >
		    (char *)end)
			return false;

		/*
		 * The name length field should also be correct,
		 * also there should be an 4-byte zero entry at the
		 * end.
		 */
		if ((char *)EXT4_XATTR_NEXT(entry) + sizeof(uint32_t) >
		    (char *)end)
			return false;

		if (to_le32(entry->e_value_size)) {
			size_t offs = to_le16(entry->e_value_offs);
			if (offs < min_offs)
				min_offs = offs;
		}
	}
	/*
	 * Entry field and data field do not override each other.
	 */
	if ((char *)base + min_offs < (char *)entry + sizeof(uint32_t))
		return false;

	return true;
}

/**
 * @brief Check whether the inode buffer's content is valid
 *
 * @param inode_ref Inode reference
 *
 * @return true if the inode buffer is valid, false otherwise.
 */
static bool ext4_xattr_is_ibody_valid(struct ext4_inode_ref *inode_ref)
{
	size_t min_offs;
	void *base, *end;
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_xattr_ibody_header *iheader;
	struct ext4_xattr_entry *entry;
	size_t inode_size = ext4_get16(&fs->sb, inode_size);

	iheader = EXT4_XATTR_IHDR(&fs->sb, inode_ref->inode);
	entry = EXT4_XATTR_IFIRST(iheader);
	base = iheader;
	end = (char *)inode_ref->inode + inode_size;
	min_offs = (char *)end - (char *)base;

	/*
	 * Check whether the magic number in the header is correct.
	 */
	if (iheader->h_magic != to_le32(EXT4_XATTR_MAGIC))
		return false;

	/*
	 * Check if those entries are maliciously corrupted to inflict harm
	 * upon us.
	 */
	for (; !EXT4_XATTR_IS_LAST_ENTRY(entry);
	     entry = EXT4_XATTR_NEXT(entry)) {
		if (!to_le32(entry->e_value_size) &&
		    to_le16(entry->e_value_offs))
			return false;

		if ((char *)base + to_le16(entry->e_value_offs) +
			to_le32(entry->e_value_size) >
		    (char *)end)
			return false;

		/*
		 * The name length field should also be correct,
		 * also there should be an 4-byte zero entry at the
		 * end.
		 */
		if ((char *)EXT4_XATTR_NEXT(entry) + sizeof(uint32_t) >
		    (char *)end)
			return false;

		if (to_le32(entry->e_value_size)) {
			size_t offs = to_le16(entry->e_value_offs);
			if (offs < min_offs)
				min_offs = offs;
		}
	}
	/*
	 * Entry field and data field do not override each other.
	 */
	if ((char *)base + min_offs < (char *)entry + sizeof(uint32_t))
		return false;

	return true;
}

/**
 * @brief An EA entry finder for inode buffer
 */
struct ext4_xattr_finder {
	/**
	 * @brief The information of the EA entry to be find
	 */
	struct ext4_xattr_info i;

	/**
	 * @brief Search context block of the current search
	 */
	struct ext4_xattr_search s;

	/**
	 * @brief Inode reference to the corresponding inode
	 */
	struct ext4_inode_ref *inode_ref;
};

static void ext4_xattr_ibody_initialize(struct ext4_inode_ref *inode_ref)
{
	struct ext4_xattr_ibody_header *header;
	struct ext4_fs *fs = inode_ref->fs;
	size_t extra_isize =
	    ext4_inode_get_extra_isize(&fs->sb, inode_ref->inode);
	size_t inode_size = ext4_get16(&fs->sb, inode_size);
	if (!extra_isize)
		return;

	header = EXT4_XATTR_IHDR(&fs->sb, inode_ref->inode);
	memset(header, 0, inode_size - EXT4_GOOD_OLD_INODE_SIZE - extra_isize);
	header->h_magic = to_le32(EXT4_XATTR_MAGIC);
	inode_ref->dirty = true;
}

/**
 * @brief Initialize a given xattr block
 *
 * @param inode_ref Inode reference
 * @param block xattr block buffer
 */
static void ext4_xattr_block_initialize(struct ext4_inode_ref *inode_ref,
					struct ext4_block *block)
{
	struct ext4_xattr_header *header;
	struct ext4_fs *fs = inode_ref->fs;

	memset(block->data, 0, ext4_sb_get_block_size(&fs->sb));

	header = EXT4_XATTR_BHDR(block);
	header->h_magic = to_le32(EXT4_XATTR_MAGIC);
	header->h_refcount = to_le32(1);
	header->h_blocks = to_le32(1);

	ext4_trans_set_block_dirty(block->buf);
}

static void ext4_xattr_block_init_search(struct ext4_inode_ref *inode_ref,
					 struct ext4_xattr_search *s,
					 struct ext4_block *block)
{
	s->base = block->data;
	s->end = block->data + ext4_sb_get_block_size(&inode_ref->fs->sb);
	s->first = EXT4_XATTR_BFIRST(block);
	s->here = NULL;
	s->not_found = true;
}

/**
 * @brief Find an EA entry inside a xattr block
 *
 * @param inode_ref Inode reference
 * @param finder    The caller-provided finder block with
 * 		    information filled
 * @param block     The block buffer to be looked into
 *
 * @return Return EOK no matter the entry is found or not.
 * 	   If the IO operation or the buffer validation failed,
 * 	   return other value.
 */
static int ext4_xattr_block_find_entry(struct ext4_inode_ref *inode_ref,
				       struct ext4_xattr_finder *finder,
				       struct ext4_block *block)
{
	int ret = EOK;

	/* Initialize the caller-given finder */
	finder->inode_ref = inode_ref;
	memset(&finder->s, 0, sizeof(finder->s));

	if (ret != EOK)
		return ret;

	/* Check the validity of the buffer */
	if (!ext4_xattr_is_block_valid(inode_ref, block))
		return EIO;

	ext4_xattr_block_init_search(inode_ref, &finder->s, block);
	ext4_xattr_find_entry(&finder->i, &finder->s);
	return EOK;
}

/**
 * @brief Find an EA entry inside an inode's extra space
 *
 * @param inode_ref Inode reference
 * @param finder    The caller-provided finder block with
 * 		    information filled
 *
 * @return Return EOK no matter the entry is found or not.
 * 	   If the IO operation or the buffer validation failed,
 * 	   return other value.
 */
static int ext4_xattr_ibody_find_entry(struct ext4_inode_ref *inode_ref,
				       struct ext4_xattr_finder *finder)
{
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_xattr_ibody_header *iheader;
	size_t extra_isize =
	    ext4_inode_get_extra_isize(&fs->sb, inode_ref->inode);
	size_t inode_size = ext4_get16(&fs->sb, inode_size);

	/* Initialize the caller-given finder */
	finder->inode_ref = inode_ref;
	memset(&finder->s, 0, sizeof(finder->s));

	/*
	 * If there is no extra inode space
	 * set ext4_xattr_ibody_finder::s::not_found to true and return EOK
	 */
	if (!extra_isize) {
		finder->s.not_found = true;
		return EOK;
	}

	/* Check the validity of the buffer */
	if (!ext4_xattr_is_ibody_valid(inode_ref))
		return EIO;

	iheader = EXT4_XATTR_IHDR(&fs->sb, inode_ref->inode);
	finder->s.base = EXT4_XATTR_IFIRST(iheader);
	finder->s.end = (char *)inode_ref->inode + inode_size;
	finder->s.first = EXT4_XATTR_IFIRST(iheader);
	ext4_xattr_find_entry(&finder->i, &finder->s);
	return EOK;
}

/**
 * @brief Try to allocate a block holding EA entries.
 *
 * @param inode_ref Inode reference
 *
 * @return Error code
 */
static int ext4_xattr_try_alloc_block(struct ext4_inode_ref *inode_ref)
{
	int ret = EOK;

	ext4_fsblk_t xattr_block = 0;
	xattr_block =
	    ext4_inode_get_file_acl(inode_ref->inode, &inode_ref->fs->sb);

	/*
	 * Only allocate a xattr block when there is no xattr block
	 * used by the inode.
	 */
	if (!xattr_block) {
		ext4_fsblk_t goal = ext4_fs_inode_to_goal_block(inode_ref);

		ret = ext4_balloc_alloc_block(inode_ref, goal, &xattr_block);
		if (ret != EOK)
			goto Finish;

		ext4_inode_set_file_acl(inode_ref->inode, &inode_ref->fs->sb,
					xattr_block);
	}

Finish:
	return ret;
}

/**
 * @brief Try to free a block holding EA entries.
 *
 * @param inode_ref Inode reference
 *
 * @return Error code
 */
static void ext4_xattr_try_free_block(struct ext4_inode_ref *inode_ref)
{
	ext4_fsblk_t xattr_block;
	xattr_block =
	    ext4_inode_get_file_acl(inode_ref->inode, &inode_ref->fs->sb);
	/*
	 * Free the xattr block used by the inode when there is one.
	 */
	if (xattr_block) {
		ext4_inode_set_file_acl(inode_ref->inode, &inode_ref->fs->sb,
					0);
		ext4_balloc_free_block(inode_ref, xattr_block);
		inode_ref->dirty = true;
	}
}

/**
 * @brief Put a list of EA entries into a caller-provided buffer
 * 	  In order to make sure that @list buffer can fit in the data,
 * 	  the routine should be called twice.
 *
 * @param inode_ref Inode reference
 * @param list A caller-provided buffer to hold a list of EA entries.
 * 	       If list == NULL, list_len will contain the size of
 * 	       the buffer required to hold these entries
 * @param list_len The length of the data written to @list
 * @return Error code
 */
int ext4_xattr_list(struct ext4_inode_ref *inode_ref,
		    struct ext4_xattr_list_entry *list, size_t *list_len)
{
	int ret = EOK;
	size_t buf_len = 0;
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_xattr_ibody_header *iheader;
	size_t extra_isize =
	    ext4_inode_get_extra_isize(&fs->sb, inode_ref->inode);
	struct ext4_block block;
	bool block_loaded = false;
	ext4_fsblk_t xattr_block = 0;
	struct ext4_xattr_entry *entry;
	struct ext4_xattr_list_entry *list_prev = NULL;
	xattr_block = ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);

	/*
	 * If there is extra inode space and the xattr buffer in the
	 * inode is valid.
	 */
	if (extra_isize && ext4_xattr_is_ibody_valid(inode_ref)) {
		iheader = EXT4_XATTR_IHDR(&fs->sb, inode_ref->inode);
		entry = EXT4_XATTR_IFIRST(iheader);

		/*
		 * The format of the list should be like this:
		 *
		 * name_len indicates the length in bytes of the name
		 * of the EA entry. The string is null-terminated.
		 *
		 * list->name => (char *)(list + 1);
		 * list->next => (void *)((char *)(list + 1) + name_len + 1);
		 */
		for (; !EXT4_XATTR_IS_LAST_ENTRY(entry);
		     entry = EXT4_XATTR_NEXT(entry)) {
			size_t name_len = entry->e_name_len;
			if (list) {
				list->name_index = entry->e_name_index;
				list->name_len = name_len;
				list->name = (char *)(list + 1);
				memcpy(list->name, EXT4_XATTR_NAME(entry),
				       list->name_len);

				if (list_prev)
					list_prev->next = list;

				list_prev = list;
				list = (struct ext4_xattr_list_entry
					    *)(list->name + name_len + 1);
			}

			/*
			 * Size calculation by pointer arithmetics.
			 */
			buf_len +=
			    (char *)((struct ext4_xattr_list_entry *)0 + 1) +
			    name_len + 1 -
			    (char *)(struct ext4_xattr_list_entry *)0;
		}
	}

	/*
	 * If there is a xattr block used by the inode
	 */
	if (xattr_block) {
		ret = ext4_trans_block_get(fs->bdev, &block, xattr_block);
		if (ret != EOK)
			goto out;

		block_loaded = true;

		/*
		 * As we don't allow the content in the block being invalid,
		 * bail out.
		 */
		if (!ext4_xattr_is_block_valid(inode_ref, &block)) {
			ret = EIO;
			goto out;
		}

		entry = EXT4_XATTR_BFIRST(&block);

		/*
		 * The format of the list should be like this:
		 *
		 * name_len indicates the length in bytes of the name
		 * of the EA entry. The string is null-terminated.
		 *
		 * list->name => (char *)(list + 1);
		 * list->next => (void *)((char *)(list + 1) + name_len + 1);
		 *
		 * Same as above actually.
		 */
		for (; !EXT4_XATTR_IS_LAST_ENTRY(entry);
		     entry = EXT4_XATTR_NEXT(entry)) {
			size_t name_len = entry->e_name_len;
			if (list) {
				list->name_index = entry->e_name_index;
				list->name_len = name_len;
				list->name = (char *)(list + 1);
				memcpy(list->name, EXT4_XATTR_NAME(entry),
				       list->name_len);

				if (list_prev)
					list_prev->next = list;

				list_prev = list;
				list = (struct ext4_xattr_list_entry
					    *)(list->name + name_len + 1);
			}

			/*
			 * Size calculation by pointer arithmetics.
			 */
			buf_len +=
			    (char *)((struct ext4_xattr_list_entry *)0 + 1) +
			    name_len + 1 -
			    (char *)(struct ext4_xattr_list_entry *)0;
		}
	}
	if (list_prev)
		list_prev->next = NULL;
out:
	if (ret == EOK && list_len)
		*list_len = buf_len;

	if (block_loaded)
		ext4_block_set(fs->bdev, &block);

	return ret;
}

/**
 * @brief Query EA entry's value with given name-index and name
 *
 * @param inode_ref Inode reference
 * @param name_index Name-index
 * @param name Name of the EA entry to be queried
 * @param name_len Length of name in bytes
 * @param buf Output buffer to hold content
 * @param buf_len Output buffer's length
 * @param data_len The length of data of the EA entry found
 *
 * @return Error code
 */
int ext4_xattr_get(struct ext4_inode_ref *inode_ref, uint8_t name_index,
		   const char *name, size_t name_len, void *buf, size_t buf_len,
		   size_t *data_len)
{
	int ret = EOK;
	struct ext4_xattr_finder ibody_finder;
	struct ext4_xattr_finder block_finder;
	struct ext4_xattr_info i;
	size_t value_len = 0;
	size_t value_offs = 0;
	struct ext4_fs *fs = inode_ref->fs;
	ext4_fsblk_t xattr_block;
	xattr_block = ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);

	i.name_index = name_index;
	i.name = name;
	i.name_len = name_len;
	i.value = 0;
	i.value_len = 0;
	if (data_len)
		*data_len = 0;

	ibody_finder.i = i;
	ret = ext4_xattr_ibody_find_entry(inode_ref, &ibody_finder);
	if (ret != EOK)
		goto out;

	if (!ibody_finder.s.not_found) {
		value_len = to_le32(ibody_finder.s.here->e_value_size);
		value_offs = to_le32(ibody_finder.s.here->e_value_offs);
		if (buf_len && buf) {
			void *data_loc =
			    (char *)ibody_finder.s.base + value_offs;
			memcpy(buf, data_loc,
			       (buf_len < value_len) ? buf_len : value_len);
		}
	} else {
		struct ext4_block block;

		/* Return ENODATA if there is no EA block */
		if (!xattr_block) {
			ret = ENODATA;
			goto out;
		}

		block_finder.i = i;
		ret = ext4_trans_block_get(fs->bdev, &block, xattr_block);
		if (ret != EOK)
			goto out;

		ret = ext4_xattr_block_find_entry(inode_ref, &block_finder,
						  &block);
		if (ret != EOK) {
			ext4_block_set(fs->bdev, &block);
			goto out;
		}

		/* Return ENODATA if entry is not found */
		if (block_finder.s.not_found) {
			ext4_block_set(fs->bdev, &block);
			ret = ENODATA;
			goto out;
		}

		value_len = to_le32(block_finder.s.here->e_value_size);
		value_offs = to_le32(block_finder.s.here->e_value_offs);
		if (buf_len && buf) {
			void *data_loc =
			    (char *)block_finder.s.base + value_offs;
			memcpy(buf, data_loc,
			       (buf_len < value_len) ? buf_len : value_len);
		}

		/*
		 * Free the xattr block buffer returned by
		 * ext4_xattr_block_find_entry.
		 */
		ext4_block_set(fs->bdev, &block);
	}

out:
	if (ret == EOK && data_len)
		*data_len = value_len;

	return ret;
}

/**
 * @brief Try to copy the content of an xattr block to a newly-allocated
 * 	  block. If the operation fails, the block buffer provided by
 * 	  caller will be freed
 *
 * @param inode_ref Inode reference
 * @param block The block buffer reference
 * @param new_block The newly-allocated block buffer reference
 * @param orig_block The block number of @block
 * @param allocated a new block is allocated
 *
 * @return Error code
 */
static int ext4_xattr_copy_new_block(struct ext4_inode_ref *inode_ref,
				     struct ext4_block *block,
				     struct ext4_block *new_block,
				     ext4_fsblk_t *orig_block, bool *allocated)
{
	int ret = EOK;
	ext4_fsblk_t xattr_block = 0;
	struct ext4_xattr_header *header;
	struct ext4_fs *fs = inode_ref->fs;
	header = EXT4_XATTR_BHDR(block);

	if (orig_block)
		*orig_block = block->lb_id;

	if (allocated)
		*allocated = false;

	/* Only do copy when a block is referenced by more than one inode. */
	if (to_le32(header->h_refcount) > 1) {
		ext4_fsblk_t goal = ext4_fs_inode_to_goal_block(inode_ref);

		/* Allocate a new block to be used by this inode */
		ret = ext4_balloc_alloc_block(inode_ref, goal, &xattr_block);
		if (ret != EOK)
			goto out;

		ret = ext4_trans_block_get(fs->bdev, new_block, xattr_block);
		if (ret != EOK)
			goto out;

		/* Copy the content of the whole block */
		memcpy(new_block->data, block->data,
		       ext4_sb_get_block_size(&inode_ref->fs->sb));

		/*
		 * Decrement the reference count of the original xattr block
		 * by one
		 */
		header->h_refcount = to_le32(to_le32(header->h_refcount) - 1);
		ext4_trans_set_block_dirty(block->buf);
		ext4_trans_set_block_dirty(new_block->buf);

		header = EXT4_XATTR_BHDR(new_block);
		header->h_refcount = to_le32(1);

		if (allocated)
			*allocated = true;
	}
out:
	if (xattr_block) {
		if (ret != EOK)
			ext4_balloc_free_block(inode_ref, xattr_block);
		else {
			/*
			 * Modify the in-inode pointer to point to the new xattr block
			 */
			ext4_inode_set_file_acl(inode_ref->inode, &fs->sb, xattr_block);
			inode_ref->dirty = true;
		}
	}

	return ret;
}

/**
 * @brief Given an EA entry's name, remove the EA entry
 *
 * @param inode_ref Inode reference
 * @param name_index Name-index
 * @param name Name of the EA entry to be removed
 * @param name_len Length of name in bytes
 *
 * @return Error code
 */
int ext4_xattr_remove(struct ext4_inode_ref *inode_ref, uint8_t name_index,
		      const char *name, size_t name_len)
{
	int ret = EOK;
	struct ext4_block block;
	struct ext4_xattr_finder ibody_finder;
	struct ext4_xattr_finder block_finder;
	bool use_block = false;
	bool block_loaded = false;
	struct ext4_xattr_info i;
	struct ext4_fs *fs = inode_ref->fs;
	ext4_fsblk_t xattr_block;

	xattr_block = ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);

	i.name_index = name_index;
	i.name = name;
	i.name_len = name_len;
	i.value = NULL;
	i.value_len = 0;

	ibody_finder.i = i;
	block_finder.i = i;

	ret = ext4_xattr_ibody_find_entry(inode_ref, &ibody_finder);
	if (ret != EOK)
		goto out;

	if (ibody_finder.s.not_found && xattr_block) {
		ret = ext4_trans_block_get(fs->bdev, &block, xattr_block);
		if (ret != EOK)
			goto out;

		block_loaded = true;
		block_finder.i = i;
		ret = ext4_xattr_block_find_entry(inode_ref, &block_finder,
						  &block);
		if (ret != EOK)
			goto out;

		/* Return ENODATA if entry is not found */
		if (block_finder.s.not_found) {
			ret = ENODATA;
			goto out;
		}
		use_block = true;
	}

	if (use_block) {
		bool allocated = false;
		struct ext4_block new_block;

		/*
		 * There will be no effect when the xattr block is only referenced
		 * once.
		 */
		ret = ext4_xattr_copy_new_block(inode_ref, &block, &new_block,
						&xattr_block, &allocated);
		if (ret != EOK)
			goto out;

		if (!allocated) {
			/* Prevent double-freeing */
			block_loaded = false;
			new_block = block;
		}

		ret = ext4_xattr_block_find_entry(inode_ref, &block_finder,
						  &new_block);
		if (ret != EOK)
			goto out;

		/* Now remove the entry */
		ext4_xattr_set_entry(&i, &block_finder.s, false);

		if (ext4_xattr_is_empty(&block_finder.s)) {
			ext4_block_set(fs->bdev, &new_block);
			ext4_xattr_try_free_block(inode_ref);
		} else {
			struct ext4_xattr_header *header =
			    EXT4_XATTR_BHDR(&new_block);
			header = EXT4_XATTR_BHDR(&new_block);
			ext4_assert(block_finder.s.first);
			ext4_xattr_rehash(header, block_finder.s.first);
			ext4_xattr_set_block_checksum(inode_ref,
						      block.lb_id,
						      header);

			ext4_trans_set_block_dirty(new_block.buf);
			ext4_block_set(fs->bdev, &new_block);
		}

	} else {
		/* Now remove the entry */
		ext4_xattr_set_entry(&i, &block_finder.s, false);
		inode_ref->dirty = true;
	}
out:
	if (block_loaded)
		ext4_block_set(fs->bdev, &block);

	return ret;
}

/**
 * @brief Insert/overwrite an EA entry into/in a xattr block
 *
 * @param inode_ref Inode reference
 * @param i The information of the given EA entry
 *
 * @return Error code
 */
static int ext4_xattr_block_set(struct ext4_inode_ref *inode_ref,
				struct ext4_xattr_info *i,
				bool no_insert)
{
	int ret = EOK;
	bool allocated = false;
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_block block, new_block;
	ext4_fsblk_t orig_xattr_block;

	orig_xattr_block = ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);

	ext4_assert(i->value);
	if (!orig_xattr_block) {
		struct ext4_xattr_search s;
		struct ext4_xattr_header *header;

		/* If insertion of new entry is not allowed... */
		if (no_insert) {
			ret = ENODATA;
			goto out;
		}

		ret = ext4_xattr_try_alloc_block(inode_ref);
		if (ret != EOK)
			goto out;

		orig_xattr_block =
		    ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);
		ret = ext4_trans_block_get(fs->bdev, &block, orig_xattr_block);
		if (ret != EOK) {
			ext4_xattr_try_free_block(inode_ref);
			goto out;
		}

		ext4_xattr_block_initialize(inode_ref, &block);
		ext4_xattr_block_init_search(inode_ref, &s, &block);

		ret = ext4_xattr_set_entry(i, &s, false);
		if (ret == EOK) {
			header = EXT4_XATTR_BHDR(&block);

			ext4_assert(s.here);
			ext4_assert(s.first);
			ext4_xattr_compute_hash(header, s.here);
			ext4_xattr_rehash(header, s.first);
			ext4_xattr_set_block_checksum(inode_ref,
						      block.lb_id,
						      header);
			ext4_trans_set_block_dirty(block.buf);
		}
		ext4_block_set(fs->bdev, &block);
		if (ret != EOK)
			ext4_xattr_try_free_block(inode_ref);

	} else {
		struct ext4_xattr_finder finder;
		struct ext4_xattr_header *header;
		finder.i = *i;
		ret = ext4_trans_block_get(fs->bdev, &block, orig_xattr_block);
		if (ret != EOK)
			goto out;

		header = EXT4_XATTR_BHDR(&block);

		/*
		 * Consider the following case when insertion of new
		 * entry is not allowed
		 */
		if (to_le32(header->h_refcount) > 1 && no_insert) {
			/*
			 * There are other people referencing the
			 * same xattr block
			 */
			ret = ext4_xattr_block_find_entry(inode_ref, &finder, &block);
			if (ret != EOK) {
				ext4_block_set(fs->bdev, &block);
				goto out;
			}
			if (finder.s.not_found) {
				ext4_block_set(fs->bdev, &block);
				ret = ENODATA;
				goto out;
			}
		}

		/*
		 * There will be no effect when the xattr block is only referenced
		 * once.
		 */
		ret = ext4_xattr_copy_new_block(inode_ref, &block, &new_block,
						&orig_xattr_block, &allocated);
		if (ret != EOK) {
			ext4_block_set(fs->bdev, &block);
			goto out;
		}

		if (allocated) {
			ext4_block_set(fs->bdev, &block);
			new_block = block;
		}

		ret = ext4_xattr_block_find_entry(inode_ref, &finder, &block);
		if (ret != EOK) {
			ext4_block_set(fs->bdev, &block);
			goto out;
		}

		ret = ext4_xattr_set_entry(i, &finder.s, false);
		if (ret == EOK) {
			header = EXT4_XATTR_BHDR(&block);

			ext4_assert(finder.s.here);
			ext4_assert(finder.s.first);
			ext4_xattr_compute_hash(header, finder.s.here);
			ext4_xattr_rehash(header, finder.s.first);
			ext4_xattr_set_block_checksum(inode_ref,
						      block.lb_id,
						      header);
			ext4_trans_set_block_dirty(block.buf);
		}
		ext4_block_set(fs->bdev, &block);
	}
out:
	return ret;
}

/**
 * @brief Remove an EA entry from a xattr block
 *
 * @param inode_ref Inode reference
 * @param i The information of the given EA entry
 *
 * @return Error code
 */
static int ext4_xattr_block_remove(struct ext4_inode_ref *inode_ref,
				   struct ext4_xattr_info *i)
{
	int ret = EOK;
	bool allocated = false;
	const void *value = i->value;
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_xattr_finder finder;
	struct ext4_block block, new_block;
	struct ext4_xattr_header *header;
	ext4_fsblk_t orig_xattr_block;
	orig_xattr_block = ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);

	ext4_assert(orig_xattr_block);
	ret = ext4_trans_block_get(fs->bdev, &block, orig_xattr_block);
	if (ret != EOK)
		goto out;

	/*
	 * There will be no effect when the xattr block is only referenced
	 * once.
	 */
	ret = ext4_xattr_copy_new_block(inode_ref, &block, &new_block,
					&orig_xattr_block, &allocated);
	if (ret != EOK) {
		ext4_block_set(fs->bdev, &block);
		goto out;
	}

	if (allocated) {
		ext4_block_set(fs->bdev, &block);
		block = new_block;
	}

	ext4_xattr_block_find_entry(inode_ref, &finder, &block);

	if (!finder.s.not_found) {
		i->value = NULL;
		ret = ext4_xattr_set_entry(i, &finder.s, false);
		i->value = value;

		header = EXT4_XATTR_BHDR(&block);
		ext4_assert(finder.s.first);
		ext4_xattr_rehash(header, finder.s.first);
		ext4_xattr_set_block_checksum(inode_ref,
					      block.lb_id,
					      header);
		ext4_trans_set_block_dirty(block.buf);
	}

	ext4_block_set(fs->bdev, &block);
out:
	return ret;
}

/**
 * @brief Insert an EA entry into a given inode reference
 *
 * @param inode_ref Inode reference
 * @param name_index Name-index
 * @param name Name of the EA entry to be inserted
 * @param name_len Length of name in bytes
 * @param value Input buffer to hold content
 * @param value_len Length of input content
 *
 * @return Error code
 */
int ext4_xattr_set(struct ext4_inode_ref *inode_ref, uint8_t name_index,
		   const char *name, size_t name_len, const void *value,
		   size_t value_len)
{
	int ret = EOK;
	struct ext4_fs *fs = inode_ref->fs;
	struct ext4_xattr_finder ibody_finder;
	struct ext4_xattr_info i;
	bool block_found = false;
	ext4_fsblk_t orig_xattr_block;
	size_t extra_isize =
	    ext4_inode_get_extra_isize(&fs->sb, inode_ref->inode);

	i.name_index = name_index;
	i.name = name;
	i.name_len = name_len;
	i.value = (value_len) ? value : &ext4_xattr_empty_value;
	i.value_len = value_len;

	ibody_finder.i = i;

	orig_xattr_block = ext4_inode_get_file_acl(inode_ref->inode, &fs->sb);

	/*
	 * Even if entry is not found, search context block inside the
	 * finder is still valid and can be used to insert entry.
	 */
	ret = ext4_xattr_ibody_find_entry(inode_ref, &ibody_finder);
	if (ret != EOK) {
		ext4_xattr_ibody_initialize(inode_ref);
		ext4_xattr_ibody_find_entry(inode_ref, &ibody_finder);
	}

	if (ibody_finder.s.not_found) {
		if (orig_xattr_block) {
			block_found = true;
			ret = ext4_xattr_block_set(inode_ref, &i, true);
			if (ret == ENOSPC)
				goto try_insert;
			else if (ret == ENODATA)
				goto try_insert;
			else if (ret != EOK)
				goto out;

		} else
			goto try_insert;

	} else {
	try_insert:
		/* Only try to set entry in ibody if inode is sufficiently large */
		if (extra_isize)
			ret = ext4_xattr_set_entry(&i, &ibody_finder.s, false);
		else
			ret = ENOSPC;

		if (ret == ENOSPC) {
			if (!block_found) {
				ret = ext4_xattr_block_set(inode_ref, &i, false);
				ibody_finder.i.value = NULL;
				ext4_xattr_set_entry(&ibody_finder.i,
						     &ibody_finder.s, false);
				inode_ref->dirty = true;
			}

		} else if (ret == EOK) {
			if (block_found)
				ret = ext4_xattr_block_remove(inode_ref, &i);

			inode_ref->dirty = true;
		}
	}

out:
	return ret;
}

#endif

/**
 * @}
 */
