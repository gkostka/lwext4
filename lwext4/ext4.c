/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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
 * @file  ext4.h
 * @brief Ext4 high level operations (file, directory, mountpoints...)
 */

#include "ext4_config.h"
#include "ext4.h"
#include "ext4_blockdev.h"
#include "ext4_types.h"
#include "ext4_debug.h"
#include "ext4_errno.h"
#include "ext4_fs.h"
#include "ext4_dir.h"
#include "ext4_inode.h"
#include "ext4_super.h"
#include "ext4_dir_idx.h"
#include "ext4_xattr.h"
#include "ext4_journal.h"


#include <stdlib.h>
#include <string.h>

/**@brief   Mount point OS dependent lock*/
#define EXT4_MP_LOCK(_m)                                                       \
	do {                                                                   \
		if ((_m)->os_locks)                                            \
			(_m)->os_locks->lock();                                \
	} while (0)

/**@brief   Mount point OS dependent unlock*/
#define EXT4_MP_UNLOCK(_m)                                                     \
	do {                                                                   \
		if ((_m)->os_locks)                                            \
			(_m)->os_locks->unlock();                              \
	} while (0)

/**@brief   Mount point descriptor.*/
struct ext4_mountpoint {

	/**@brief   Mount done flag.*/
	bool mounted;

	/**@brief   Mount point name (@ref ext4_mount)*/
	char name[32];

	/**@brief   OS dependent lock/unlock functions.*/
	const struct ext4_lock *os_locks;

	/**@brief   Ext4 filesystem internals.*/
	struct ext4_fs fs;

	/**@brief   Dynamic allocation cache flag.*/
	bool cache_dynamic;

	struct jbd_fs jbd_fs;
	struct jbd_journal jbd_journal;
};

/**@brief   Block devices descriptor.*/
struct _ext4_devices {

	/**@brief   Block device name (@ref ext4_device_register)*/
	char name[32];

	/**@brief   Block device handle.*/
	struct ext4_blockdev *bd;

	/**@brief   Block cache handle.*/
	struct ext4_bcache *bc;
};

/**@brief   Block devices.*/
struct _ext4_devices _bdevices[CONFIG_EXT4_BLOCKDEVS_COUNT];

/**@brief   Mountpoints.*/
struct ext4_mountpoint _mp[CONFIG_EXT4_MOUNTPOINTS_COUNT];

int ext4_device_register(struct ext4_blockdev *bd, struct ext4_bcache *bc,
			 const char *dev_name)
{
	uint32_t i;
	ext4_assert(bd && dev_name);

	for (i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
		if (!_bdevices[i].bd) {
			strcpy(_bdevices[i].name, dev_name);
			_bdevices[i].bd = bd;
			_bdevices[i].bc = bc;
			return EOK;
		}

		if (!strcmp(_bdevices[i].name, dev_name))
			return EOK;
	}
	return ENOSPC;
}

/****************************************************************************/

static bool ext4_is_dots(const uint8_t *name, size_t name_size)
{
	if ((name_size == 1) && (name[0] == '.'))
		return true;

	if ((name_size == 2) && (name[0] == '.') && (name[1] == '.'))
		return true;

	return false;
}

static int ext4_has_children(bool *has_children, struct ext4_inode_ref *enode)
{
	struct ext4_sblock *sb = &enode->fs->sb;

	/* Check if node is directory */
	if (!ext4_inode_is_type(sb, enode->inode, EXT4_INODE_MODE_DIRECTORY)) {
		*has_children = false;
		return EOK;
	}

	struct ext4_dir_iter it;
	int rc = ext4_dir_iterator_init(&it, enode, 0);
	if (rc != EOK)
		return rc;

	/* Find a non-empty directory entry */
	bool found = false;
	while (it.curr != NULL) {
		if (ext4_dir_en_get_inode(it.curr) != 0) {
			uint16_t nsize;
			nsize = ext4_dir_en_get_name_len(sb, it.curr);
			if (!ext4_is_dots(it.curr->name, nsize)) {
				found = true;
				break;
			}
		}

		rc = ext4_dir_iterator_next(&it);
		if (rc != EOK) {
			ext4_dir_iterator_fini(&it);
			return rc;
		}
	}

	rc = ext4_dir_iterator_fini(&it);
	if (rc != EOK)
		return rc;

	*has_children = found;

	return EOK;
}

static int ext4_link(struct ext4_mountpoint *mp, struct ext4_inode_ref *parent,
		     struct ext4_inode_ref *ch, const char *n,
		     uint32_t len, bool rename)
{
	/* Check maximum name length */
	if (len > EXT4_DIRECTORY_FILENAME_LEN)
		return EINVAL;

	/* Add entry to parent directory */
	int r = ext4_dir_add_entry(parent, n, len, ch);
	if (r != EOK)
		return r;

	/* Fill new dir -> add '.' and '..' entries.
	 * Also newly allocated inode should have 0 link count.
	 */

	bool is_dir = ext4_inode_is_type(&mp->fs.sb, ch->inode,
			       EXT4_INODE_MODE_DIRECTORY);
	if (is_dir && !rename) {

#if CONFIG_DIR_INDEX_ENABLE
		/* Initialize directory index if supported */
		if (ext4_sb_feature_com(&mp->fs.sb, EXT4_FCOM_DIR_INDEX)) {
			r = ext4_dir_dx_init(ch, parent);
			if (r != EOK)
				return r;

			ext4_inode_set_flag(ch->inode, EXT4_INODE_FLAG_INDEX);
			ch->dirty = true;
		} else
#endif
		{
			r = ext4_dir_add_entry(ch, ".", strlen("."), ch);
			if (r != EOK) {
				ext4_dir_remove_entry(parent, n, strlen(n));
				return r;
			}

			r = ext4_dir_add_entry(ch, "..", strlen(".."), parent);
			if (r != EOK) {
				ext4_dir_remove_entry(parent, n, strlen(n));
				ext4_dir_remove_entry(ch, ".", strlen("."));
				return r;
			}
		}

		/*New empty directory. Two links (. and ..) */
		ext4_inode_set_links_cnt(ch->inode, 2);
		ext4_fs_inode_links_count_inc(parent);
		ch->dirty = true;
		parent->dirty = true;
		return r;
	}
	/*
	 * In case we want to rename a directory,
	 * we reset the original '..' pointer.
	 */
	if (is_dir) {
		bool idx;
		idx = ext4_inode_has_flag(ch->inode, EXT4_INODE_FLAG_INDEX);
		struct ext4_dir_search_result res;
		if (!idx) {
			r = ext4_dir_find_entry(&res, ch, "..", strlen(".."));
			if (r != EOK)
				return EIO;

			ext4_dir_en_set_inode(res.dentry, parent->index);
			ext4_trans_set_block_dirty(res.block.buf);
			r = ext4_dir_destroy_result(ch, &res);
			if (r != EOK)
				return r;

		} else {
#if CONFIG_DIR_INDEX_ENABLE
			r = ext4_dir_dx_reset_parent_inode(ch, parent->index);
			if (r != EOK)
				return r;

#endif
		}

		ext4_fs_inode_links_count_inc(parent);
		parent->dirty = true;
	}
	if (!rename) {
		ext4_fs_inode_links_count_inc(ch);
		ch->dirty = true;
	}

	return r;
}

static int ext4_unlink(struct ext4_mountpoint *mp,
		       struct ext4_inode_ref *parent,
		       struct ext4_inode_ref *child, const char *name,
		       uint32_t name_len)
{
	bool has_children;
	int rc = ext4_has_children(&has_children, child);
	if (rc != EOK)
		return rc;

	/* Cannot unlink non-empty node */
	if (has_children)
		return ENOTEMPTY;

	/* Remove entry from parent directory */
	rc = ext4_dir_remove_entry(parent, name, name_len);
	if (rc != EOK)
		return rc;

	bool is_dir = ext4_inode_is_type(&mp->fs.sb, child->inode,
					 EXT4_INODE_MODE_DIRECTORY);

	/* If directory - handle links from parent */
	if (is_dir) {
		ext4_fs_inode_links_count_dec(parent);
		parent->dirty = true;
	}

	/*
	 * TODO: Update timestamps of the parent
	 * (when we have wall-clock time).
	 *
	 * ext4_inode_set_change_inode_time(parent->inode, (uint32_t) now);
	 * ext4_inode_set_modification_time(parent->inode, (uint32_t) now);
	 * parent->dirty = true;
	 */

	/*
	 * TODO: Update timestamp for inode.
	 *
	 * ext4_inode_set_change_inode_time(child->inode,
	 *     (uint32_t) now);
	 */
	if (ext4_inode_get_links_cnt(child->inode)) {
		ext4_fs_inode_links_count_dec(child);
		child->dirty = true;
	}

	return EOK;
}

/****************************************************************************/

int ext4_mount(const char *dev_name, const char *mount_point)
{
	ext4_assert(mount_point && dev_name);
	int r;
	int i;

	uint32_t bsize;
	struct ext4_blockdev *bd = 0;
	struct ext4_bcache *bc = 0;
	struct ext4_mountpoint *mp = 0;

	if (mount_point[strlen(mount_point) - 1] != '/')
		return ENOTSUP;

	for (i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
		if (_bdevices[i].name) {
			if (!strcmp(dev_name, _bdevices[i].name)) {
				bd = _bdevices[i].bd;
				bc = _bdevices[i].bc;
				break;
			}
		}
	}

	if (!bd)
		return ENODEV;

	for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
		if (!_mp[i].mounted) {
			strcpy(_mp[i].name, mount_point);
			_mp[i].mounted = 1;
			mp = &_mp[i];
			break;
		}

		if (!strcmp(_mp[i].name, mount_point))
			return EOK;
	}

	if (!mp)
		return ENOMEM;

	r = ext4_block_init(bd);
	if (r != EOK)
		return r;

	r = ext4_fs_init(&mp->fs, bd);
	if (r != EOK) {
		ext4_block_fini(bd);
		return r;
	}

	bsize = ext4_sb_get_block_size(&mp->fs.sb);
	ext4_block_set_lb_size(bd, bsize);

	mp->cache_dynamic = 0;

	if (!bc) {
		/*Automatic block cache alloc.*/
		mp->cache_dynamic = 1;
		bc = malloc(sizeof(struct ext4_bcache));

		r = ext4_bcache_init_dynamic(bc, CONFIG_BLOCK_DEV_CACHE_SIZE,
					     bsize);
		if (r != EOK) {
			free(bc);
			ext4_block_fini(bd);
			return r;
		}
	}

	if (bsize != bc->itemsize)
		return ENOTSUP;

	/*Bind block cache to block device*/
	r = ext4_block_bind_bcache(bd, bc);
	if (r != EOK) {
		ext4_bcache_cleanup(bc);
		ext4_block_fini(bd);
		if (mp->cache_dynamic) {
			ext4_bcache_fini_dynamic(bc);
			free(bc);
		}
		return r;
	}
	bd->fs = &mp->fs;

	return r;
}


int ext4_umount(const char *mount_point)
{
	int i;
	int r;
	struct ext4_mountpoint *mp = 0;

	for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
		if (!strcmp(_mp[i].name, mount_point)) {
			mp = &_mp[i];
			break;
		}
	}

	if (!mp)
		return ENODEV;

	r = ext4_fs_fini(&mp->fs);
	if (r != EOK)
		goto Finish;

	mp->mounted = 0;

	ext4_bcache_cleanup(mp->fs.bdev->bc);
	if (mp->cache_dynamic) {
		ext4_bcache_fini_dynamic(mp->fs.bdev->bc);
		free(mp->fs.bdev->bc);
	}
	r = ext4_block_fini(mp->fs.bdev);
Finish:
	mp->fs.bdev->fs = NULL;
	return r;
}

static struct ext4_mountpoint *ext4_get_mount(const char *path)
{
	int i;
	for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {

		if (!_mp[i].mounted)
			continue;

		if (!strncmp(_mp[i].name, path, strlen(_mp[i].name)))
			return &_mp[i];
	}
	return 0;
}

int ext4_journal_start(const char *mount_point)
{
	int r = EOK;
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);
	if (!mp)
		return ENOENT;

	if (ext4_sb_feature_com(&mp->fs.sb,
				EXT4_FCOM_HAS_JOURNAL)) {
		r = jbd_get_fs(&mp->fs, &mp->jbd_fs);
		if (r != EOK)
			goto Finish;

		r = jbd_journal_start(&mp->jbd_fs, &mp->jbd_journal);
		if (r != EOK) {
			mp->jbd_fs.dirty = false;
			jbd_put_fs(&mp->jbd_fs);
			goto Finish;
		}
		mp->fs.jbd_fs = &mp->jbd_fs;
		mp->fs.jbd_journal = &mp->jbd_journal;
	}
Finish:
	return r;
}

int ext4_journal_stop(const char *mount_point)
{
	int r = EOK;
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);
	if (!mp)
		return ENOENT;

	if (ext4_sb_feature_com(&mp->fs.sb,
				EXT4_FCOM_HAS_JOURNAL)) {
		r = jbd_journal_stop(&mp->jbd_journal);
		if (r != EOK) {
			mp->jbd_fs.dirty = false;
			jbd_put_fs(&mp->jbd_fs);
			mp->fs.jbd_journal = NULL;
			mp->fs.jbd_fs = NULL;
			goto Finish;
		}

		r = jbd_put_fs(&mp->jbd_fs);
		if (r != EOK) {
			mp->fs.jbd_journal = NULL;
			mp->fs.jbd_fs = NULL;
			goto Finish;
		}

		mp->fs.jbd_journal = NULL;
		mp->fs.jbd_fs = NULL;
	}
Finish:
	return r;
}

int ext4_recover(const char *mount_point)
{
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);
	if (!mp)
		return ENOENT;

	int r = ENOTSUP;
	EXT4_MP_LOCK(mp);
	if (ext4_sb_feature_com(&mp->fs.sb, EXT4_FCOM_HAS_JOURNAL)) {
		struct jbd_fs *jbd_fs = calloc(1, sizeof(struct jbd_fs));
		if (!jbd_fs) {
			 r = ENOMEM;
			 goto Finish;
		}


		r = jbd_get_fs(&mp->fs, jbd_fs);
		if (r != EOK) {
			free(jbd_fs);
			goto Finish;
		}

		r = jbd_recover(jbd_fs);
		jbd_put_fs(jbd_fs);
		free(jbd_fs);
	}


Finish:
	EXT4_MP_UNLOCK(mp);
	return r;
}

static int ext4_trans_start(struct ext4_mountpoint *mp)
{
	int r = EOK;
	if (mp->fs.jbd_journal && !mp->fs.curr_trans) {
		struct jbd_journal *journal = mp->fs.jbd_journal;
		struct jbd_trans *trans;
		trans = jbd_journal_new_trans(journal);
		if (!trans) {
			r = ENOMEM;
			goto Finish;
		}
		mp->fs.curr_trans = trans;
	}
Finish:
	return r;
}

static int ext4_trans_stop(struct ext4_mountpoint *mp)
{
	int r = EOK;
	if (mp->fs.jbd_journal && mp->fs.curr_trans) {
		struct jbd_journal *journal = mp->fs.jbd_journal;
		struct jbd_trans *trans = mp->fs.curr_trans;
		r = jbd_journal_commit_trans(journal, trans);
		mp->fs.curr_trans = NULL;
	}
	return r;
}

static void ext4_trans_abort(struct ext4_mountpoint *mp)
{
	if (mp->fs.jbd_journal && mp->fs.curr_trans) {
		struct jbd_journal *journal = mp->fs.jbd_journal;
		struct jbd_trans *trans = mp->fs.curr_trans;
		jbd_journal_free_trans(journal, trans, true);
		mp->fs.curr_trans = NULL;
	}
}

int ext4_mount_point_stats(const char *mount_point,
			   struct ext4_mount_stats *stats)
{
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	stats->inodes_count = ext4_get32(&mp->fs.sb, inodes_count);
	stats->free_inodes_count = ext4_get32(&mp->fs.sb, free_inodes_count);
	stats->blocks_count = ext4_sb_get_blocks_cnt(&mp->fs.sb);
	stats->free_blocks_count = ext4_sb_get_free_blocks_cnt(&mp->fs.sb);
	stats->block_size = ext4_sb_get_block_size(&mp->fs.sb);

	stats->block_group_count = ext4_block_group_cnt(&mp->fs.sb);
	stats->blocks_per_group = ext4_get32(&mp->fs.sb, blocks_per_group);
	stats->inodes_per_group = ext4_get32(&mp->fs.sb, inodes_per_group);

	memcpy(stats->volume_name, mp->fs.sb.volume_name, 16);
	EXT4_MP_UNLOCK(mp);

	return EOK;
}

int ext4_mount_setup_locks(const char *mount_point,
			   const struct ext4_lock *locks)
{
	uint32_t i;
	struct ext4_mountpoint *mp = 0;

	for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
		if (!strcmp(_mp[i].name, mount_point)) {
			mp = &_mp[i];
			break;
		}
	}
	if (!mp)
		return ENOENT;

	mp->os_locks = locks;
	return EOK;
}

/********************************FILE OPERATIONS*****************************/

static int ext4_path_check(const char *path, bool *is_goal)
{
	int i;

	for (i = 0; i < EXT4_DIRECTORY_FILENAME_LEN; ++i) {

		if (path[i] == '/') {
			*is_goal = false;
			return i;
		}

		if (path[i] == 0) {
			*is_goal = true;
			return i;
		}
	}

	return 0;
}

static bool ext4_parse_flags(const char *flags, uint32_t *file_flags)
{
	if (!flags)
		return false;

	if (!strcmp(flags, "r") || !strcmp(flags, "rb")) {
		*file_flags = O_RDONLY;
		return true;
	}

	if (!strcmp(flags, "w") || !strcmp(flags, "wb")) {
		*file_flags = O_WRONLY | O_CREAT | O_TRUNC;
		return true;
	}

	if (!strcmp(flags, "a") || !strcmp(flags, "ab")) {
		*file_flags = O_WRONLY | O_CREAT | O_APPEND;
		return true;
	}

	if (!strcmp(flags, "r+") || !strcmp(flags, "rb+") ||
	    !strcmp(flags, "r+b")) {
		*file_flags = O_RDWR;
		return true;
	}

	if (!strcmp(flags, "w+") || !strcmp(flags, "wb+") ||
	    !strcmp(flags, "w+b")) {
		*file_flags = O_RDWR | O_CREAT | O_TRUNC;
		return true;
	}

	if (!strcmp(flags, "a+") || !strcmp(flags, "ab+") ||
	    !strcmp(flags, "a+b")) {
		*file_flags = O_RDWR | O_CREAT | O_APPEND;
		return true;
	}

	return false;
}

static int ext4_trunc_inode(struct ext4_mountpoint *mp,
			    uint32_t index, uint64_t new_size)
{
	int r = EOK;
	struct ext4_fs *const fs = &mp->fs;
	struct ext4_inode_ref inode_ref;
	uint64_t inode_size;
	bool has_trans = mp->fs.jbd_journal && mp->fs.curr_trans;
	r = ext4_fs_get_inode_ref(fs, index, &inode_ref);
	if (r != EOK)
		return r;

	inode_size = ext4_inode_get_size(&fs->sb, inode_ref.inode);
	ext4_fs_put_inode_ref(&inode_ref);
	if (has_trans)
		ext4_trans_stop(mp);

	while (inode_size > new_size + CONFIG_MAX_TRUNCATE_SIZE) {

		inode_size -= CONFIG_MAX_TRUNCATE_SIZE;

		ext4_trans_start(mp);
		r = ext4_fs_get_inode_ref(fs, index, &inode_ref);
		if (r != EOK) {
			ext4_trans_abort(mp);
			break;
		}
		r = ext4_fs_truncate_inode(&inode_ref, inode_size);
		if (r != EOK)
			ext4_fs_put_inode_ref(&inode_ref);
		else
			r = ext4_fs_put_inode_ref(&inode_ref);

		if (r != EOK) {
			ext4_trans_abort(mp);
			goto Finish;
		} else
			ext4_trans_stop(mp);
	}

	if (inode_size > new_size) {

		inode_size = new_size;

		ext4_trans_start(mp);
		r = ext4_fs_get_inode_ref(fs, index, &inode_ref);
		if (r != EOK) {
			ext4_trans_abort(mp);
			goto Finish;
		}
		r = ext4_fs_truncate_inode(&inode_ref, inode_size);
		if (r != EOK)
			ext4_fs_put_inode_ref(&inode_ref);
		else
			r = ext4_fs_put_inode_ref(&inode_ref);

		if (r != EOK)
			ext4_trans_abort(mp);
		else
			ext4_trans_stop(mp);

	}

Finish:

	if (has_trans)
		ext4_trans_start(mp);

	return r;
}

/*
 * NOTICE: if filetype is equal to EXT4_DIRENTRY_UNKNOWN,
 * any filetype of the target dir entry will be accepted.
 */
static int ext4_generic_open2(ext4_file *f, const char *path, int flags,
			      int ftype, uint32_t *parent_inode,
			      uint32_t *name_off)
{
	bool is_goal = false;
	uint32_t imode = EXT4_INODE_MODE_DIRECTORY;
	uint32_t next_inode;

	int r;
	int len;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	struct ext4_dir_search_result result;
	struct ext4_inode_ref ref;

	f->mp = 0;

	if (!mp)
		return ENOENT;

	struct ext4_fs *const fs = &mp->fs;
	struct ext4_sblock *const sb = &mp->fs.sb;

	f->flags = flags;

	/*Skip mount point*/
	path += strlen(mp->name);

	if (name_off)
		*name_off = strlen(mp->name);

	/*Load root*/
	r = ext4_fs_get_inode_ref(fs, EXT4_INODE_ROOT_INDEX, &ref);
	if (r != EOK)
		return r;

	if (parent_inode)
		*parent_inode = ref.index;

	if (flags & O_CREAT)
		ext4_trans_start(mp);

	len = ext4_path_check(path, &is_goal);
	while (1) {

		len = ext4_path_check(path, &is_goal);
		if (!len) {
			/*If root open was request.*/
			if (ftype == EXT4_DE_DIR || ftype == EXT4_DE_UNKNOWN)
				if (is_goal)
					break;

			r = ENOENT;
			break;
		}

		r = ext4_dir_find_entry(&result, &ref, path, len);
		if (r != EOK) {

			/*Destroy last result*/
			ext4_dir_destroy_result(&ref, &result);
			if (r != ENOENT)
				break;

			if (!(f->flags & O_CREAT))
				break;

			/*O_CREAT allows create new entry*/
			struct ext4_inode_ref child_ref;
			r = ext4_fs_alloc_inode(fs, &child_ref,
					is_goal ? ftype : EXT4_DE_DIR);
			if (r != EOK)
				break;


			/*Link with root dir.*/
			r = ext4_link(mp, &ref, &child_ref, path, len, false);
			if (r != EOK) {
				/*Fail. Free new inode.*/
				ext4_fs_free_inode(&child_ref);
				/*We do not want to write new inode.
				  But block has to be released.*/
				child_ref.dirty = false;
				ext4_fs_put_inode_ref(&child_ref);
				break;
			}

			ext4_fs_put_inode_ref(&child_ref);
			continue;
		}

		if (parent_inode)
			*parent_inode = ref.index;

		next_inode = ext4_dir_en_get_inode(result.dentry);
		if (ext4_sb_feature_incom(sb, EXT4_FINCOM_FILETYPE)) {
			uint8_t t;
			t = ext4_dir_en_get_inode_type(sb, result.dentry);
			imode = ext4_fs_correspond_inode_mode(t);
		} else {
			struct ext4_inode_ref child_ref;
			r = ext4_fs_get_inode_ref(fs, next_inode, &child_ref);
			if (r != EOK)
				break;

			imode = ext4_inode_type(sb, child_ref.inode);
			ext4_fs_put_inode_ref(&child_ref);
		}

		r = ext4_dir_destroy_result(&ref, &result);
		if (r != EOK)
			break;

		/*If expected file error*/
		if (imode != EXT4_INODE_MODE_DIRECTORY && !is_goal) {
			r = ENOENT;
			break;
		}
		if (ftype != EXT4_DE_UNKNOWN) {
			bool df = imode != ext4_fs_correspond_inode_mode(ftype);
			if (df && is_goal) {
				r = ENOENT;
				break;
			}
		}

		r = ext4_fs_put_inode_ref(&ref);
		if (r != EOK)
			break;

		r = ext4_fs_get_inode_ref(fs, next_inode, &ref);
		if (r != EOK)
			break;

		if (is_goal)
			break;

		path += len + 1;

		if (name_off)
			*name_off += len + 1;
	};

	if (r != EOK) {
		ext4_fs_put_inode_ref(&ref);
		return r;
	}

	if (is_goal) {

		if ((f->flags & O_TRUNC) && (imode == EXT4_INODE_MODE_FILE)) {
			r = ext4_trunc_inode(mp, ref.index, 0);
			if (r != EOK) {
				ext4_fs_put_inode_ref(&ref);
				return r;
			}
		}

		f->mp = mp;
		f->fsize = ext4_inode_get_size(sb, ref.inode);
		f->inode = ref.index;
		f->fpos = 0;

		if (f->flags & O_APPEND)
			f->fpos = f->fsize;

	}

	r = ext4_fs_put_inode_ref(&ref);
	if (flags & O_CREAT) {
		if (r == EOK)
			ext4_trans_stop(mp);
		else
			ext4_trans_abort(mp);

	}

	return r;
}

/****************************************************************************/

static int ext4_generic_open(ext4_file *f, const char *path, const char *flags,
			     bool file_expect, uint32_t *parent_inode,
			     uint32_t *name_off)
{
	uint32_t iflags;
	int filetype;
	if (ext4_parse_flags(flags, &iflags) == false)
		return EINVAL;

	if (file_expect == true)
		filetype = EXT4_DE_REG_FILE;
	else
		filetype = EXT4_DE_DIR;

	return ext4_generic_open2(f, path, iflags, filetype, parent_inode,
				  name_off);
}

static int ext4_create_hardlink(const char *path,
		struct ext4_inode_ref *child_ref, bool rename)
{
	bool is_goal = false;
	uint32_t inode_mode = EXT4_INODE_MODE_DIRECTORY;
	uint32_t next_inode;

	int r;
	int len;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	struct ext4_dir_search_result result;
	struct ext4_inode_ref ref;

	if (!mp)
		return ENOENT;

	struct ext4_fs *const fs = &mp->fs;
	struct ext4_sblock *const sb = &mp->fs.sb;

	/*Skip mount point*/
	path += strlen(mp->name);

	/*Load root*/
	r = ext4_fs_get_inode_ref(fs, EXT4_INODE_ROOT_INDEX, &ref);
	if (r != EOK)
		return r;

	len = ext4_path_check(path, &is_goal);
	while (1) {

		len = ext4_path_check(path, &is_goal);
		if (!len) {
			/*If root open was request.*/
			r = is_goal ? EINVAL : ENOENT;
			break;
		}

		r = ext4_dir_find_entry(&result, &ref, path, len);
		if (r != EOK) {

			/*Destroy last result*/
			ext4_dir_destroy_result(&ref, &result);

			if (r != ENOENT || !is_goal)
				break;

			/*Link with root dir.*/
			r = ext4_link(mp, &ref, child_ref, path, len, rename);
			break;
		} else if (r == EOK && is_goal) {
			/*Destroy last result*/
			ext4_dir_destroy_result(&ref, &result);
			r = EEXIST;
			break;
		}

		next_inode = result.dentry->inode;
		if (ext4_sb_feature_incom(sb, EXT4_FINCOM_FILETYPE)) {
			uint8_t t;
			t = ext4_dir_en_get_inode_type(sb, result.dentry);
			inode_mode = ext4_fs_correspond_inode_mode(t);
		} else {
			struct ext4_inode_ref child_ref;
			r = ext4_fs_get_inode_ref(fs, next_inode, &child_ref);
			if (r != EOK)
				break;

			inode_mode = ext4_inode_type(sb, child_ref.inode);
			ext4_fs_put_inode_ref(&child_ref);
		}

		r = ext4_dir_destroy_result(&ref, &result);
		if (r != EOK)
			break;

		if (inode_mode != EXT4_INODE_MODE_DIRECTORY) {
			r = is_goal ? EEXIST : ENOENT;
			break;
		}

		r = ext4_fs_put_inode_ref(&ref);
		if (r != EOK)
			break;

		r = ext4_fs_get_inode_ref(fs, next_inode, &ref);
		if (r != EOK)
			break;

		if (is_goal)
			break;

		path += len + 1;
	};

	if (r != EOK) {
		ext4_fs_put_inode_ref(&ref);
		return r;
	}

	r = ext4_fs_put_inode_ref(&ref);
	return r;
}

static int ext4_remove_orig_reference(const char *path, uint32_t name_off,
				      struct ext4_inode_ref *parent_ref,
				      struct ext4_inode_ref *child_ref)
{
	bool is_goal;
	int r;
	int len;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	/*Set path*/
	path += name_off;

	len = ext4_path_check(path, &is_goal);

	/* Remove entry from parent directory */
	r = ext4_dir_remove_entry(parent_ref, path, len);
	if (r != EOK)
		goto Finish;

	if (ext4_inode_is_type(&mp->fs.sb, child_ref->inode,
			       EXT4_INODE_MODE_DIRECTORY)) {
		ext4_fs_inode_links_count_dec(parent_ref);
		parent_ref->dirty = true;
	}
Finish:
	return r;
}

int ext4_flink(const char *path, const char *hardlink_path)
{
	int r;
	ext4_file f;
	uint32_t name_off;
	bool child_loaded = false;
	uint32_t parent_inode, child_inode;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	struct ext4_mountpoint *target_mp = ext4_get_mount(hardlink_path);
	struct ext4_inode_ref child_ref;

	if (!mp)
		return ENOENT;

	/* Will that happen? Anyway return EINVAL for such case. */
	if (mp != target_mp)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN,
			       &parent_inode, &name_off);
	if (r != EOK)
		goto Finish;

	child_inode = f.inode;
	ext4_fclose(&f);

	/*We have file to unlink. Load it.*/
	r = ext4_fs_get_inode_ref(&mp->fs, child_inode, &child_ref);
	if (r != EOK)
		goto Finish;

	child_loaded = true;

	/* Creating hardlink for directory is not allowed. */
	if (ext4_inode_is_type(&mp->fs.sb, child_ref.inode,
			       EXT4_INODE_MODE_DIRECTORY)) {
		r = EINVAL;
		goto Finish;
	}

	r = ext4_create_hardlink(hardlink_path, &child_ref, false);

Finish:
	if (child_loaded)
		ext4_fs_put_inode_ref(&child_ref);

	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;

}

int ext4_frename(const char *path, const char *new_path)
{
	int r;
	ext4_file f;
	uint32_t name_off;
	bool parent_loaded = false, child_loaded = false;
	uint32_t parent_inode, child_inode;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	struct ext4_inode_ref child_ref, parent_ref;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN,
				&parent_inode, &name_off);
	if (r != EOK)
		goto Finish;

	child_inode = f.inode;
	ext4_fclose(&f);

	/*Load parent*/
	r = ext4_fs_get_inode_ref(&mp->fs, parent_inode, &parent_ref);
	if (r != EOK)
		goto Finish;

	parent_loaded = true;

	/*We have file to unlink. Load it.*/
	r = ext4_fs_get_inode_ref(&mp->fs, child_inode, &child_ref);
	if (r != EOK)
		goto Finish;

	child_loaded = true;

	r = ext4_create_hardlink(new_path, &child_ref, true);
	if (r != EOK)
		goto Finish;

	r = ext4_remove_orig_reference(path, name_off, &parent_ref, &child_ref);
	if (r != EOK)
		goto Finish;

Finish:
	if (parent_loaded)
		ext4_fs_put_inode_ref(&parent_ref);

	if (child_loaded)
		ext4_fs_put_inode_ref(&child_ref);

	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;

}

/****************************************************************************/

int ext4_get_sblock(const char *mount_point, struct ext4_sblock **sb)
{
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);

	if (!mp)
		return ENOENT;

	*sb = &mp->fs.sb;
	return EOK;
}

int ext4_cache_write_back(const char *path, bool on)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_block_cache_write_back(mp->fs.bdev, on);
	EXT4_MP_UNLOCK(mp);
	return EOK;
}

int ext4_fremove(const char *path)
{
	ext4_file f;
	uint32_t parent_inode;
	uint32_t name_off;
	bool is_goal;
	int r;
	int len;
	struct ext4_inode_ref child;
	struct ext4_inode_ref parent;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN,
			       &parent_inode, &name_off);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	/*Load parent*/
	r = ext4_fs_get_inode_ref(&mp->fs, parent_inode, &parent);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	/*We have file to delete. Load it.*/
	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, &child);
	if (r != EOK) {
		ext4_fs_put_inode_ref(&parent);
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}
	/* We do not allow opening files here. */
	if (ext4_inode_type(&mp->fs.sb, child.inode) ==
	    EXT4_INODE_MODE_DIRECTORY) {
		ext4_fs_put_inode_ref(&parent);
		ext4_fs_put_inode_ref(&child);
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	/*Link count will be zero, the inode should be freed. */
	if (ext4_inode_get_links_cnt(child.inode) == 1) {
		ext4_block_cache_write_back(mp->fs.bdev, 1);
		r = ext4_trunc_inode(mp, child.index, 0);
		if (r != EOK) {
			ext4_fs_put_inode_ref(&parent);
			ext4_fs_put_inode_ref(&child);
			ext4_trans_abort(mp);
			EXT4_MP_UNLOCK(mp);
			return r;
		}
		ext4_block_cache_write_back(mp->fs.bdev, 0);
	}

	/*Set path*/
	path += name_off;

	len = ext4_path_check(path, &is_goal);

	/*Unlink from parent*/
	r = ext4_unlink(mp, &parent, &child, path, len);
	if (r != EOK)
		goto Finish;

	/*Link count is zero, the inode should be freed. */
	if (!ext4_inode_get_links_cnt(child.inode)) {
		ext4_inode_set_del_time(child.inode, -1L);

		r = ext4_fs_free_inode(&child);
		if (r != EOK)
			goto Finish;
	}

Finish:
	ext4_fs_put_inode_ref(&child);
	ext4_fs_put_inode_ref(&parent);

	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_fill_raw_inode(const char *path, uint32_t *ret_ino,
			struct ext4_inode *inode)
{
	int r;
	ext4_file f;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	uint32_t ino;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	ino = f.inode;
	ext4_fclose(&f);

	/*Load parent*/
	r = ext4_fs_get_inode_ref(&mp->fs, ino, &inode_ref);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	memcpy(inode, inode_ref.inode, sizeof(struct ext4_inode));
	ext4_fs_put_inode_ref(&inode_ref);
	EXT4_MP_UNLOCK(mp);

	if (ret_ino)
		*ret_ino = ino;

	return r;
}

int ext4_fopen(ext4_file *f, const char *path, const char *flags)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	ext4_block_cache_write_back(mp->fs.bdev, 1);
	r = ext4_generic_open(f, path, flags, true, 0, 0);
	ext4_block_cache_write_back(mp->fs.bdev, 0);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_fopen2(ext4_file *f, const char *path, int flags)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;
	int filetype;

	if (!mp)
		return ENOENT;

        filetype = EXT4_DE_REG_FILE;

	EXT4_MP_LOCK(mp);

	ext4_block_cache_write_back(mp->fs.bdev, 1);
	r = ext4_generic_open2(f, path, flags, filetype, NULL, NULL);
	ext4_block_cache_write_back(mp->fs.bdev, 0);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_fclose(ext4_file *f)
{
	ext4_assert(f && f->mp);

	f->mp = 0;
	f->flags = 0;
	f->inode = 0;
	f->fpos = f->fsize = 0;

	return EOK;
}

static int ext4_ftruncate_no_lock(ext4_file *f, uint64_t size)
{
	struct ext4_inode_ref ref;
	int r;


	r = ext4_fs_get_inode_ref(&f->mp->fs, f->inode, &ref);
	if (r != EOK) {
		EXT4_MP_UNLOCK(f->mp);
		return r;
	}

	/*Sync file size*/
	f->fsize = ext4_inode_get_size(&f->mp->fs.sb, ref.inode);
	if (f->fsize <= size) {
		r = EOK;
		goto Finish;
	}

	/*Start write back cache mode.*/
	r = ext4_block_cache_write_back(f->mp->fs.bdev, 1);
	if (r != EOK)
		goto Finish;

	r = ext4_trunc_inode(f->mp, ref.index, size);
	if (r != EOK)
		goto Finish;

	f->fsize = size;
	if (f->fpos > size)
		f->fpos = size;

	/*Stop write back cache mode*/
	ext4_block_cache_write_back(f->mp->fs.bdev, 0);

	if (r != EOK)
		goto Finish;

Finish:
	ext4_fs_put_inode_ref(&ref);
	return r;

}

int ext4_ftruncate(ext4_file *f, uint64_t size)
{
	int r;
	ext4_assert(f && f->mp);

	if (f->flags & O_RDONLY)
		return EPERM;

	EXT4_MP_LOCK(f->mp);

	ext4_trans_start(f->mp);
	r = ext4_ftruncate_no_lock(f, size);
	if (r != EOK)
		ext4_trans_abort(f->mp);
	else
		ext4_trans_stop(f->mp);

	EXT4_MP_UNLOCK(f->mp);
	return r;
}

int ext4_fread(ext4_file *f, void *buf, size_t size, size_t *rcnt)
{
	uint32_t unalg;
	uint32_t iblock_idx;
	uint32_t iblock_last;
	uint32_t block_size;

	ext4_fsblk_t fblock;
	ext4_fsblk_t fblock_start;
	uint32_t fblock_count;

	uint8_t *u8_buf = buf;
	int r;
	struct ext4_inode_ref ref;

	ext4_assert(f && f->mp);

	if (f->flags & O_WRONLY)
		return EPERM;

	if (!size)
		return EOK;

	EXT4_MP_LOCK(f->mp);

	struct ext4_fs *const fs = &f->mp->fs;
	struct ext4_sblock *const sb = &f->mp->fs.sb;

	if (rcnt)
		*rcnt = 0;

	r = ext4_fs_get_inode_ref(fs, f->inode, &ref);
	if (r != EOK) {
		EXT4_MP_UNLOCK(f->mp);
		return r;
	}

	/*Sync file size*/
	f->fsize = ext4_inode_get_size(sb, ref.inode);

	block_size = ext4_sb_get_block_size(sb);
	size = size > (f->fsize - f->fpos) ? (f->fsize - f->fpos) : size;

	iblock_idx = (f->fpos) / block_size;
	iblock_last = (f->fpos + size) / block_size;
	unalg = (f->fpos) % block_size;

	/*If the size of symlink is smaller than 60 bytes*/
	bool softlink;
	softlink = ext4_inode_is_type(sb, ref.inode, EXT4_INODE_MODE_SOFTLINK);
	if (softlink && f->fsize < sizeof(ref.inode->blocks)
		     && !ext4_inode_get_blocks_count(sb, ref.inode)) {

		char *content = (char *)ref.inode->blocks;
		if (f->fpos < f->fsize) {
			size_t len = size;
			if (unalg + size > f->fsize)
				len = f->fsize - unalg;
			memcpy(buf, content + unalg, len);
			if (rcnt)
				*rcnt = len;

		}

		r = EOK;
		goto Finish;
	}

	if (unalg) {
		size_t len =  size;
		if (size > (block_size - unalg))
			len = block_size - unalg;

		r = ext4_fs_get_inode_dblk_idx(&ref, iblock_idx, &fblock, true);
		if (r != EOK)
			goto Finish;

		/* Do we get an unwritten range? */
		if (fblock != 0) {
			uint64_t off = fblock * block_size + unalg;
			r = ext4_block_readbytes(f->mp->fs.bdev, off, u8_buf, len);
			if (r != EOK)
				goto Finish;

		} else {
			/* Yes, we do. */
			memset(u8_buf, 0, len);
		}

		u8_buf += len;
		size -= len;
		f->fpos += len;

		if (rcnt)
			*rcnt += len;

		iblock_idx++;
	}

	fblock_start = 0;
	fblock_count = 0;
	while (size >= block_size) {
		while (iblock_idx < iblock_last) {
			r = ext4_fs_get_inode_dblk_idx(&ref, iblock_idx,
						       &fblock, true);
			if (r != EOK)
				goto Finish;

			iblock_idx++;

			if (!fblock_start)
				fblock_start = fblock;

			if ((fblock_start + fblock_count) != fblock)
				break;

			fblock_count++;
		}

		r = ext4_blocks_get_direct(f->mp->fs.bdev, u8_buf, fblock_start,
					   fblock_count);
		if (r != EOK)
			goto Finish;

		size -= block_size * fblock_count;
		u8_buf += block_size * fblock_count;
		f->fpos += block_size * fblock_count;

		if (rcnt)
			*rcnt += block_size * fblock_count;

		fblock_start = fblock;
		fblock_count = 1;
	}

	if (size) {
		uint64_t off;
		r = ext4_fs_get_inode_dblk_idx(&ref, iblock_idx, &fblock, true);
		if (r != EOK)
			goto Finish;

		off = fblock * block_size;
		r = ext4_block_readbytes(f->mp->fs.bdev, off, u8_buf, size);
		if (r != EOK)
			goto Finish;

		f->fpos += size;

		if (rcnt)
			*rcnt += size;
	}

Finish:
	ext4_fs_put_inode_ref(&ref);
	EXT4_MP_UNLOCK(f->mp);
	return r;
}

int ext4_fwrite(ext4_file *f, const void *buf, size_t size, size_t *wcnt)
{
	uint32_t unalg;
	uint32_t iblk_idx;
	uint32_t iblock_last;
	uint32_t ifile_blocks;
	uint32_t block_size;

	uint32_t fblock_count;
	ext4_fsblk_t fblk;
	ext4_fsblk_t fblock_start;

	struct ext4_inode_ref ref;
	const uint8_t *u8_buf = buf;
	int r, rr = EOK;

	ext4_assert(f && f->mp);

	if (f->flags & O_RDONLY)
		return EPERM;

	if (!size)
		return EOK;

	EXT4_MP_LOCK(f->mp);
	ext4_trans_start(f->mp);

	struct ext4_fs *const fs = &f->mp->fs;
	struct ext4_sblock *const sb = &f->mp->fs.sb;

	if (wcnt)
		*wcnt = 0;

	r = ext4_fs_get_inode_ref(fs, f->inode, &ref);
	if (r != EOK) {
		ext4_trans_abort(f->mp);
		EXT4_MP_UNLOCK(f->mp);
		return r;
	}

	/*Sync file size*/
	f->fsize = ext4_inode_get_size(sb, ref.inode);
	block_size = ext4_sb_get_block_size(sb);

	iblock_last = (f->fpos + size) / block_size;
	iblk_idx = (f->fpos) / block_size;
	ifile_blocks = (f->fsize + block_size - 1) / block_size;

	unalg = (f->fpos) % block_size;

	if (unalg) {
		size_t len =  size;
		uint64_t off;
		if (size > (block_size - unalg))
			len = block_size - unalg;

		r = ext4_fs_init_inode_dblk_idx(&ref, iblk_idx, &fblk);
		if (r != EOK)
			goto Finish;

		off = fblk * block_size + unalg;
		r = ext4_block_writebytes(f->mp->fs.bdev, off, u8_buf, len);
		if (r != EOK)
			goto Finish;

		u8_buf += len;
		size -= len;
		f->fpos += len;

		if (wcnt)
			*wcnt += len;

		iblk_idx++;
	}

	/*Start write back cache mode.*/
	r = ext4_block_cache_write_back(f->mp->fs.bdev, 1);
	if (r != EOK)
		goto Finish;

	fblock_start = 0;
	fblock_count = 0;
	while (size >= block_size) {

		while (iblk_idx < iblock_last) {
			if (iblk_idx < ifile_blocks) {
				r = ext4_fs_init_inode_dblk_idx(&ref, iblk_idx,
								&fblk);
				if (r != EOK)
					goto Finish;
			} else {
				rr = ext4_fs_append_inode_dblk(&ref, &fblk,
							       &iblk_idx);
				if (rr != EOK) {
					/* Unable to append more blocks. But
					 * some block might be allocated already
					 * */
					break;
				}
			}

			iblk_idx++;

			if (!fblock_start) {
				fblock_start = fblk;
			}

			if ((fblock_start + fblock_count) != fblk)
				break;

			fblock_count++;
		}

		r = ext4_blocks_set_direct(f->mp->fs.bdev, u8_buf, fblock_start,
					   fblock_count);
		if (r != EOK)
			break;

		size -= block_size * fblock_count;
		u8_buf += block_size * fblock_count;
		f->fpos += block_size * fblock_count;

		if (wcnt)
			*wcnt += block_size * fblock_count;

		fblock_start = fblk;
		fblock_count = 1;

		if (rr != EOK) {
			/*ext4_fs_append_inode_block has failed and no
			 * more blocks might be written. But node size
			 * should be updated.*/
			r = rr;
			goto out_fsize;
		}
	}

	/*Stop write back cache mode*/
	ext4_block_cache_write_back(f->mp->fs.bdev, 0);

	if (r != EOK)
		goto Finish;

	if (size) {
		uint64_t off;
		if (iblk_idx < ifile_blocks) {
			r = ext4_fs_init_inode_dblk_idx(&ref, iblk_idx, &fblk);
			if (r != EOK)
				goto Finish;
		} else {
			r = ext4_fs_append_inode_dblk(&ref, &fblk, &iblk_idx);
			if (r != EOK)
				/*Node size sholud be updated.*/
				goto out_fsize;
		}

		off = fblk * block_size;
		r = ext4_block_writebytes(f->mp->fs.bdev, off, u8_buf, size);
		if (r != EOK)
			goto Finish;

		f->fpos += size;

		if (wcnt)
			*wcnt += size;
	}

out_fsize:
	if (f->fpos > f->fsize) {
		f->fsize = f->fpos;
		ext4_inode_set_size(ref.inode, f->fsize);
		ref.dirty = true;
	}

Finish:
	r = ext4_fs_put_inode_ref(&ref);

	if (r != EOK)
		ext4_trans_abort(f->mp);
	else
		ext4_trans_stop(f->mp);

	EXT4_MP_UNLOCK(f->mp);
	return r;
}

int ext4_fseek(ext4_file *f, uint64_t offset, uint32_t origin)
{
	switch (origin) {
	case SEEK_SET:
		if (offset > f->fsize)
			return EINVAL;

		f->fpos = offset;
		return EOK;
	case SEEK_CUR:
		if ((offset + f->fpos) > f->fsize)
			return EINVAL;

		f->fpos += offset;
		return EOK;
	case SEEK_END:
		if (offset > f->fsize)
			return EINVAL;

		f->fpos = f->fsize - offset;
		return EOK;
	}
	return EINVAL;
}

uint64_t ext4_ftell(ext4_file *f)
{
	return f->fpos;
}

uint64_t ext4_fsize(ext4_file *f)
{
	return f->fsize;
}

int ext4_chmod(const char *path, uint32_t mode)
{
	int r;
	uint32_t ino;
	ext4_file f;
	struct ext4_sblock *sb;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}
	ino = f.inode;
	sb = &mp->fs.sb;
	ext4_fclose(&f);
	r = ext4_fs_get_inode_ref(&mp->fs, ino, &inode_ref);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	ext4_inode_set_mode(sb, inode_ref.inode, mode);
	inode_ref.dirty = true;

	r = ext4_fs_put_inode_ref(&inode_ref);
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_chown(const char *path, uint32_t uid, uint32_t gid)
{
	int r;
	ext4_file f;
	uint32_t ino;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}
	ino = f.inode;
	ext4_fclose(&f);
	r = ext4_fs_get_inode_ref(&mp->fs, ino, &inode_ref);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	ext4_inode_set_uid(inode_ref.inode, uid);
	ext4_inode_set_gid(inode_ref.inode, gid);
	inode_ref.dirty = true;

	r = ext4_fs_put_inode_ref(&inode_ref);
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_file_set_atime(const char *path, uint32_t atime)
{
	int r;
	ext4_file f;
	uint32_t ino;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}
	ino = f.inode;
	ext4_fclose(&f);
	r = ext4_fs_get_inode_ref(&mp->fs, ino, &inode_ref);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	ext4_inode_set_access_time(inode_ref.inode, atime);
	inode_ref.dirty = true;

	r = ext4_fs_put_inode_ref(&inode_ref);
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_file_set_mtime(const char *path, uint32_t mtime)
{
	int r;
	ext4_file f;
	uint32_t ino;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}
	ino = f.inode;
	ext4_fclose(&f);
	r = ext4_fs_get_inode_ref(&mp->fs, ino, &inode_ref);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	ext4_inode_set_modif_time(inode_ref.inode, mtime);
	inode_ref.dirty = true;

	r = ext4_fs_put_inode_ref(&inode_ref);
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_file_set_ctime(const char *path, uint32_t ctime)
{
	int r;
	ext4_file f;
	uint32_t ino;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}
	ino = f.inode;
	ext4_fclose(&f);
	r = ext4_fs_get_inode_ref(&mp->fs, ino, &inode_ref);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	ext4_inode_set_change_inode_time(inode_ref.inode, ctime);
	inode_ref.dirty = true;

	r = ext4_fs_put_inode_ref(&inode_ref);
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

static int ext4_fsymlink_set(ext4_file *f, const void *buf, uint32_t size)
{
	struct ext4_inode_ref ref;
	uint32_t sblock;
	ext4_fsblk_t fblock;
	uint32_t block_size;
	int r;

	ext4_assert(f && f->mp);

	if (!size)
		return EOK;

	r = ext4_fs_get_inode_ref(&f->mp->fs, f->inode, &ref);
	if (r != EOK)
		return r;

	/*Sync file size*/
	block_size = ext4_sb_get_block_size(&f->mp->fs.sb);
	if (size > block_size) {
		r = EINVAL;
		goto Finish;
	}
	r = ext4_ftruncate_no_lock(f, 0);
	if (r != EOK)
		goto Finish;

	/*Start write back cache mode.*/
	r = ext4_block_cache_write_back(f->mp->fs.bdev, 1);
	if (r != EOK)
		goto Finish;

	/*If the size of symlink is smaller than 60 bytes*/
	if (size < sizeof(ref.inode->blocks)) {
		memset(ref.inode->blocks, 0, sizeof(ref.inode->blocks));
		memcpy(ref.inode->blocks, buf, size);
		ext4_inode_clear_flag(ref.inode, EXT4_INODE_FLAG_EXTENTS);
	} else {
		ext4_fs_inode_blocks_init(&f->mp->fs, &ref);
		r = ext4_fs_append_inode_dblk(&ref, &fblock, &sblock);
		if (r != EOK)
			goto Finish;

		r = ext4_block_writebytes(f->mp->fs.bdev, 0, buf, size);
		if (r != EOK)
			goto Finish;

	}

	/*Stop write back cache mode*/
	ext4_block_cache_write_back(f->mp->fs.bdev, 0);

	if (r != EOK)
		goto Finish;

	ext4_inode_set_size(ref.inode, size);
	ref.dirty = true;

	f->fsize = size;
	if (f->fpos > size)
		f->fpos = size;

Finish:
	ext4_fs_put_inode_ref(&ref);
	return r;
}

int ext4_fsymlink(const char *target, const char *path)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;
	ext4_file f;
	int filetype;

	if (!mp)
		return ENOENT;

	filetype = EXT4_DE_SYMLINK;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	ext4_block_cache_write_back(mp->fs.bdev, 1);
	r = ext4_generic_open2(&f, path, O_RDWR|O_CREAT, filetype, NULL, NULL);
	if (r == EOK)
		r = ext4_fsymlink_set(&f, target, strlen(target));
	else
		goto Finish;

	ext4_fclose(&f);

Finish:
	ext4_block_cache_write_back(mp->fs.bdev, 0);

	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_readlink(const char *path, char *buf, size_t bufsize, size_t *rcnt)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;
	ext4_file f;
	int filetype;

	if (!mp)
		return ENOENT;

	if (!buf)
		return EINVAL;

	filetype = EXT4_DE_SYMLINK;

	EXT4_MP_LOCK(mp);
	ext4_block_cache_write_back(mp->fs.bdev, 1);
	r = ext4_generic_open2(&f, path, O_RDONLY, filetype, NULL, NULL);
	if (r == EOK)
		r = ext4_fread(&f, buf, bufsize, rcnt);
	else
		goto Finish;

	ext4_fclose(&f);

Finish:
	ext4_block_cache_write_back(mp->fs.bdev, 0);
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_setxattr(const char *path, const char *name, size_t name_len,
		  const void *data, size_t data_size, bool replace)
{
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	uint8_t name_index;
	const char *dissected_name = NULL;
	size_t dissected_len = 0;
	struct ext4_xattr_ref xattr_ref;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	dissected_name = ext4_extract_xattr_name(name, name_len,
				&name_index, &dissected_len);
	if (!dissected_len)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;
	inode = f.inode;
	ext4_fclose(&f);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_xattr_ref(&mp->fs, &inode_ref, &xattr_ref);
	if (r != EOK) {
		ext4_fs_put_inode_ref(&inode_ref);
		goto Finish;
	}

	r = ext4_fs_set_xattr(&xattr_ref, name_index, dissected_name,
			dissected_len, data, data_size, replace);

	ext4_fs_put_xattr_ref(&xattr_ref);
	ext4_fs_put_inode_ref(&inode_ref);
Finish:
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_getxattr(const char *path, const char *name, size_t name_len,
		  void *buf, size_t buf_size, size_t *data_size)
{
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	uint8_t name_index;
	const char *dissected_name = NULL;
	size_t dissected_len = 0;
	struct ext4_xattr_ref xattr_ref;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	dissected_name = ext4_extract_xattr_name(name, name_len,
				&name_index, &dissected_len);
	if (!dissected_len)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;
	inode = f.inode;
	ext4_fclose(&f);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_xattr_ref(&mp->fs, &inode_ref, &xattr_ref);
	if (r != EOK) {
		ext4_fs_put_inode_ref(&inode_ref);
		goto Finish;
	}

	r = ext4_fs_get_xattr(&xattr_ref, name_index, dissected_name,
				dissected_len, buf, buf_size, data_size);

	ext4_fs_put_xattr_ref(&xattr_ref);
	ext4_fs_put_inode_ref(&inode_ref);
Finish:
	EXT4_MP_UNLOCK(mp);
	return r;
}

struct ext4_listxattr_iterator {
	char *list;
	char *list_ptr;
	size_t size;
	size_t ret_size;
	bool list_too_small;
	bool get_required_size;
};

static int ext4_iterate_ea_list(struct ext4_xattr_ref *ref,
				struct ext4_xattr_item *item)
{
	struct ext4_listxattr_iterator *lxi;
	lxi = ref->iter_arg;
	if (!lxi->get_required_size) {
		size_t plen;
		const char *prefix;
		prefix = ext4_get_xattr_name_prefix(item->name_index, &plen);
		if (lxi->ret_size + plen + item->name_len + 1 > lxi->size) {
			lxi->list_too_small = 1;
			return EXT4_XATTR_ITERATE_STOP;
		}
		if (prefix) {
			memcpy(lxi->list_ptr, prefix, plen);
			lxi->list_ptr += plen;
			lxi->ret_size += plen;
		}
		memcpy(lxi->list_ptr, item->name, item->name_len);
		lxi->list_ptr[item->name_len] = 0;
		lxi->list_ptr += item->name_len + 1;
	}
	lxi->ret_size += item->name_len + 1;
	return EXT4_XATTR_ITERATE_CONT;
}

int ext4_listxattr(const char *path, char *list, size_t size, size_t *ret_size)
{
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	struct ext4_xattr_ref xattr_ref;
	struct ext4_inode_ref inode_ref;
	struct ext4_listxattr_iterator lxi;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	lxi.list = list;
	lxi.list_ptr = list;
	lxi.size = size;
	lxi.ret_size = 0;
	lxi.list_too_small = false;
	lxi.get_required_size = (!size) ? true : false;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;
	inode = f.inode;
	ext4_fclose(&f);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_xattr_ref(&mp->fs, &inode_ref, &xattr_ref);
	if (r != EOK) {
		ext4_fs_put_inode_ref(&inode_ref);
		goto Finish;
	}

	xattr_ref.iter_arg = &lxi;
	ext4_fs_xattr_iterate(&xattr_ref, ext4_iterate_ea_list);
	if (lxi.list_too_small)
		r = ERANGE;

	if (r == EOK) {
		if (ret_size)
			*ret_size = lxi.ret_size;

	}
	ext4_fs_put_xattr_ref(&xattr_ref);
	ext4_fs_put_inode_ref(&inode_ref);
Finish:
	EXT4_MP_UNLOCK(mp);
	return r;

}

int ext4_removexattr(const char *path, const char *name, size_t name_len)
{
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	uint8_t name_index;
	const char *dissected_name = NULL;
	size_t dissected_len = 0;
	struct ext4_xattr_ref xattr_ref;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	dissected_name = ext4_extract_xattr_name(name, name_len,
						&name_index, &dissected_len);
	if (!dissected_len)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;
	inode = f.inode;
	ext4_fclose(&f);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_xattr_ref(&mp->fs, &inode_ref, &xattr_ref);
	if (r != EOK) {
		ext4_fs_put_inode_ref(&inode_ref);
		goto Finish;
	}

	r = ext4_fs_remove_xattr(&xattr_ref, name_index, dissected_name,
				dissected_len);

	ext4_fs_put_xattr_ref(&xattr_ref);
	ext4_fs_put_inode_ref(&inode_ref);
Finish:
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	EXT4_MP_UNLOCK(mp);
	return r;

}

/*********************************DIRECTORY OPERATION************************/

int ext4_dir_rm(const char *path)
{
	int r;
	int len;
	ext4_file f;

	struct ext4_mountpoint *mp = ext4_get_mount(path);
	struct ext4_inode_ref act;
	struct ext4_inode_ref child;
	struct ext4_dir_iter it;

	uint32_t name_off;
	uint32_t inode_up;
	uint32_t inode_current;
	uint32_t depth = 1;

	bool has_children;
	bool is_goal;
	bool dir_end;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	struct ext4_fs *const fs = &mp->fs;

	/*Check if exist.*/
	r = ext4_generic_open(&f, path, "r", false, &inode_up, &name_off);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	path += name_off;
	len = ext4_path_check(path, &is_goal);

	inode_current = f.inode;

	ext4_block_cache_write_back(mp->fs.bdev, 1);

	do {

		uint64_t act_curr_pos = 0;
		has_children = false;
		dir_end = false;

		while (r == EOK && !has_children && !dir_end) {

			/*Load directory node.*/
			r = ext4_fs_get_inode_ref(fs, inode_current, &act);
			if (r != EOK) {
				break;
			}

			/*Initialize iterator.*/
			r = ext4_dir_iterator_init(&it, &act, act_curr_pos);
			if (r != EOK) {
				ext4_fs_put_inode_ref(&act);
				break;
			}

			if (!it.curr) {
				dir_end = true;
				goto End;
			}

			ext4_trans_start(mp);

			/*Get up directory inode when ".." entry*/
			if ((it.curr->name_len == 2) &&
			    ext4_is_dots(it.curr->name, it.curr->name_len)) {
				inode_up = ext4_dir_en_get_inode(it.curr);
			}

			/*If directory or file entry,  but not "." ".." entry*/
			if (!ext4_is_dots(it.curr->name, it.curr->name_len)) {

				/*Get child inode reference do unlink
				 * directory/file.*/
				uint32_t cinode;
				cinode = ext4_dir_en_get_inode(it.curr);
				r = ext4_fs_get_inode_ref(fs, cinode, &child);
				if (r != EOK)
					goto End;

				/*If directory with no leaf children*/
				r = ext4_has_children(&has_children, &child);
				if (r != EOK) {
					ext4_fs_put_inode_ref(&child);
					goto End;
				}

				if (has_children) {
					/*Has directory children. Go into this
					 * directory.*/
					inode_up = inode_current;
					inode_current = cinode;
					depth++;
					ext4_fs_put_inode_ref(&child);
					goto End;
				}

				/* Truncate */
				r = ext4_fs_truncate_inode(&child, 0);
				if (r != EOK) {
					ext4_fs_put_inode_ref(&child);
					goto End;
				}

				/*No children in child directory or file. Just
				 * unlink.*/
				r = ext4_unlink(f.mp, &act, &child,
						(char *)it.curr->name,
						it.curr->name_len);
				if (r != EOK) {
					ext4_fs_put_inode_ref(&child);
					goto End;
				}

				ext4_inode_set_del_time(child.inode, -1L);
				ext4_inode_set_links_cnt(child.inode, 0);
				child.dirty = true;

				r = ext4_fs_free_inode(&child);
				if (r != EOK) {
					ext4_fs_put_inode_ref(&child);
					goto End;
				}

				r = ext4_fs_put_inode_ref(&child);
				if (r != EOK)
					goto End;

			}

			r = ext4_dir_iterator_next(&it);
			if (r != EOK)
				goto End;

			act_curr_pos = it.curr_off;
End:
			ext4_dir_iterator_fini(&it);
			if (r == EOK)
				r = ext4_fs_put_inode_ref(&act);
			else
				ext4_fs_put_inode_ref(&act);

			if (r != EOK)
				ext4_trans_abort(mp);
			else
				ext4_trans_stop(mp);
		}

		if (dir_end) {
			/*Directory iterator reached last entry*/
			depth--;
			if (depth)
				inode_current = inode_up;

		}

		if (r != EOK)
			break;

	} while (depth);

	/*Last unlink*/
	if (r == EOK && !depth) {
		/*Load parent.*/
		struct ext4_inode_ref parent;
		r = ext4_fs_get_inode_ref(&f.mp->fs, inode_up,
				&parent);
		if (r != EOK)
			goto Finish;
		r = ext4_fs_get_inode_ref(&f.mp->fs, inode_current,
				&act);
		if (r != EOK) {
			ext4_fs_put_inode_ref(&act);
			goto Finish;
		}

		ext4_trans_start(mp);

		/* In this place all directories should be
		 * unlinked.
		 * Last unlink from root of current directory*/
		r = ext4_unlink(f.mp, &parent, &act,
				(char *)path, len);
		if (r != EOK) {
			ext4_fs_put_inode_ref(&parent);
			ext4_fs_put_inode_ref(&act);
			goto Finish;
		}

		if (ext4_inode_get_links_cnt(act.inode) == 2) {
			ext4_inode_set_del_time(act.inode, -1L);
			ext4_inode_set_links_cnt(act.inode, 0);
			act.dirty = true;
			/*Turncate*/
			r = ext4_fs_truncate_inode(&act, 0);
			if (r != EOK) {
				ext4_fs_put_inode_ref(&parent);
				ext4_fs_put_inode_ref(&act);
				goto Finish;
			}

			r = ext4_fs_free_inode(&act);
			if (r != EOK) {
				ext4_fs_put_inode_ref(&parent);
				ext4_fs_put_inode_ref(&act);
				goto Finish;
			}
		}

		r = ext4_fs_put_inode_ref(&parent);
		if (r != EOK)
			goto Finish;

		r = ext4_fs_put_inode_ref(&act);
	Finish:
		if (r != EOK)
			ext4_trans_abort(mp);
		else
			ext4_trans_stop(mp);
	}

	ext4_block_cache_write_back(mp->fs.bdev, 0);
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_dir_mk(const char *path)
{
	int r;
	ext4_file f;

	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ext4_trans_start(mp);

	/*Check if exist.*/
	r = ext4_generic_open(&f, path, "r", false, 0, 0);
	if (r == EOK) {
		/*Directory already created*/
		ext4_trans_stop(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	/*Create new dir*/
	r = ext4_generic_open(&f, path, "w", false, 0, 0);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	ext4_trans_stop(mp);
	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_dir_open(ext4_dir *d, const char *path)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open(&d->f, path, "r", false, 0, 0);
	d->next_off = 0;
	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_dir_close(ext4_dir *d)
{
    return ext4_fclose(&d->f);
}

const ext4_direntry *ext4_dir_entry_next(ext4_dir *d)
{
#define EXT4_DIR_ENTRY_OFFSET_TERM (uint64_t)(-1)

	int r;
	ext4_direntry *de = 0;
	struct ext4_inode_ref dir;
	struct ext4_dir_iter it;

	EXT4_MP_LOCK(d->f.mp);

	if (d->next_off == EXT4_DIR_ENTRY_OFFSET_TERM) {
		EXT4_MP_UNLOCK(d->f.mp);
		return 0;
	}

	r = ext4_fs_get_inode_ref(&d->f.mp->fs, d->f.inode, &dir);
	if (r != EOK) {
		goto Finish;
	}

	r = ext4_dir_iterator_init(&it, &dir, d->next_off);
	if (r != EOK) {
		ext4_fs_put_inode_ref(&dir);
		goto Finish;
	}

	memcpy(&d->de, it.curr, sizeof(ext4_direntry));
	de = &d->de;

	ext4_dir_iterator_next(&it);

	d->next_off = it.curr ? it.curr_off : EXT4_DIR_ENTRY_OFFSET_TERM;

	ext4_dir_iterator_fini(&it);
	ext4_fs_put_inode_ref(&dir);

Finish:
	EXT4_MP_UNLOCK(d->f.mp);
	return de;
}

void ext4_dir_entry_rewind(ext4_dir *d)
{
	d->next_off = 0;
}

int ext4_test_journal(const char *mount_point)
{
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);
	if (!mp)
		return ENOENT;

	int r = ENOTSUP;
	EXT4_MP_LOCK(mp);
	ext4_block_cache_write_back(mp->fs.bdev, 1);
	if (ext4_sb_feature_com(&mp->fs.sb, EXT4_FCOM_HAS_JOURNAL)) {
		struct jbd_fs *jbd_fs = calloc(1, sizeof(struct jbd_fs));
		struct jbd_journal *journal;
		if (!jbd_fs) {
			 r = ENOMEM;
			 goto Finish;
		}
		journal = calloc(1, sizeof(struct jbd_journal));
		if (!journal) {
			free(jbd_fs);
			r = ENOMEM;
			goto Finish;
		}

		r = jbd_get_fs(&mp->fs, jbd_fs);
		if (r != EOK) {
			free(jbd_fs);
			goto Finish;
		}
		r = jbd_journal_start(jbd_fs, journal);
		if (r != EOK) {
			jbd_put_fs(jbd_fs);
			free(journal);
			free(jbd_fs);
			goto Finish;
		}

		int i;
		for (i = 0;i < 50;i++) {
			ext4_fsblk_t rand_block = rand() % 4096;
			if (!rand_block)
				rand_block = 1;
			struct ext4_block block;
			r = ext4_block_get(mp->fs.bdev, &block, rand_block);
			if (r != EOK)
				goto out;

			struct jbd_trans *t = jbd_journal_new_trans(journal);
			if (!t) {
				ext4_block_set(mp->fs.bdev, &block);
				r = ENOMEM;
				goto out;
			}

			switch (rand() % 2) {
			case 0:
				r = jbd_trans_get_access(journal, t, &block);
				if (r != EOK) {
					jbd_journal_free_trans(journal, t,
							       true);
					ext4_block_set(mp->fs.bdev, &block);
					r = ENOMEM;
					goto out;
				}
				r = jbd_trans_set_block_dirty(t, &block);
				if (r != EOK) {
					jbd_journal_free_trans(journal, t,
							       true);
					ext4_block_set(mp->fs.bdev, &block);
					r = ENOMEM;
					goto out;
				}
				break;
			case 1:
				r = jbd_trans_try_revoke_block(t, rand_block);
				if (r != EOK) {
					jbd_journal_free_trans(journal, t,
							       true);
					ext4_block_set(mp->fs.bdev, &block);
					r = ENOMEM;
					goto out;
				}
			}
			ext4_block_set(mp->fs.bdev, &block);
			jbd_journal_submit_trans(journal, t);
			jbd_journal_commit_one(journal);
		}
out:
		jbd_journal_stop(journal);
		jbd_put_fs(jbd_fs);
		free(journal);
		free(jbd_fs);
	}

Finish:
	ext4_block_cache_write_back(mp->fs.bdev, 0);
	EXT4_MP_UNLOCK(mp);
	return r;
}

/**
 * @}
 */
