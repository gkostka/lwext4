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
#include <ext4_blockdev.h>
#include <ext4_types.h>
#include <ext4_debug.h>
#include <ext4_errno.h>
#include <ext4_fs.h>
#include <ext4_dir.h>
#include <ext4_inode.h>
#include <ext4_super.h>
#include <ext4_dir_idx.h>

#include <stdlib.h>
#include <string.h>

#include <ext4.h>

/**@brief	Mount point OS dependent lock*/
#define EXT4_MP_LOCK(_m)    \
        do { (_m)->os_locks ? (_m)->os_locks->lock()   : 0; }while(0)

/**@brief	Mount point OS dependent unlock*/
#define EXT4_MP_UNLOCK(_m)  \
        do { (_m)->os_locks ? (_m)->os_locks->unlock() : 0;	}while(0)

/**@brief	Mount point descrpitor.*/
struct ext4_mountpoint {

    /**@brief	Mount point name (@ref ext4_mount)*/
    const char 		 *name;

    /**@brief	Os dependent lock/unlock functions.*/
    struct ext4_lock *os_locks;

    /**@brief	Ext4 filesystem internals.*/
    struct ext4_fs	 fs;

    /**@brief	Dynamic alocation cache flag.*/
    bool			cache_dynamic;
};

/**@brief	Block devices descriptor.*/
struct	_ext4_devices {

    /**@brief	Block device name (@ref ext4_device_register)*/
    const char				*name;

    /**@brief	Block device handle.*/
    struct ext4_blockdev	*bd;

    /**@brief	Block cache handle.*/
    struct ext4_bcache		*bc;
};

/**@brief	Block devices.*/
struct	_ext4_devices _bdevices[CONFIG_EXT4_BLOCKDEVS_COUNT];


/**@brief	Mountpoints.*/
struct ext4_mountpoint	_mp[CONFIG_EXT4_MOUNTPOINTS_COUNT];



int	ext4_device_register(struct ext4_blockdev *bd, struct ext4_bcache *bc,
        const char *dev_name)
{
    uint32_t i;
    ext4_assert(bd && dev_name);

    for (i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
        if(!_bdevices[i].name){
            _bdevices[i].name   = dev_name;
            _bdevices[i].bd 	= bd;
            _bdevices[i].bc 	= bc;
            return EOK;
        }
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

    struct ext4_fs *fs = enode->fs;

    /* Check if node is directory */
    if (!ext4_inode_is_type(&fs->sb, enode->inode,
            EXT4_INODE_MODE_DIRECTORY)) {
        *has_children = false;
        return EOK;
    }

    struct ext4_directory_iterator it;
    int rc = ext4_dir_iterator_init(&it, enode, 0);
    if (rc != EOK)
        return rc;

    /* Find a non-empty directory entry */
    bool found = false;
    while (it.current != NULL) {
        if (it.current->inode != 0) {
            uint16_t name_size =
                    ext4_dir_entry_ll_get_name_length(&fs->sb,
                            it.current);
            if (!ext4_is_dots(it.current->name, name_size)) {
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
        struct ext4_inode_ref *child, const char *name, uint32_t name_len)
{
    /* Check maximum name length */
    if(name_len > EXT4_DIRECTORY_FILENAME_LEN)
        return EINVAL;

    /* Add entry to parent directory */
    int rc = ext4_dir_add_entry(parent, name, name_len,
            child);
    if (rc != EOK)
        return rc;

    /* Fill new dir -> add '.' and '..' entries */
    if (ext4_inode_is_type(&mp->fs.sb, child->inode,
            EXT4_INODE_MODE_DIRECTORY)) {
        rc = ext4_dir_add_entry(child, ".", strlen("."),
                child);
        if (rc != EOK) {
            ext4_dir_remove_entry(parent, name, strlen(name));
            return rc;
        }

        rc = ext4_dir_add_entry(child, "..", strlen(".."),
                parent);
        if (rc != EOK) {
            ext4_dir_remove_entry(parent, name, strlen(name));
            ext4_dir_remove_entry(child, ".", strlen("."));
            return rc;
        }

#if CONFIG_DIR_INDEX_ENABLE
        /* Initialize directory index if supported */
        if (ext4_sb_check_feature_compatible(&mp->fs.sb,
                EXT4_FEATURE_COMPAT_DIR_INDEX)) {
            rc = ext4_dir_dx_init(child);
            if (rc != EOK)
                return rc;

            ext4_inode_set_flag(child->inode,
                    EXT4_INODE_FLAG_INDEX);
            child->dirty = true;
        }
#endif

        uint16_t parent_links =
                ext4_inode_get_links_count(parent->inode);
        parent_links++;
        ext4_inode_set_links_count(parent->inode, parent_links);

        parent->dirty = true;
    }

    uint16_t child_links =
            ext4_inode_get_links_count(child->inode);
    child_links++;
    ext4_inode_set_links_count(child->inode, child_links);

    child->dirty = true;

    return EOK;
}

static int ext4_unlink(struct ext4_mountpoint *mp,
    struct ext4_inode_ref *parent, struct ext4_inode_ref *child_inode_ref,
    const char *name, uint32_t name_len)
{
    bool has_children;
    int rc = ext4_has_children(&has_children, child_inode_ref);
    if (rc != EOK)
        return rc;

    /* Cannot unlink non-empty node */
    if (has_children)
        return ENOTSUP;

    /* Remove entry from parent directory */

    rc = ext4_dir_remove_entry(parent, name, name_len);
    if (rc != EOK)
        return rc;


    uint32_t lnk_count =
            ext4_inode_get_links_count(child_inode_ref->inode);
    lnk_count--;

    bool is_dir = ext4_inode_is_type(&mp->fs.sb, child_inode_ref->inode,
            EXT4_INODE_MODE_DIRECTORY);

    /* If directory - handle links from parent */
    if ((lnk_count <= 1) && (is_dir)) {
        ext4_assert(lnk_count == 1);

        lnk_count--;

        uint32_t parent_lnk_count = ext4_inode_get_links_count(
                parent->inode);

        parent_lnk_count--;
        ext4_inode_set_links_count(parent->inode, parent_lnk_count);

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
     * ext4_inode_set_change_inode_time(child_inode_ref->inode,
     *     (uint32_t) now);
     */

    ext4_inode_set_deletion_time(child_inode_ref->inode, 1);
    ext4_inode_set_links_count(child_inode_ref->inode, lnk_count);
    child_inode_ref->dirty = true;

    return EOK;
}

/****************************************************************************/

int			ext4_mount(const char * dev_name,  char *mount_point)
{
    ext4_assert(mount_point && dev_name);
    int r = EOK;
    int i;

    uint32_t bsize;
    struct ext4_blockdev	*bd = 0;
    struct ext4_bcache		*bc = 0;
    struct ext4_mountpoint	*mp = 0;

    for (i = 0; i < CONFIG_EXT4_BLOCKDEVS_COUNT; ++i) {
        if(_bdevices[i].name){
            if(!strcmp(dev_name, _bdevices[i].name)){
                bd = _bdevices[i].bd;
                bc = _bdevices[i].bc;
                break;
            }
        }
    }

    if(!bd)
        return ENODEV;

    for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
        if(!_mp[i].name){
            _mp[i].name = mount_point;
            mp = &_mp[i];
            break;
        }
    }

    if(!mp)
        return ENOMEM;

    r = ext4_block_init(bd);
    if(r != EOK)
        return r;

    r = ext4_fs_init(&mp->fs, bd);
    if(r != EOK){
        ext4_block_fini(bd);
        return r;
    }

    bsize = ext4_sb_get_block_size(&mp->fs.sb);
    ext4_block_set_lb_size(bd, bsize);


    mp->cache_dynamic = 0;

    if(!bc){
        /*Automatic block cache alloc.*/
        mp->cache_dynamic = 1;
        bc = malloc(sizeof(struct ext4_bcache));

        r = ext4_bcache_init_dynamic(bc, 8, bsize);
        if(r != EOK){
            free(bc);
            ext4_block_fini(bd);
            return r;
        }
    }

    if(bsize != bc->itemsize)
        return ENOTSUP;


    /*Bind block cache to block device*/
    r = ext4_block_bind_bcache(bd, bc);
    if(r != EOK){
        ext4_block_fini(bd);
        if(mp->cache_dynamic){
            ext4_bcache_fini_dynamic(bc);
            free(bc);
        }
        return r;
    }

    return r;
}


int			ext4_umount(char *mount_point)
{
    int 	i;
    int 	r = EOK;
    struct ext4_mountpoint	*mp = 0;

    for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
        if(_mp[i].name){
            if(!strcmp(_mp[i].name, mount_point))
                mp = &_mp[i];
            break;
        }
    }

    if(!mp)
        return ENODEV;

    r = ext4_fs_fini(&mp->fs);
    if(r != EOK)
        return r;

    mp->name = 0;

    if(mp->cache_dynamic){
        ext4_bcache_fini_dynamic(mp->fs.bdev->bc);
        free(mp->fs.bdev->bc);
    }

    return ext4_block_fini(mp->fs.bdev);
}


/********************************FILE OPERATIONS*****************************/

static struct ext4_mountpoint*  ext4_get_mount(const char *path)
{
    int i;
    for (i = 0; i < CONFIG_EXT4_MOUNTPOINTS_COUNT; ++i) {
        if(_mp[i].name){
            if(!strncmp(_mp[i].name, path, strlen(_mp[i].name)))
                return &_mp[i];
        }
    }
    return 0;
}


static int ext4_path_check(const char *path, bool* is_goal)
{
    int i;

    for (i = 0; i < EXT4_DIRECTORY_FILENAME_LEN; ++i) {

        if(path[i] == '/'){
            *is_goal = false;
            return i;
        }

        if(path[i] == 0){
            *is_goal = true;
            return i;
        }
    }

    return 0;
}


static bool ext4_parse_flags(const char *flags, uint32_t *file_flags)
{
    if(!flags)
        return false;

    if(!strcmp(flags, "r") || !strcmp(flags, "rb")){
        *file_flags = O_RDONLY;
        return true;
    }

    if(!strcmp(flags, "w") || !strcmp(flags, "wb")){
        *file_flags = O_WRONLY | O_CREAT | O_TRUNC;
        return true;
    }

    if(!strcmp(flags, "a") || !strcmp(flags, "ab")){
        *file_flags = O_WRONLY | O_CREAT | O_APPEND	;
        return true;
    }

    if(!strcmp(flags, "r+") || !strcmp(flags, "rb+") || !strcmp(flags, "r+b")){
        *file_flags = O_RDWR;
        return true;
    }

    if(!strcmp(flags, "w+") || !strcmp(flags, "wb+") || !strcmp(flags, "w+b")){
        *file_flags = O_RDWR | O_CREAT | O_TRUNC;
        return true;
    }

    if(!strcmp(flags, "a+") || !strcmp(flags, "ab+") || !strcmp(flags, "a+b")){
        *file_flags = O_RDWR | O_CREAT | O_APPEND;
        return true;
    }

    return false;
}

/****************************************************************************/

static int ext4_generic_open (ext4_file *f, const char *path,
        const char *flags, bool file_expect)
{
    struct ext4_mountpoint *mp = ext4_get_mount(path);
    struct ext4_directory_search_result result;
    struct ext4_inode_ref	ref;
    bool	is_goal = false;
    uint8_t	inode_type;
    int		r = ENOENT;
    uint32_t next_inode;

    f->mp = 0;

    if(!mp)
        return ENOENT;

    if(ext4_parse_flags(flags, &f->flags) == false)
        return EINVAL;

    /*Skip mount point*/
    path += strlen(mp->name);

    /*Skip first /*/
    if(*path != '/')
        return ENOENT;

    path++;

    EXT4_MP_LOCK(mp);

    /*Load root*/
    r = ext4_fs_get_inode_ref(&mp->fs, EXT4_INODE_ROOT_INDEX, &ref);

    if(r != EOK)
        return r;

    int len = ext4_path_check(path, &is_goal);

    /*If root open was request.*/
    if(!len && is_goal)
        goto IsGoal;

    while(1){

        len = ext4_path_check(path, &is_goal);

        if(!len){
            r = ENOENT;
            break;
        }

        r = ext4_dir_find_entry(&result, &ref, path, len);
        if(r != EOK){

            if(r != ENOENT)
                break;

            if(!(f->flags & O_CREAT))
                break;

            /*O_CREAT allows to create new entry*/
            struct ext4_inode_ref	child_ref;
            r = ext4_fs_alloc_inode(&mp->fs, &child_ref, !is_goal);
            if(r != EOK)
                break;

            /*Destroy last result*/
            ext4_dir_destroy_result(&ref, &result);

            /*Link with root dir.*/
            r = ext4_link(mp, &ref, &child_ref, path, len);
            if(r != EOK){
                /*Fali. Free new inode.*/
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

        next_inode = result.dentry->inode;
        inode_type = ext4_dir_entry_ll_get_inode_type(&mp->fs.sb, result.dentry);

        r = ext4_dir_destroy_result(&ref, &result);
        if(r != EOK)
            break;

        /*If expected file error*/
        if((inode_type == EXT4_DIRECTORY_FILETYPE_REG_FILE)
                && !file_expect && is_goal){
            r = ENOENT;
            break;
        }

        /*If expected directory error*/
        if((inode_type == EXT4_DIRECTORY_FILETYPE_DIR)
                && file_expect && is_goal){
            r = ENOENT;
            break;
        }

        r = ext4_fs_put_inode_ref(&ref);
        if(r != EOK)
            break;

        r = ext4_fs_get_inode_ref(&mp->fs, next_inode, &ref);
        if(r != EOK)
            break;

        if(is_goal)
            break;

        path += len + 1;
    };

    if(r != EOK){
        ext4_fs_put_inode_ref(&ref);
        EXT4_MP_UNLOCK(mp);
        return r;
    }

    IsGoal:
    if(is_goal){

        if(f->flags & O_TRUNC){
            /*Turncate.*/
            ext4_block_delay_cache_flush(mp->fs.bdev, 1);
            /*Truncate may be IO heavy.
             Do it with delayed cache flush mode.*/
            r = ext4_fs_truncate_inode(&ref, 0);
            ext4_block_delay_cache_flush(mp->fs.bdev, 0);

            if(r != EOK){
                ext4_fs_put_inode_ref(&ref);
                EXT4_MP_UNLOCK(mp);
                return r;
            }
        }

        f->mp = mp;
        f->fsize = ext4_inode_get_size(&f->mp->fs.sb, ref.inode);
        f->inode = ref.index;
        f->fpos  = 0;

        if(f->flags & O_APPEND)
            f->fpos = f->fsize;

    }

    r = ext4_fs_put_inode_ref(&ref);
    EXT4_MP_UNLOCK(mp);
    return r;
}

/****************************************************************************/

int ext4_fremove(const char *path)
{
    struct ext4_mountpoint *mp = ext4_get_mount(path);
    struct ext4_directory_search_result result;
    bool	is_goal = false;
    uint8_t	inode_type;
    int		r = ENOENT;
    uint32_t next_inode;

    struct ext4_inode_ref	parent;
    struct ext4_inode_ref	child;
    int len = 0;

    if(!mp)
        return ENOENT;


    /*Skip mount point*/
    path += strlen(mp->name);

    /*Skip first /*/
    if(*path != '/')
        return ENOENT;

    path++;

    /*Lock mountpoint*/
    EXT4_MP_LOCK(mp);

    /*Load root*/
    r = ext4_fs_get_inode_ref(&mp->fs, EXT4_INODE_ROOT_INDEX, &parent);

    if(r != EOK){
        EXT4_MP_UNLOCK(mp);
        return r;
    }

    while(1){

        len = ext4_path_check(path, &is_goal);

        if(!len){
            r = ENOENT;
            break;
        }

        r = ext4_dir_find_entry(&result, &parent, path, len);
        if(r != EOK){
            ext4_dir_destroy_result(&parent, &result);
            break;
        }

        next_inode = result.dentry->inode;
        inode_type = ext4_dir_entry_ll_get_inode_type(&mp->fs.sb,
                result.dentry);

        r = ext4_dir_destroy_result(&parent, &result);
        if(r != EOK)
            break;

        /*If expected file error*/
        if((inode_type == EXT4_DIRECTORY_FILETYPE_REG_FILE) && !is_goal){
            r = ENOENT;
            break;
        }

        /*If expected directory error*/
        if((inode_type == EXT4_DIRECTORY_FILETYPE_DIR) && is_goal){
            r = ENOENT;
            break;
        }

        if(is_goal)
            break;

        r = ext4_fs_put_inode_ref(&parent);
        if(r != EOK)
            break;


        r = ext4_fs_get_inode_ref(&mp->fs, next_inode, &parent);
        if(r != EOK)
            break;

        path += len + 1;
    };

    if(r != EOK){
        /*No entry or other error.*/
        ext4_fs_put_inode_ref(&parent);
        EXT4_MP_UNLOCK(mp);
        return r;
    }


    /*We have file to delete. Load it.*/
    r = ext4_fs_get_inode_ref(&mp->fs, next_inode, &child);
    if(r != EOK){
        /*Parent free*/
        ext4_fs_put_inode_ref(&parent);
        EXT4_MP_UNLOCK(mp);
        return r;
    }

    /*Turncate.*/
    ext4_block_delay_cache_flush(mp->fs.bdev, 1);

    /*Truncate may be IO heavy. Do it with delayed cache flush mode.*/
    r = ext4_fs_truncate_inode(&child, 0);

    ext4_block_delay_cache_flush(mp->fs.bdev, 0);
    if(r != EOK)
        goto Finish;

    /*Unlink from parent.*/
    r = ext4_unlink(mp, &parent, &child, path, len);
    if(r != EOK)
        goto Finish;

    r = ext4_fs_free_inode(&child);
    if(r != EOK)
        goto Finish;


    Finish:
    ext4_fs_put_inode_ref(&child);
    ext4_fs_put_inode_ref(&parent);
    EXT4_MP_UNLOCK(mp);
    return r;
}


int	ext4_fopen (ext4_file *f, const char *path, const char *flags)
{
    return ext4_generic_open(f, path, flags, true);
}

int	ext4_fclose(ext4_file *f)
{
    ext4_assert(f && f->mp);

    f->mp    = 0;
    f->flags = 0;
    f->inode = 0;
    f->fpos  = f->fsize = 0;

    return EOK;
}
int	ext4_fread (ext4_file *f, void *buf, uint32_t size, uint32_t *rcnt)
{
    int		 r = EOK;
    uint32_t u;
    uint32_t fblock;
    struct ext4_block b;
    uint8_t	*u8_buf = buf;
    struct ext4_inode_ref ref;
    uint32_t sblock;
    uint32_t block_size;

    ext4_assert(f && f->mp);

    if(f->flags & O_WRONLY)
        return EPERM;

    if(!size)
        return EOK;

    EXT4_MP_LOCK(f->mp);

    if(rcnt)
        *rcnt = 0;

    r = ext4_fs_get_inode_ref(&f->mp->fs, f->inode, &ref);
    if(r != EOK){
        EXT4_MP_UNLOCK(f->mp);
        return r;
    }

    /*Sync file size*/
    f->fsize = ext4_inode_get_size(&f->mp->fs.sb, ref.inode);


    block_size = ext4_sb_get_block_size(&f->mp->fs.sb);
    size = size > (f->fsize - f->fpos) ? (f->fsize - f->fpos) : size;

    sblock 	= (f->fpos) / block_size;

    u = (f->fpos) % block_size;


    if(u){

        uint32_t ll = size > (block_size - u) ? (block_size - u) : size;

        r = ext4_fs_get_inode_data_block_index(&ref, sblock, &fblock);
        if(r != EOK)
            goto Finish;

        r = ext4_block_get(f->mp->fs.bdev, &b, fblock);
        if(r != EOK)
            goto Finish;

        memcpy(u8_buf, b.data + u, ll);

        r = ext4_block_set(f->mp->fs.bdev, &b);
        if(r != EOK)
            goto Finish;

        u8_buf  += ll;
        size    -= ll;
        f->fpos += ll;

        if(rcnt)
            *rcnt += ll;

        sblock++;
    }

    while(size >= block_size){

        r = ext4_fs_get_inode_data_block_index(&ref, sblock, &fblock);
        if(r != EOK)
            goto Finish;

        r = ext4_block_get_direct(f->mp->fs.bdev, u8_buf, fblock);
        if(r != EOK)
            goto Finish;


        u8_buf  += block_size;
        size    -= block_size;
        f->fpos += block_size;

        if(rcnt)
            *rcnt += block_size;

        sblock++;
    }

    if(size){
        r = ext4_fs_get_inode_data_block_index(&ref, sblock, &fblock);
        if(r != EOK)
            goto Finish;

        r = ext4_block_get(f->mp->fs.bdev, &b, fblock);
        if(r != EOK)
            goto Finish;

        memcpy(u8_buf, b.data , size);

        r = ext4_block_set(f->mp->fs.bdev, &b);
        if(r != EOK)
            goto Finish;

        f->fpos += size;

        if(rcnt)
            *rcnt += size;
    }

    Finish:
    ext4_fs_put_inode_ref(&ref);
    EXT4_MP_UNLOCK(f->mp);
    return r;
}

int	ext4_fwrite(ext4_file *f, void *buf, uint32_t size, uint32_t *wcnt)
{
    int		 r = EOK;
    uint32_t u;
    uint32_t fblock;
    struct ext4_block b;
    uint8_t	*u8_buf = buf;
    struct ext4_inode_ref ref;
    uint32_t sblock;
    uint32_t file_blocks;
    uint32_t block_size;

    ext4_assert(f && f->mp);

    if(f->flags & O_RDONLY)
        return EPERM;

    if(!size)
        return EOK;

    EXT4_MP_LOCK(f->mp);

    if(wcnt)
        *wcnt = 0;

    r = ext4_fs_get_inode_ref(&f->mp->fs, f->inode, &ref);
    if(r != EOK){
        EXT4_MP_UNLOCK(f->mp);
        return r;
    }

    /*Sync file size*/
    f->fsize = ext4_inode_get_size(&f->mp->fs.sb, ref.inode);

    block_size = ext4_sb_get_block_size(&f->mp->fs.sb);

    file_blocks = (f->fsize / block_size);

    if(f->fsize % block_size)
        file_blocks++;

    sblock 	= (f->fpos) / block_size;

    u = (f->fpos) % block_size;


    if(u){
        uint32_t ll = size > (block_size - u) ? (block_size - u) : size;

        r = ext4_fs_get_inode_data_block_index(&ref, sblock, &fblock);
        if(r != EOK)
            goto Finish;

        r = ext4_block_get(f->mp->fs.bdev, &b, fblock);
        if(r != EOK)
            goto Finish;

        memcpy(b.data + u, u8_buf, ll);
        b.dirty = true;

        r = ext4_block_set(f->mp->fs.bdev, &b);
        if(r != EOK)
            goto Finish;

        u8_buf  += ll;
        size    -= ll;
        f->fpos += ll;

        if(wcnt)
            *wcnt += ll;

        sblock++;
    }


    /*Start delay cache flush mode.*/
    r = ext4_block_delay_cache_flush(f->mp->fs.bdev, 1);
    if(r != EOK)
        goto Finish;

    while(size >= block_size){

        if(sblock < file_blocks){
            r = ext4_fs_get_inode_data_block_index(&ref, sblock, &fblock);
            if(r != EOK)
                break;
        }
        else {
            r = ext4_fs_append_inode_block(&ref, &fblock, &sblock);
            if(r != EOK)
                break;
        }

        r = ext4_block_set_direct(f->mp->fs.bdev, u8_buf, fblock);
        if(r != EOK)
            break;

        u8_buf  += block_size;
        size    -= block_size;
        f->fpos += block_size;

        if(wcnt)
            *wcnt += block_size;

        sblock++;
    }

    /*Stop delay cache flush mode*/
    ext4_block_delay_cache_flush(f->mp->fs.bdev, 0);

    if(r != EOK)
        goto Finish;

    if(size){
        if(sblock < file_blocks){
            r = ext4_fs_get_inode_data_block_index(&ref, sblock, &fblock);
            if(r != EOK)
                goto Finish;
        }
        else {
            r = ext4_fs_append_inode_block(&ref, &fblock, &sblock);
            if(r != EOK)
                goto Finish;
        }

        r = ext4_block_get(f->mp->fs.bdev, &b, fblock);
        if(r != EOK)
            goto Finish;

        memcpy(b.data, u8_buf , size);
        b.dirty = true;

        r = ext4_block_set(f->mp->fs.bdev, &b);
        if(r != EOK)
            goto Finish;

        f->fpos += size;

        if(wcnt)
            *wcnt += size;
    }

    if(f->fpos > f->fsize){
        f->fsize = f->fpos;
        ext4_inode_set_size(ref.inode, f->fsize);
        ref.dirty = true;
    }

    Finish:
    ext4_fs_put_inode_ref(&ref);
    EXT4_MP_UNLOCK(f->mp);
    return r;

}



int ext4_fseek (ext4_file *f, uint64_t offset, uint32_t origin)
{
    switch(origin){
    case SEEK_SET:

        if(offset > f->fsize)
            return EINVAL;

        f->fpos = offset;
        return EOK;
    case SEEK_CUR:
        if((offset + f->fpos) > f->fsize)
            return EINVAL;

        f->fpos += offset;
        return EOK;
    case SEEK_END:
        if(offset > f->fsize)
            return EINVAL;
        f->fpos = f->fsize - offset;
        return EOK;

    }

    return EINVAL;
}

uint64_t ext4_ftell (ext4_file *f)
{
    return  f->fpos;
}

uint64_t ext4_fsize (ext4_file *f)
{
    return  f->fsize;
}

/*********************************DIRECTORY OPERATION************************/

int	ext4_dir_open (ext4_dir *d, const char *path)
{
    return ext4_generic_open(&d->f, path, "r", false);
}

int	ext4_dir_close(ext4_dir *d)
{
    return ext4_fclose(&d->f);
}

ext4_direntry*	ext4_entry_get(ext4_dir *d, uint32_t id)
{
    int 	 r;
    uint32_t i;
    ext4_direntry *de = 0;
    struct ext4_inode_ref dir;
    struct ext4_directory_iterator it;

    EXT4_MP_LOCK(d->f.mp);

    r = ext4_fs_get_inode_ref(&d->f.mp->fs, d->f.inode, &dir);
    if(r != EOK){
        goto Finish;
    }

    r = ext4_dir_iterator_init(&it, &dir, 0);
    if(r != EOK){
        ext4_fs_put_inode_ref(&dir);
        goto Finish;
    }

    i = 0;
    while(r == EOK){

        if(!it.current)
            break;

        if(i == id){
            memcpy(&d->de, it.current, sizeof(ext4_direntry));
            de = &d->de;
            break;
        }


        i++;
        r = ext4_dir_iterator_next(&it);
    }

    ext4_dir_iterator_fini(&it);
    ext4_fs_put_inode_ref(&dir);

    Finish:
    EXT4_MP_UNLOCK(d->f.mp);
    return de;
}

/**
 * @}
 */
