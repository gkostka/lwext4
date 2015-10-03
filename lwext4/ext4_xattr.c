#include "ext4_config.h"
#include "ext4_types.h"
#include "ext4_fs.h"
#include "ext4_errno.h"
#include "ext4_blockdev.h"
#include "ext4_super.h"
#include "ext4_debug.h"
#include "ext4_block_group.h"
#include "ext4_balloc.h"
#include "ext4_bitmap.h"
#include "ext4_inode.h"
#include "ext4_ialloc.h"
#include "ext4_extent.h"

#include <string.h>

static void *ext4_xattr_entry_data(struct ext4_xattr_ref *xattr_ref,
				   struct ext4_xattr_entry *entry,
				   bool in_inode)
{
	void *ret;
	if (in_inode) {
		struct ext4_xattr_ibody_header *header;
		struct ext4_xattr_entry *first_entry;
		uint16_t inode_size = ext4_get16(&xattr_ref->fs->sb,
						 inode_size);
		header = EXT4_XATTR_IHDR(xattr_ref->inode_ref->inode);
		first_entry = EXT4_XATTR_IFIRST(header);
 
		ret = (void *)((char *)first_entry + to_le16(entry->e_value_offs));
		if ((char *)ret + EXT4_XATTR_SIZE(to_le32(entry->e_value_size))
			- (char *)xattr_ref->inode_ref->inode >
		    inode_size)
			ret = NULL;

	} else {
		uint32_t block_size =
				ext4_sb_get_block_size(&xattr_ref->fs->sb);
		ret = (void *)((char *)xattr_ref->block.data + 
				to_le16(entry->e_value_offs));
		if ((char *)ret + EXT4_XATTR_SIZE(to_le32(entry->e_value_size))
			- (char *)xattr_ref->block.data >
		    block_size)
			ret = NULL;
	}
	return ret;
}

static int ext4_xattr_block_lookup(struct ext4_xattr_ref *xattr_ref,
				   uint8_t  name_index,
				   char    *name,
				   size_t   name_len,
				   struct ext4_xattr_entry **found_entry,
				   void   **out_data,
				   size_t  *out_len)
{
	size_t size_rem;
	bool entry_found = false;
	void *data;
	struct ext4_xattr_entry *entry = NULL;

	ext4_assert(xattr_ref->block.data);
	entry = EXT4_XATTR_BFIRST(&xattr_ref->block);

	size_rem = ext4_sb_get_block_size(&xattr_ref->fs->sb);
	for(;size_rem > 0 && !EXT4_XATTR_IS_LAST_ENTRY(entry);
	    entry = EXT4_XATTR_NEXT(entry),
	    size_rem -= EXT4_XATTR_LEN(entry->e_name_len)) {
		int diff = -1;
		if (name_index == entry->e_name_index &&
		    name_len == entry->e_name_len) {
			char *e_name = (char *)(entry + 1);
			diff = memcmp(e_name,
				      name,
				      name_len);
		}
		if (!diff) {
			entry_found = true;
			break;
		}
	}
	if (!entry_found)
		return ENOENT;

	data = ext4_xattr_entry_data(xattr_ref, entry,
				     false);
	if (!data)
		return EIO;

	if (found_entry)
		*found_entry = entry;

	if (out_data && out_len) {
		*out_data = data;
		*out_len = to_le32(entry->e_value_size);
	}

	return EOK;
}

static int ext4_xattr_inode_lookup(struct ext4_xattr_ref *xattr_ref,
				   uint8_t  name_index,
				   char    *name,
				   size_t   name_len,
				   struct ext4_xattr_entry **found_entry,
				   void   **out_data,
				   size_t  *out_len)
{
	void *data;
	size_t size_rem;
	bool entry_found = false;
	struct ext4_xattr_ibody_header *header = NULL;
	struct ext4_xattr_entry *entry = NULL;
	uint16_t inode_size = ext4_get16(&xattr_ref->fs->sb,
					 inode_size);

	header = EXT4_XATTR_IHDR(xattr_ref->inode_ref->inode);
	entry = EXT4_XATTR_IFIRST(header);

	size_rem = inode_size -
		EXT4_GOOD_OLD_INODE_SIZE -
		xattr_ref->inode_ref->inode->extra_isize;
	for(;size_rem > 0 && !EXT4_XATTR_IS_LAST_ENTRY(entry);
	    entry = EXT4_XATTR_NEXT(entry),
	    size_rem -= EXT4_XATTR_LEN(entry->e_name_len)) {
		int diff = -1;
		if (name_index == entry->e_name_index &&
		    name_len == entry->e_name_len) {
			char *e_name = (char *)(entry + 1);
			diff = memcmp(e_name,
				      name,
				      name_len);
		}
		if (!diff) {
			entry_found = true;
			break;
		}
	}
	if (!entry_found)
		return ENOENT;

	data = ext4_xattr_entry_data(xattr_ref, entry,
				     true);
	if (!data)
		return EIO;

	if (found_entry)
		*found_entry = entry;

	if (out_data && out_len) {
		*out_data = data;
		*out_len = to_le32(entry->e_value_size);
	}

	return EOK;
}

int ext4_xattr_lookup(struct ext4_xattr_ref *xattr_ref,
		      uint8_t  name_index,
		      char    *name,
		      size_t   name_len,
		      struct ext4_xattr_entry **found_entry,
		      void   **out_data,
		      size_t  *out_len)
{
	int ret = ENOENT;
	uint16_t inode_size = ext4_get16(&xattr_ref->fs->sb,
					 inode_size);
	if (inode_size > EXT4_GOOD_OLD_INODE_SIZE) {
		ret = ext4_xattr_inode_lookup(xattr_ref,
					      name_index,
					      name,
					      name_len,
					      found_entry,
					      out_data,
					      out_len);
		if (ret != ENOENT)
			return ret;

	}

	if (xattr_ref->block_loaded)
		ret = ext4_xattr_block_lookup(xattr_ref,
					      name_index,
					      name,
					      name_len,
					      found_entry,
					      out_data,
					      out_len);
	return ret;
}

int ext4_fs_get_xattr_ref(struct ext4_fs *fs,
			  struct ext4_inode_ref *inode_ref,
			  struct ext4_xattr_ref *ref)
{
	int rc;
	uint64_t xattr_block;
	xattr_block = ext4_inode_get_file_acl(inode_ref->inode,
					      &fs->sb);
	if (xattr_block) {
		rc = ext4_block_get(fs->bdev,
				    &inode_ref->block, xattr_block);
		if (rc != EOK)
			return EIO;
	
		ref->block_loaded = true;
	} else
		ref->block_loaded = false;

	ref->inode_ref = inode_ref;
	ref->fs = fs;
	return EOK;
}

void ext4_fs_put_xattr_ref(struct ext4_xattr_ref *ref)
{
	if (ref->block_loaded) {
		ext4_block_set(ref->fs->bdev, &ref->block);
		ref->block_loaded = false;
	}
	ref->inode_ref = NULL;
	ref->fs = NULL;
}

