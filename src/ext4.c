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

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_oflags.h>
#include <ext4_debug.h>

#include <ext4.h>
#include <ext4_trans.h>
#include <ext4_blockdev.h>
#include <ext4_fs.h>
#include <ext4_dir.h>
#include <ext4_inode.h>
#include <ext4_super.h>
#include <ext4_block_group.h>
#include <ext4_dir_idx.h>
#include <ext4_xattr.h>
#include <ext4_journal.h>


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
	char name[CONFIG_EXT4_MAX_MP_NAME + 1];

	/**@brief   OS dependent lock/unlock functions.*/
	const struct ext4_lock *os_locks;

	/**@brief   Ext4 filesystem internals.*/
	struct ext4_fs fs;

	/**@brief   JBD fs.*/
	struct jbd_fs jbd_fs;

	/**@brief   Journal.*/
	struct jbd_journal jbd_journal;

	/**@brief   Block cache.*/
	struct ext4_bcache bc;
};

/**@brief   Block devices descriptor.*/
struct ext4_block_devices {

	/**@brief   Block device name.*/
	char name[CONFIG_EXT4_MAX_BLOCKDEV_NAME + 1];

	/**@brief   Block device handle.*/
	struct ext4_blockdev *bd;
};

/**@brief   Block devices.*/
static struct ext4_block_devices s_bdevices[CONFIG_EXT4_BLOCKDEVS_COUNT];

/**@brief   Mountpoints.*/
static struct ext4_mountpoint s_mp[CONFIG_EXT4_MOUNTPOINTS_COUNT];

int ext4_device_register(struct ext4_blockdev *bd,
			 const char *dev_name)
{
	ext4_assert(bd && dev_name);

	if (strlen(dev_name) > CONFIG_EXT4_MAX_BLOCKDEV_NAME)
		return EINVAL;

	for (size_t i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
		if (!strcmp(s_bdevices[i].name, dev_name))
			return EEXIST;
	}

	for (size_t i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
		if (!s_bdevices[i].bd) {
			strcpy(s_bdevices[i].name, dev_name);
			s_bdevices[i].bd = bd;
			return EOK;
		}
	}

	return ENOSPC;
}

int ext4_device_unregister(const char *dev_name)
{
	ext4_assert(dev_name);

	for (size_t i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
		if (strcmp(s_bdevices[i].name, dev_name))
			continue;

		memset(&s_bdevices[i], 0, sizeof(s_bdevices[i]));
		return EOK;
	}

	return ENOENT;
}

int ext4_device_unregister_all(void)
{
	memset(s_bdevices, 0, sizeof(s_bdevices));

	return EOK;
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

int ext4_mount(const char *dev_name, const char *mount_point,
	       bool read_only)
{
	int r;
	uint32_t bsize;
	struct ext4_bcache *bc;
	struct ext4_blockdev *bd = 0;
	struct ext4_mountpoint *mp = 0;

	ext4_assert(mount_point && dev_name);

	size_t mp_len = strlen(mount_point);

	if (mp_len > CONFIG_EXT4_MAX_MP_NAME)
		return EINVAL;

	if (mount_point[mp_len - 1] != '/')
		return ENOTSUP;

	for (size_t i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
		if (!strcmp(dev_name, s_bdevices[i].name)) {
			bd = s_bdevices[i].bd;
			break;
		}
	}

	if (!bd)
		return ENODEV;

	for (size_t i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
		if (!s_mp[i].mounted) {
			strcpy(s_mp[i].name, mount_point);
			mp = &s_mp[i];
			break;
		}

		if (!strcmp(s_mp[i].name, mount_point))
			return EOK;
	}

	if (!mp)
		return ENOMEM;

	r = ext4_block_init(bd);
	if (r != EOK)
		return r;

	r = ext4_fs_init(&mp->fs, bd, read_only);
	if (r != EOK) {
		ext4_block_fini(bd);
		return r;
	}

	bsize = ext4_sb_get_block_size(&mp->fs.sb);
	ext4_block_set_lb_size(bd, bsize);
	bc = &mp->bc;

	r = ext4_bcache_init_dynamic(bc, CONFIG_BLOCK_DEV_CACHE_SIZE, bsize);
	if (r != EOK) {
		ext4_block_fini(bd);
		return r;
	}

	if (bsize != bc->itemsize)
		return ENOTSUP;

	/*Bind block cache to block device*/
	r = ext4_block_bind_bcache(bd, bc);
	if (r != EOK) {
		ext4_bcache_cleanup(bc);
		ext4_block_fini(bd);
		ext4_bcache_fini_dynamic(bc);
		return r;
	}

	bd->fs = &mp->fs;
	mp->mounted = 1;
	return r;
}


int ext4_umount(const char *mount_point)
{
	int i;
	int r;
	struct ext4_mountpoint *mp = 0;

	for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
		if (!strcmp(s_mp[i].name, mount_point)) {
			mp = &s_mp[i];
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
	ext4_bcache_fini_dynamic(mp->fs.bdev->bc);

	r = ext4_block_fini(mp->fs.bdev);
Finish:
	mp->fs.bdev->fs = NULL;
	return r;
}

static struct ext4_mountpoint *ext4_get_mount(const char *path)
{
	for (size_t i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {

		if (!s_mp[i].mounted)
			continue;

		if (!strncmp(s_mp[i].name, path, strlen(s_mp[i].name)))
			return &s_mp[i];
	}

	return NULL;
}

__unused
static int __ext4_journal_start(const char *mount_point)
{
	int r = EOK;
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EOK;

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

__unused
static int __ext4_journal_stop(const char *mount_point)
{
	int r = EOK;
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EOK;

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

__unused
static int __ext4_recover(const char *mount_point)
{
	struct ext4_mountpoint *mp = ext4_get_mount(mount_point);
	int r = ENOTSUP;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	if (ext4_sb_feature_com(&mp->fs.sb, EXT4_FCOM_HAS_JOURNAL)) {
		struct jbd_fs *jbd_fs = ext4_calloc(1, sizeof(struct jbd_fs));
		if (!jbd_fs) {
			 r = ENOMEM;
			 goto Finish;
		}

		r = jbd_get_fs(&mp->fs, jbd_fs);
		if (r != EOK) {
			ext4_free(jbd_fs);
			goto Finish;
		}

		r = jbd_recover(jbd_fs);
		jbd_put_fs(jbd_fs);
		ext4_free(jbd_fs);
	}
	if (r == EOK && !mp->fs.read_only) {
		uint32_t bgid;
		uint64_t free_blocks_count = 0;
		uint32_t free_inodes_count = 0;
		struct ext4_block_group_ref bg_ref;

		/* Update superblock's stats */
		for (bgid = 0;bgid < ext4_block_group_cnt(&mp->fs.sb);bgid++) {
			r = ext4_fs_get_block_group_ref(&mp->fs, bgid, &bg_ref);
			if (r != EOK)
				goto Finish;

			free_blocks_count +=
				ext4_bg_get_free_blocks_count(bg_ref.block_group,
						&mp->fs.sb);
			free_inodes_count +=
				ext4_bg_get_free_inodes_count(bg_ref.block_group,
						&mp->fs.sb);

			ext4_fs_put_block_group_ref(&bg_ref);
		}
		ext4_sb_set_free_blocks_cnt(&mp->fs.sb, free_blocks_count);
		ext4_set32(&mp->fs.sb, free_inodes_count, free_inodes_count);
		/* We don't need to save the superblock stats immediately. */
	}

Finish:
	EXT4_MP_UNLOCK(mp);
	return r;
}

__unused
static int __ext4_trans_start(struct ext4_mountpoint *mp)
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

__unused
static int __ext4_trans_stop(struct ext4_mountpoint *mp)
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

__unused
static void __ext4_trans_abort(struct ext4_mountpoint *mp)
{
	if (mp->fs.jbd_journal && mp->fs.curr_trans) {
		struct jbd_journal *journal = mp->fs.jbd_journal;
		struct jbd_trans *trans = mp->fs.curr_trans;
		jbd_journal_free_trans(journal, trans, true);
		mp->fs.curr_trans = NULL;
	}
}

int ext4_journal_start(const char *mount_point __unused)
{
	int r = EOK;
#if CONFIG_JOURNALING_ENABLE
	r = __ext4_journal_start(mount_point);
#endif
	return r;
}

int ext4_journal_stop(const char *mount_point __unused)
{
	int r = EOK;
#if CONFIG_JOURNALING_ENABLE
	r = __ext4_journal_stop(mount_point);
#endif
	return r;
}

int ext4_recover(const char *mount_point __unused)
{
	int r = EOK;
#if CONFIG_JOURNALING_ENABLE
	r = __ext4_recover(mount_point);
#endif
	return r;
}

static int ext4_trans_start(struct ext4_mountpoint *mp __unused)
{
	int r = EOK;
#if CONFIG_JOURNALING_ENABLE
	r = __ext4_trans_start(mp);
#endif
	return r;
}

static int ext4_trans_stop(struct ext4_mountpoint *mp __unused)
{
	int r = EOK;
#if CONFIG_JOURNALING_ENABLE
	r = __ext4_trans_stop(mp);
#endif
	return r;
}

static void ext4_trans_abort(struct ext4_mountpoint *mp __unused)
{
#if CONFIG_JOURNALING_ENABLE
	__ext4_trans_abort(mp);
#endif
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
		if (!strcmp(s_mp[i].name, mount_point)) {
			mp = &s_mp[i];
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

static int ext4_trunc_dir(struct ext4_mountpoint *mp,
			  struct ext4_inode_ref *parent,
			  struct ext4_inode_ref *dir)
{
	int r = EOK;
	bool is_dir = ext4_inode_is_type(&mp->fs.sb, dir->inode,
			EXT4_INODE_MODE_DIRECTORY);
	uint32_t block_size = ext4_sb_get_block_size(&mp->fs.sb);
	if (!is_dir)
		return EINVAL;

#if CONFIG_DIR_INDEX_ENABLE
	/* Initialize directory index if supported */
	if (ext4_sb_feature_com(&mp->fs.sb, EXT4_FCOM_DIR_INDEX)) {
		r = ext4_dir_dx_init(dir, parent);
		if (r != EOK)
			return r;

		r = ext4_trunc_inode(mp, dir->index,
				     EXT4_DIR_DX_INIT_BCNT * block_size);
		if (r != EOK)
			return r;
	} else
#endif
	{
		r = ext4_trunc_inode(mp, dir->index, block_size);
		if (r != EOK)
			return r;
	}

	return ext4_fs_truncate_inode(dir, 0);
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

	if (fs->read_only && flags & O_CREAT)
		return EROFS;

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

			ext4_fs_inode_blocks_init(fs, &child_ref);

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
	}

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

	return ext4_fs_put_inode_ref(&ref);
}

/****************************************************************************/

static int ext4_generic_open(ext4_file *f, const char *path, const char *flags,
			     bool file_expect, uint32_t *parent_inode,
			     uint32_t *name_off)
{
	uint32_t iflags;
	int filetype;
	int r;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (ext4_parse_flags(flags, &iflags) == false)
		return EINVAL;

	if (file_expect == true)
		filetype = EXT4_DE_REG_FILE;
	else
		filetype = EXT4_DE_DIR;

	if (iflags & O_CREAT)
		ext4_trans_start(mp);

	r = ext4_generic_open2(f, path, iflags, filetype, parent_inode,
				name_off);

	if (iflags & O_CREAT) {
		if (r == EOK)
			ext4_trans_stop(mp);
		else
			ext4_trans_abort(mp);
	}

	return r;
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

	if (mp->fs.read_only)
		return EROFS;

	/* Will that happen? Anyway return EINVAL for such case. */
	if (mp != target_mp)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN,
			       &parent_inode, &name_off);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	child_inode = f.inode;
	ext4_fclose(&f);
	ext4_trans_start(mp);

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

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN,
				&parent_inode, &name_off);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	child_inode = f.inode;
	ext4_fclose(&f);
	ext4_trans_start(mp);

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
	int ret;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ret = ext4_block_cache_write_back(mp->fs.bdev, on);
	EXT4_MP_UNLOCK(mp);
	return ret;
}

int ext4_cache_flush(const char *path)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int ret;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	ret = ext4_block_cache_flush(mp->fs.bdev);
	EXT4_MP_UNLOCK(mp);
	return ret;
}

int ext4_fremove(const char *path)
{
	ext4_file f;
	uint32_t parent_inode;
	uint32_t child_inode;
	uint32_t name_off;
	bool is_goal;
	int r;
	int len;
	struct ext4_inode_ref child;
	struct ext4_inode_ref parent;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN,
			       &parent_inode, &name_off);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	child_inode = f.inode;
	ext4_fclose(&f);
	ext4_trans_start(mp);

	/*Load parent*/
	r = ext4_fs_get_inode_ref(&mp->fs, parent_inode, &parent);
	if (r != EOK) {
		ext4_trans_abort(mp);
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	/*We have file to delete. Load it.*/
	r = ext4_fs_get_inode_ref(&mp->fs, child_inode, &child);
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

int ext4_fopen(ext4_file *file, const char *path, const char *flags)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	ext4_block_cache_write_back(mp->fs.bdev, 1);
	r = ext4_generic_open(file, path, flags, true, 0, 0);
	ext4_block_cache_write_back(mp->fs.bdev, 0);

	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_fopen2(ext4_file *file, const char *path, int flags)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;
	int filetype;

	if (!mp)
		return ENOENT;

        filetype = EXT4_DE_REG_FILE;

	EXT4_MP_LOCK(mp);
	ext4_block_cache_write_back(mp->fs.bdev, 1);

	if (flags & O_CREAT)
		ext4_trans_start(mp);

	r = ext4_generic_open2(file, path, flags, filetype, NULL, NULL);

	if (flags & O_CREAT) {
		if (r == EOK)
			ext4_trans_stop(mp);
		else
			ext4_trans_abort(mp);
	}

	ext4_block_cache_write_back(mp->fs.bdev, 0);
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_fclose(ext4_file *file)
{
	ext4_assert(file && file->mp);

	file->mp = 0;
	file->flags = 0;
	file->inode = 0;
	file->fpos = file->fsize = 0;

	return EOK;
}

static int ext4_ftruncate_no_lock(ext4_file *file, uint64_t size)
{
	struct ext4_inode_ref ref;
	int r;


	r = ext4_fs_get_inode_ref(&file->mp->fs, file->inode, &ref);
	if (r != EOK) {
		EXT4_MP_UNLOCK(file->mp);
		return r;
	}

	/*Sync file size*/
	file->fsize = ext4_inode_get_size(&file->mp->fs.sb, ref.inode);
	if (file->fsize <= size) {
		r = EOK;
		goto Finish;
	}

	/*Start write back cache mode.*/
	r = ext4_block_cache_write_back(file->mp->fs.bdev, 1);
	if (r != EOK)
		goto Finish;

	r = ext4_trunc_inode(file->mp, ref.index, size);
	if (r != EOK)
		goto Finish;

	file->fsize = size;
	if (file->fpos > size)
		file->fpos = size;

	/*Stop write back cache mode*/
	ext4_block_cache_write_back(file->mp->fs.bdev, 0);

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

	if (f->mp->fs.read_only)
		return EROFS;

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

int ext4_fread(ext4_file *file, void *buf, size_t size, size_t *rcnt)
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

	ext4_assert(file && file->mp);

	if (file->flags & O_WRONLY)
		return EPERM;

	if (!size)
		return EOK;

	EXT4_MP_LOCK(file->mp);

	struct ext4_fs *const fs = &file->mp->fs;
	struct ext4_sblock *const sb = &file->mp->fs.sb;

	if (rcnt)
		*rcnt = 0;

	r = ext4_fs_get_inode_ref(fs, file->inode, &ref);
	if (r != EOK) {
		EXT4_MP_UNLOCK(file->mp);
		return r;
	}

	/*Sync file size*/
	file->fsize = ext4_inode_get_size(sb, ref.inode);

	block_size = ext4_sb_get_block_size(sb);
	size = ((uint64_t)size > (file->fsize - file->fpos))
		? ((size_t)(file->fsize - file->fpos)) : size;

	iblock_idx = (uint32_t)((file->fpos) / block_size);
	iblock_last = (uint32_t)((file->fpos + size) / block_size);
	unalg = (file->fpos) % block_size;

	/*If the size of symlink is smaller than 60 bytes*/
	bool softlink;
	softlink = ext4_inode_is_type(sb, ref.inode, EXT4_INODE_MODE_SOFTLINK);
	if (softlink && file->fsize < sizeof(ref.inode->blocks)
		     && !ext4_inode_get_blocks_count(sb, ref.inode)) {

		char *content = (char *)ref.inode->blocks;
		if (file->fpos < file->fsize) {
			size_t len = size;
			if (unalg + size > (uint32_t)file->fsize)
				len = (uint32_t)file->fsize - unalg;
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
			r = ext4_block_readbytes(file->mp->fs.bdev, off, u8_buf, len);
			if (r != EOK)
				goto Finish;

		} else {
			/* Yes, we do. */
			memset(u8_buf, 0, len);
		}

		u8_buf += len;
		size -= len;
		file->fpos += len;

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

		r = ext4_blocks_get_direct(file->mp->fs.bdev, u8_buf, fblock_start,
					   fblock_count);
		if (r != EOK)
			goto Finish;

		size -= block_size * fblock_count;
		u8_buf += block_size * fblock_count;
		file->fpos += block_size * fblock_count;

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
		r = ext4_block_readbytes(file->mp->fs.bdev, off, u8_buf, size);
		if (r != EOK)
			goto Finish;

		file->fpos += size;

		if (rcnt)
			*rcnt += size;
	}

Finish:
	ext4_fs_put_inode_ref(&ref);
	EXT4_MP_UNLOCK(file->mp);
	return r;
}

int ext4_fwrite(ext4_file *file, const void *buf, size_t size, size_t *wcnt)
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

	ext4_assert(file && file->mp);

	if (file->mp->fs.read_only)
		return EROFS;

	if (file->flags & O_RDONLY)
		return EPERM;

	if (!size)
		return EOK;

	EXT4_MP_LOCK(file->mp);
	ext4_trans_start(file->mp);

	struct ext4_fs *const fs = &file->mp->fs;
	struct ext4_sblock *const sb = &file->mp->fs.sb;

	if (wcnt)
		*wcnt = 0;

	r = ext4_fs_get_inode_ref(fs, file->inode, &ref);
	if (r != EOK) {
		ext4_trans_abort(file->mp);
		EXT4_MP_UNLOCK(file->mp);
		return r;
	}

	/*Sync file size*/
	file->fsize = ext4_inode_get_size(sb, ref.inode);
	block_size = ext4_sb_get_block_size(sb);

	iblock_last = (uint32_t)((file->fpos + size) / block_size);
	iblk_idx = (uint32_t)(file->fpos / block_size);
	ifile_blocks = (uint32_t)((file->fsize + block_size - 1) / block_size);

	unalg = (file->fpos) % block_size;

	if (unalg) {
		size_t len =  size;
		uint64_t off;
		if (size > (block_size - unalg))
			len = block_size - unalg;

		r = ext4_fs_init_inode_dblk_idx(&ref, iblk_idx, &fblk);
		if (r != EOK)
			goto Finish;

		off = fblk * block_size + unalg;
		r = ext4_block_writebytes(file->mp->fs.bdev, off, u8_buf, len);
		if (r != EOK)
			goto Finish;

		u8_buf += len;
		size -= len;
		file->fpos += len;

		if (wcnt)
			*wcnt += len;

		iblk_idx++;
	}

	/*Start write back cache mode.*/
	r = ext4_block_cache_write_back(file->mp->fs.bdev, 1);
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

		r = ext4_blocks_set_direct(file->mp->fs.bdev, u8_buf, fblock_start,
					   fblock_count);
		if (r != EOK)
			break;

		size -= block_size * fblock_count;
		u8_buf += block_size * fblock_count;
		file->fpos += block_size * fblock_count;

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
	ext4_block_cache_write_back(file->mp->fs.bdev, 0);

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
		r = ext4_block_writebytes(file->mp->fs.bdev, off, u8_buf, size);
		if (r != EOK)
			goto Finish;

		file->fpos += size;

		if (wcnt)
			*wcnt += size;
	}

out_fsize:
	if (file->fpos > file->fsize) {
		file->fsize = file->fpos;
		ext4_inode_set_size(ref.inode, file->fsize);
		ref.dirty = true;
	}

Finish:
	r = ext4_fs_put_inode_ref(&ref);

	if (r != EOK)
		ext4_trans_abort(file->mp);
	else
		ext4_trans_stop(file->mp);

	EXT4_MP_UNLOCK(file->mp);
	return r;
}

int ext4_fseek(ext4_file *file, int64_t offset, uint32_t origin)
{
	switch (origin) {
	case SEEK_SET:
		if (offset < 0 || (uint64_t)offset > file->fsize)
			return EINVAL;

		file->fpos = offset;
		return EOK;
	case SEEK_CUR:
		if ((offset < 0 && (uint64_t)(-offset) > file->fpos) ||
		    (offset > 0 &&
		     (uint64_t)offset > (file->fsize - file->fpos)))
			return EINVAL;

		file->fpos += offset;
		return EOK;
	case SEEK_END:
		if (offset < 0 || (uint64_t)offset > file->fsize)
			return EINVAL;

		file->fpos = file->fsize - offset;
		return EOK;
	}
	return EINVAL;
}

uint64_t ext4_ftell(ext4_file *file)
{
	return file->fpos;
}

uint64_t ext4_fsize(ext4_file *file)
{
	return file->fsize;
}


static int ext4_trans_get_inode_ref(const char *path,
				    struct ext4_mountpoint *mp,
				    struct ext4_inode_ref *inode_ref)
{
	int r;
	ext4_file f;

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		return r;

	ext4_trans_start(mp);

	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, inode_ref);
	if (r != EOK) {
		ext4_trans_abort(mp);
		return r;
	}

	return r;
}

static int ext4_trans_put_inode_ref(struct ext4_mountpoint *mp,
				    struct ext4_inode_ref *inode_ref)
{
	int r;

	r = ext4_fs_put_inode_ref(inode_ref);
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	return r;
}


int ext4_raw_inode_fill(const char *path, uint32_t *ret_ino,
			struct ext4_inode *inode)
{
	int r;
	ext4_file f;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	/*Load parent*/
	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, &inode_ref);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	if (ret_ino)
		*ret_ino = f.inode;

	memcpy(inode, inode_ref.inode, sizeof(struct ext4_inode));
	ext4_fs_put_inode_ref(&inode_ref);
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_inode_exist(const char *path, int type)
{
	int r;
	ext4_file f;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDONLY, type, NULL, NULL);
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_mode_set(const char *path, uint32_t mode)
{
	int r;
	uint32_t orig_mode;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	r = ext4_trans_get_inode_ref(path, mp, &inode_ref);
	if (r != EOK)
		goto Finish;

	orig_mode = ext4_inode_get_mode(&mp->fs.sb, inode_ref.inode);
	orig_mode &= ~0xFFF;
	orig_mode |= mode & 0xFFF;
	ext4_inode_set_mode(&mp->fs.sb, inode_ref.inode, orig_mode);

	inode_ref.dirty = true;
	r = ext4_trans_put_inode_ref(mp, &inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_owner_set(const char *path, uint32_t uid, uint32_t gid)
{
	int r;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	r = ext4_trans_get_inode_ref(path, mp, &inode_ref);
	if (r != EOK)
		goto Finish;

	ext4_inode_set_uid(inode_ref.inode, uid);
	ext4_inode_set_gid(inode_ref.inode, gid);

	inode_ref.dirty = true;
	r = ext4_trans_put_inode_ref(mp, &inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_mode_get(const char *path, uint32_t *mode)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	ext4_file f;
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	*mode = ext4_inode_get_mode(&mp->fs.sb, inode_ref.inode);
	r = ext4_fs_put_inode_ref(&inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_owner_get(const char *path, uint32_t *uid, uint32_t *gid)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	ext4_file f;
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	*uid = ext4_inode_get_uid(inode_ref.inode);
	*gid = ext4_inode_get_gid(inode_ref.inode);
	r = ext4_fs_put_inode_ref(&inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_atime_set(const char *path, uint32_t atime)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	r = ext4_trans_get_inode_ref(path, mp, &inode_ref);
	if (r != EOK)
		goto Finish;

	ext4_inode_set_access_time(inode_ref.inode, atime);
	inode_ref.dirty = true;
	r = ext4_trans_put_inode_ref(mp, &inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_mtime_set(const char *path, uint32_t mtime)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	r = ext4_trans_get_inode_ref(path, mp, &inode_ref);
	if (r != EOK)
		goto Finish;

	ext4_inode_set_modif_time(inode_ref.inode, mtime);
	inode_ref.dirty = true;
	r = ext4_trans_put_inode_ref(mp, &inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_ctime_set(const char *path, uint32_t ctime)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	r = ext4_trans_get_inode_ref(path, mp, &inode_ref);
	if (r != EOK)
		goto Finish;

	ext4_inode_set_change_inode_time(inode_ref.inode, ctime);
	inode_ref.dirty = true;
	r = ext4_trans_put_inode_ref(mp, &inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_atime_get(const char *path, uint32_t *atime)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	ext4_file f;
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	*atime = ext4_inode_get_access_time(inode_ref.inode);
	r = ext4_fs_put_inode_ref(&inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_mtime_get(const char *path, uint32_t *mtime)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	ext4_file f;
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	*mtime = ext4_inode_get_modif_time(inode_ref.inode);
	r = ext4_fs_put_inode_ref(&inode_ref);

	Finish:
	EXT4_MP_UNLOCK(mp);

	return r;
}

int ext4_ctime_get(const char *path, uint32_t *ctime)
{
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	ext4_file f;
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);

	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;

	r = ext4_fs_get_inode_ref(&mp->fs, f.inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	*ctime = ext4_inode_get_change_inode_time(inode_ref.inode);
	r = ext4_fs_put_inode_ref(&inode_ref);

	Finish:
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
		uint64_t off;
		ext4_fs_inode_blocks_init(&f->mp->fs, &ref);
		r = ext4_fs_append_inode_dblk(&ref, &fblock, &sblock);
		if (r != EOK)
			goto Finish;

		off = fblock * block_size;
		r = ext4_block_writebytes(f->mp->fs.bdev, off, buf, size);
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

	if (mp->fs.read_only)
		return EROFS;

	filetype = EXT4_DE_SYMLINK;

	EXT4_MP_LOCK(mp);
	ext4_block_cache_write_back(mp->fs.bdev, 1);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR | O_CREAT, filetype, NULL, NULL);
	if (r == EOK)
		r = ext4_fsymlink_set(&f, target, strlen(target));
	else
		goto Finish;

	ext4_fclose(&f);

Finish:
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	ext4_block_cache_write_back(mp->fs.bdev, 0);
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
	EXT4_MP_UNLOCK(mp);
	return r;
}

static int ext4_mknod_set(ext4_file *f, uint32_t dev)
{
	struct ext4_inode_ref ref;
	int r;

	ext4_assert(f && f->mp);

	r = ext4_fs_get_inode_ref(&f->mp->fs, f->inode, &ref);
	if (r != EOK)
		return r;

	ext4_inode_set_dev(ref.inode, dev);

	ext4_inode_set_size(ref.inode, 0);
	ref.dirty = true;

	f->fsize = 0;
	f->fpos = 0;

	r = ext4_fs_put_inode_ref(&ref);
	return r;
}

int ext4_mknod(const char *path, int filetype, uint32_t dev)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;
	ext4_file f;

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	/*
	 * The filetype shouldn't be normal file, directory or
	 * unknown.
	 */
	if (filetype == EXT4_DE_UNKNOWN ||
	    filetype == EXT4_DE_REG_FILE ||
	    filetype == EXT4_DE_DIR ||
	    filetype == EXT4_DE_SYMLINK)
		return EINVAL;

	/*
	 * Nor should it be any bogus value.
	 */
	if (filetype != EXT4_DE_CHRDEV &&
	    filetype != EXT4_DE_BLKDEV &&
	    filetype != EXT4_DE_FIFO &&
	    filetype != EXT4_DE_SOCK)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	ext4_block_cache_write_back(mp->fs.bdev, 1);
	ext4_trans_start(mp);

	r = ext4_generic_open2(&f, path, O_RDWR | O_CREAT, filetype, NULL, NULL);
	if (r == EOK) {
		if (filetype == EXT4_DE_CHRDEV ||
		    filetype == EXT4_DE_BLKDEV)
			r = ext4_mknod_set(&f, dev);
	} else {
		goto Finish;
	}

	ext4_fclose(&f);

Finish:
	if (r != EOK)
		ext4_trans_abort(mp);
	else
		ext4_trans_stop(mp);

	ext4_block_cache_write_back(mp->fs.bdev, 0);
	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_setxattr(const char *path, const char *name, size_t name_len,
		  const void *data, size_t data_size)
{
	bool found;
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	uint8_t name_index;
	const char *dissected_name = NULL;
	size_t dissected_len = 0;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	dissected_name = ext4_extract_xattr_name(name, name_len,
				&name_index, &dissected_len,
				&found);
	if (!found)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		EXT4_MP_UNLOCK(mp);
		return r;
	}

	inode = f.inode;
	ext4_fclose(&f);
	ext4_trans_start(mp);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_xattr_set(&inode_ref, name_index, dissected_name,
			dissected_len, data, data_size);

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
	bool found;
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	uint8_t name_index;
	const char *dissected_name = NULL;
	size_t dissected_len = 0;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	dissected_name = ext4_extract_xattr_name(name, name_len,
				&name_index, &dissected_len,
				&found);
	if (!found)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;

	inode = f.inode;
	ext4_fclose(&f);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_xattr_get(&inode_ref, name_index, dissected_name,
				dissected_len, buf, buf_size, data_size);

	ext4_fs_put_inode_ref(&inode_ref);
Finish:
	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_listxattr(const char *path, char *list, size_t size, size_t *ret_size)
{
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	size_t list_len, list_size = 0;
	struct ext4_inode_ref inode_ref;
	struct ext4_xattr_list_entry *xattr_list = NULL,
				     *entry = NULL;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK)
		goto Finish;
	inode = f.inode;
	ext4_fclose(&f);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_xattr_list(&inode_ref, NULL, &list_len);
	if (r == EOK && list_len) {
		xattr_list = ext4_malloc(list_len);
		if (!xattr_list) {
			ext4_fs_put_inode_ref(&inode_ref);
			r = ENOMEM;
			goto Finish;
		}
		entry = xattr_list;
		r = ext4_xattr_list(&inode_ref, entry, &list_len);
		if (r != EOK) {
			ext4_fs_put_inode_ref(&inode_ref);
			goto Finish;
		}

		for (;entry;entry = entry->next) {
			size_t prefix_len;
			const char *prefix =
				ext4_get_xattr_name_prefix(entry->name_index,
							   &prefix_len);
			if (size) {
				if (prefix_len + entry->name_len + 1 > size) {
					ext4_fs_put_inode_ref(&inode_ref);
					r = ERANGE;
					goto Finish;
				}
			}

			if (list && size) {
				memcpy(list, prefix, prefix_len);
				list += prefix_len;
				memcpy(list, entry->name,
					entry->name_len);
				list[entry->name_len] = 0;
				list += entry->name_len + 1;

				size -= prefix_len + entry->name_len + 1;
			}

			list_size += prefix_len + entry->name_len + 1;
		}
		if (ret_size)
			*ret_size = list_size;

	}
	ext4_fs_put_inode_ref(&inode_ref);
Finish:
	EXT4_MP_UNLOCK(mp);
	if (xattr_list)
		ext4_free(xattr_list);

	return r;

}

int ext4_removexattr(const char *path, const char *name, size_t name_len)
{
	bool found;
	int r = EOK;
	ext4_file f;
	uint32_t inode;
	uint8_t name_index;
	const char *dissected_name = NULL;
	size_t dissected_len = 0;
	struct ext4_inode_ref inode_ref;
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	dissected_name = ext4_extract_xattr_name(name, name_len,
						&name_index, &dissected_len,
						&found);
	if (!found)
		return EINVAL;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, NULL, NULL);
	if (r != EOK) {
		EXT4_MP_LOCK(mp);
		return r;
	}

	inode = f.inode;
	ext4_fclose(&f);
	ext4_trans_start(mp);

	r = ext4_fs_get_inode_ref(&mp->fs, inode, &inode_ref);
	if (r != EOK)
		goto Finish;

	r = ext4_xattr_remove(&inode_ref, name_index, dissected_name,
			      dissected_len);

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

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	struct ext4_fs *const fs = &mp->fs;

	/*Check if exist.*/
	r = ext4_generic_open(&f, path, "r", false, &inode_up, &name_off);
	if (r != EOK) {
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
				uint32_t inode_type;
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
				inode_type = ext4_inode_type(&mp->fs.sb,
						child.inode);

				/* Truncate */
				if (inode_type != EXT4_INODE_MODE_DIRECTORY)
					r = ext4_trunc_inode(mp, child.index, 0);
				else
					r = ext4_trunc_dir(mp, &act, &child);

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
			/*Truncate*/
			r = ext4_trunc_dir(mp, &parent, &act);
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

int ext4_dir_mv(const char *path, const char *new_path)
{
	return ext4_frename(path, new_path);
}

int ext4_dir_mk(const char *path)
{
	int r;
	ext4_file f;
	struct ext4_mountpoint *mp = ext4_get_mount(path);

	if (!mp)
		return ENOENT;

	if (mp->fs.read_only)
		return EROFS;

	EXT4_MP_LOCK(mp);

	/*Check if exist.*/
	r = ext4_generic_open(&f, path, "r", false, 0, 0);
	if (r == EOK)
		goto Finish;

	/*Create new directory.*/
	r = ext4_generic_open(&f, path, "w", false, 0, 0);

Finish:
	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_dir_open(ext4_dir *dir, const char *path)
{
	struct ext4_mountpoint *mp = ext4_get_mount(path);
	int r;

	if (!mp)
		return ENOENT;

	EXT4_MP_LOCK(mp);
	r = ext4_generic_open(&dir->f, path, "r", false, 0, 0);
	dir->next_off = 0;
	EXT4_MP_UNLOCK(mp);
	return r;
}

int ext4_dir_close(ext4_dir *dir)
{
    return ext4_fclose(&dir->f);
}

const ext4_direntry *ext4_dir_entry_next(ext4_dir *dir)
{
#define EXT4_DIR_ENTRY_OFFSET_TERM (uint64_t)(-1)

	int r;
	uint16_t name_length;
	ext4_direntry *de = 0;
	struct ext4_inode_ref dir_inode;
	struct ext4_dir_iter it;

	EXT4_MP_LOCK(dir->f.mp);

	if (dir->next_off == EXT4_DIR_ENTRY_OFFSET_TERM) {
		EXT4_MP_UNLOCK(dir->f.mp);
		return 0;
	}

	r = ext4_fs_get_inode_ref(&dir->f.mp->fs, dir->f.inode, &dir_inode);
	if (r != EOK) {
		goto Finish;
	}

	r = ext4_dir_iterator_init(&it, &dir_inode, dir->next_off);
	if (r != EOK) {
		ext4_fs_put_inode_ref(&dir_inode);
		goto Finish;
	}

	memset(&dir->de.name, 0, sizeof(dir->de.name));
	name_length = ext4_dir_en_get_name_len(&dir->f.mp->fs.sb,
					       it.curr);
	memcpy(&dir->de.name, it.curr->name, name_length);

	/* Directly copying the content isn't safe for Big-endian targets*/
	dir->de.inode = ext4_dir_en_get_inode(it.curr);
	dir->de.entry_length = ext4_dir_en_get_entry_len(it.curr);
	dir->de.name_length = name_length;
	dir->de.inode_type = ext4_dir_en_get_inode_type(&dir->f.mp->fs.sb,
						      it.curr);

	de = &dir->de;

	ext4_dir_iterator_next(&it);

	dir->next_off = it.curr ? it.curr_off : EXT4_DIR_ENTRY_OFFSET_TERM;

	ext4_dir_iterator_fini(&it);
	ext4_fs_put_inode_ref(&dir_inode);

Finish:
	EXT4_MP_UNLOCK(dir->f.mp);
	return de;
}

void ext4_dir_entry_rewind(ext4_dir *dir)
{
	dir->next_off = 0;
}

/**
 * @}
 */
