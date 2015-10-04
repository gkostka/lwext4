#ifndef EXT4_XATTR_H_
#define EXT4_XATTR_H_

#include "ext4_config.h"
#include "ext4_types.h"

int ext4_fs_get_xattr_ref(struct ext4_fs *fs,
			  struct ext4_inode_ref *inode_ref,
			  struct ext4_xattr_ref *ref);

void ext4_fs_put_xattr_ref(struct ext4_xattr_ref *ref);

#endif
