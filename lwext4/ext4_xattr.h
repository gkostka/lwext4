#ifndef EXT4_XATTR_H_
#define EXT4_XATTR_H_

#include "ext4_config.h"
#include "ext4_types.h"

int ext4_fs_get_xattr_ref(struct ext4_fs *fs,
			  struct ext4_inode_ref *inode_ref,
			  struct ext4_xattr_ref *ref);

void ext4_fs_put_xattr_ref(struct ext4_xattr_ref *ref);

int ext4_xattr_lookup(struct ext4_xattr_ref *xattr_ref,
		      uint8_t  name_index,
		      char    *name,
		      size_t   name_len,
		      struct ext4_xattr_entry **found_entry,
		      void   **out_data,
		      size_t  *out_len);

#endif
