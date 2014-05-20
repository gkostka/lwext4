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
 * @brief Ext4 high level operations (files, directories, mount points...).
 *        Client has to include only this file.
 */

#ifndef EXT4_H_
#define EXT4_H_

#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <stdint.h>

/********************************FILE OPEN FLAGS*****************************/

#ifndef O_RDONLY
#define O_RDONLY    00
#endif

#ifndef O_WRONLY
#define O_WRONLY    01
#endif

#ifndef O_RDWR
#define O_RDWR      02
#endif

#ifndef O_CREAT
#define O_CREAT     0100
#endif

#ifndef O_EXCL
#define O_EXCL      0200
#endif

#ifndef O_TRUNC
#define O_TRUNC     01000
#endif

#ifndef O_APPEND
#define O_APPEND    02000
#endif

/********************************FILE SEEK FLAGS*****************************/

#ifndef SEEK_SET
#define SEEK_SET    0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR    1
#endif

#ifndef SEEK_END
#define SEEK_END    2
#endif

/********************************OS LOCK INFERFACE***************************/

/**@brief   OS dependent lock interface.*/
struct ext4_lock {

    /**@brief   Lock access to mount point*/
    void (*lock)(void);

    /**@brief   Unlock access to mount point*/
    void (*unlock)(void);
};


/********************************FILE DESCRIPTOR*****************************/

/**@brief   File descriptor*/
typedef struct ext4_file {

    /**@brief   Mount point handle.*/
    struct ext4_mountpoint *mp;

    /**@brief   File inode id*/
    uint32_t inode;

    /**@brief   Open flags.*/
    uint32_t flags;

    /**@brief   File size.*/
    uint64_t fsize;

    /**@brief   File position*/
    uint64_t fpos;
}ext4_file;

/*****************************DIRECTORY DESCRIPTOR***************************/
/**@brief   Directory entry types. Copy from ext4_types.h*/
enum  {
    EXT4_DIRENTRY_UNKNOWN = 0,
    EXT4_DIRENTRY_REG_FILE,
    EXT4_DIRENTRY_DIR,
    EXT4_DIRENTRY_CHRDEV,
    EXT4_DIRENTRY_BLKDEV,
    EXT4_DIRENTRY_FIFO,
    EXT4_DIRENTRY_SOCK,
    EXT4_DIRENTRY_SYMLINK
};

/**@brief   Directory entry descriptor. Copy from ext4_types.h*/
typedef struct {
    uint32_t inode;
    uint16_t entry_length;
    uint8_t name_length;
    uint8_t inode_type;
    uint8_t name[255];
}ext4_direntry;

typedef struct  {
    /**@brief   File descriptor*/
    ext4_file f;
    /**@brief   Current directory entry.*/
    ext4_direntry de;
}ext4_dir;

/********************************MOUNT OPERATIONS****************************/

/**@brief   Register a block device to a name.
 *          @warning Block device has to be filled by
 *          @ref EXT4_BLOCKDEV_STATIC_INSTANCE. Block cache may be created
 *          @ref EXT4_BCACHE_STATIC_INSTANCE.
 *          Block cache may by created automatically when bc parameter is 0.
 * @param   bd block device
 * @param   bd block device cache (0 = automatic cache mode)
 * @param   dev_name register name
 * @param   standard error code*/
int ext4_device_register(struct ext4_blockdev *bd, struct ext4_bcache *bc,
        const char *dev_name);

/**@brief   Mount a block device with EXT4 partition to the mount point.
 * @param   dev_name block device name (@ref ext4_device_register)
 * @param   mount_point mount point, for example
 *          -   /
 *          -   /my_partition/
 *          -   /my_second_partition/
 *
 * @return standard error code */
int ext4_mount(const char * dev_name,  char *mount_point);

/**@brief   Umount operation.
 * @param   mount_point mount name
 * @return  standard error code */
int ext4_umount(char *mount_point);


/**@brief   Some of the filesystem stats.*/
struct ext4_mount_stats {
    uint32_t inodes_count;
    uint32_t free_inodes_count;
    uint64_t blocks_count;
    uint64_t free_blocks_count;

    uint32_t block_size;
    uint32_t block_group_count;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;

    char volume_name[16];
};

/**@brief   Get file system params.
 * @param   mount_point mount path
 * @param   stats ext fs stats
 * @return  standard error code */
int ext4_mount_point_stats(const char *mount_point,
    struct ext4_mount_stats *stats);


/**@brief   Enable/disable write back cache mode.
 * @warning Default model of cache is write trough. It means that when You do:
 *
 *          ext4_fopen(...);
 *          ext4_fwrie(...);
 *                           < --- data is flushed to physical drive
 *
 *          When you do:
 *          ext4_cache_write_back(..., 1);
 *          ext4_fopen(...);
 *          ext4_fwrie(...);
 *                           < --- data is NOT flushed to physical drive
 *          ext4_cache_write_back(..., 0);
 *                           < --- when write back mode is disabled all
 *                                 cache data will be flushed
 * To enable write back mode permanently just call this function
 * once after ext4_mount (and disable before ext4_umount).
 *
 * Some of the function use write back cache mode internally.
 * If you enable write back mode twice you have to disable it twice
 * to flush all data:
 *
 *      ext4_cache_write_back(..., 1);
 *      ext4_cache_write_back(..., 1);
 *
 *      ext4_cache_write_back(..., 0);
 *      ext4_cache_write_back(..., 0);
 *
 * Write back mode is useful when you want to create a lot of empty
 * files/directories.
 *
 * @param   path mount point path
 * @param   on enable/disable
 *
 * @return  standard error code */
int ext4_cache_write_back(const char *path, bool on);

/********************************FILE OPERATIONS*****************************/

/**@brief   Remove file by path.
 * @param   path path to file
 * @return  standard error code */
int ext4_fremove(const char *path);

/**@brief   File open function.
 * @param   filename, (has to start from mount point)
 *          /my_partition/my_file
 * @param   flags open file flags
 *  |---------------------------------------------------------------|
 *  |   r or rb                 O_RDONLY                            |
 *  |---------------------------------------------------------------|
 *  |   w or wb                 O_WRONLY|O_CREAT|O_TRUNC            |
 *  |---------------------------------------------------------------|
 *  |   a or ab                 O_WRONLY|O_CREAT|O_APPEND           |
 *  |---------------------------------------------------------------|
 *  |   r+ or rb+ or r+b        O_RDWR                              |
 *  |---------------------------------------------------------------|
 *  |   w+ or wb+ or w+b        O_RDWR|O_CREAT|O_TRUNC              |
 *  |---------------------------------------------------------------|
 *  |   a+ or ab+ or a+b        O_RDWR|O_CREAT|O_APPEND             |
 *  |---------------------------------------------------------------|
 *
 * @return  standard error code*/
int ext4_fopen (ext4_file *f, const char *path, const char *flags);

/**@brief   File close function.
 * @param   f file handle
 * @return  standard error code*/
int ext4_fclose(ext4_file *f);

/**@brief   Read data from file.
 * @param   f file handle
 * @param   buf output buffer
 * @param   size bytes to read
 * @param   rcnt bytes read (may be NULL)
 * @return  standard error code*/
int ext4_fread (ext4_file *f, void *buf, uint32_t size, uint32_t *rcnt);

/**@brief   Write data to file.
 * @param   f file handle
 * @param   buf data to write
 * @param   size write length
 * @param   wcnt bytes written (may be NULL)
 * @return  standard error code*/
int ext4_fwrite(ext4_file *f, void *buf, uint32_t size, uint32_t *wcnt);

/**@brief   File seek operation.
 * @param   f file handle
 * @param   offset offset to seek
 * @param   origin seek type:
 *              @ref SEEK_SET
 *              @ref SEEK_CUR
 *              @ref SEEK_END
 * @return  standard error code*/
int ext4_fseek (ext4_file *f, uint64_t offset, uint32_t origin);

/**@brief   Get file position.
 * @param   f file handle
 * @return  actual file position */
uint64_t ext4_ftell (ext4_file *f);

/**@brief   Get file size.
 * @param   f file handle
 * @return  file size */
uint64_t ext4_fsize (ext4_file *f);

/*********************************DIRECTORY OPERATION***********************/

/**@brief   Recursive directory remove.
 * @param   path directory path to remove
 * @return  standard error code*/
int ext4_dir_rm(const char *path);

/**@brief   Create new directory.
 * @param   name new directory name
 * @return  standard error code*/
int ext4_dir_mk(const char *path);

/**@brief   Directory open.
 * @param   d directory handle
 * @param   path directory path
 * @return  standard error code*/
int ext4_dir_open (ext4_dir *d, const char *path);

/**@brief   Directory close.
 * @param   d directory handle
 * @return  standard error code*/
int ext4_dir_close(ext4_dir *d);


/**@brief   Return directory entry by id.
 * @param   d directory handle
 * @param   id entry id
 * @return  directory entry id (NULL id no entry)*/
ext4_direntry* ext4_dir_entry_get(ext4_dir *d, uint32_t id);

#endif /* EXT4_H_ */

/**
 * @}
 */
