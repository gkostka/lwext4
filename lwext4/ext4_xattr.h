#ifndef EXT4_XATTR_H_
#define EXT4_XATTR_H_

#include "ext4_config.h"
#include "ext4_types.h"

int ext4_fs_get_xattr_ref(struct ext4_fs *fs, struct ext4_inode_ref *inode_ref,
			  struct ext4_xattr_ref *ref);

void ext4_fs_put_xattr_ref(struct ext4_xattr_ref *ref);

int ext4_fs_set_xattr(struct ext4_xattr_ref *ref, uint8_t name_index,
		      char *name, size_t name_len, void *data, size_t data_size,
		      bool replace);

int ext4_fs_remove_xattr(struct ext4_xattr_ref *ref, uint8_t name_index,
			 char *name, size_t name_len);

int ext4_fs_get_xattr(struct ext4_xattr_ref *ref, uint8_t name_index,
		      char *name, size_t name_len, void *buf, size_t buf_size,
		      size_t *size_got);

void ext4_fs_xattr_iterate(struct ext4_xattr_ref *ref,
			   int(iter)(struct ext4_xattr_ref *ref,
				     struct ext4_xattr_item *item));

void ext4_fs_xattr_iterate_reset(struct ext4_xattr_ref *ref);

char *ext4_extract_xattr_name(char *full_name, size_t full_name_len,
			      uint8_t *name_index, size_t *name_len);

#endif
