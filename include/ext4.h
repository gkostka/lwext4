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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_errno.h>
#include <ext4_oflags.h>
#include <ext4_debug.h>

#include <ext4_blockdev.h>

/********************************OS LOCK INFERFACE***************************/

/**@brief   OS dependent lock interface.*/
struct ext4_lock {

	/**@brief   Lock access to mount point.*/
	void (*lock)(void);

	/**@brief   Unlock access to mount point.*/
	void (*unlock)(void);
};

/********************************FILE DESCRIPTOR*****************************/

/**@brief   File descriptor. */
typedef struct ext4_file {

	/**@brief   Mount point handle.*/
	struct ext4_mountpoint *mp;

	/**@brief   File inode id.*/
	uint32_t inode;

	/**@brief   Open flags.*/
	uint32_t flags;

	/**@brief   File size.*/
	uint64_t fsize;

	/**@brief   Actual file position.*/
	uint64_t fpos;
} ext4_file;

/*****************************DIRECTORY DESCRIPTOR***************************/

/**@brief   Directory entry descriptor. */
typedef struct ext4_direntry {
	uint32_t inode;
	uint16_t entry_length;
	uint8_t name_length;
	uint8_t inode_type;
	uint8_t name[255];
} ext4_direntry;

/**@brief   Directory descriptor. */
typedef struct ext4_dir {
	/**@brief   File descriptor.*/
	ext4_file f;
	/**@brief   Current directory entry.*/
	ext4_direntry de;
	/**@brief   Next entry offset.*/
	uint64_t next_off;
} ext4_dir;

/********************************MOUNT OPERATIONS****************************/

/**@brief   Register block device.
 *
 * @param   bd Block device.
 * @param   dev_name Block device name.
 *
 * @return  Standard error code.*/
int ext4_device_register(struct ext4_blockdev *bd,
			 const char *dev_name);

/**@brief   Un-register block device.
 *
 * @param   dev_name Block device name.
 *
 * @return  Standard error code.*/
int ext4_device_unregister(const char *dev_name);

/**@brief   Un-register all block devices.
 *
 * @return  Standard error code.*/
int ext4_device_unregister_all(void);

/**@brief   Mount a block device with EXT4 partition to the mount point.
 *
 * @param   dev_name Block device name (@ref ext4_device_register).
 * @param   mount_point Mount point, for example:
 *          -   /
 *          -   /my_partition/
 *          -   /my_second_partition/
 * @param   read_only mount as read-only mode.
 *
 * @return Standard error code */
int ext4_mount(const char *dev_name,
	       const char *mount_point,
	       bool read_only);

/**@brief   Umount operation.
 *
 * @param   mount_point Mount point.
 *
 * @return  Standard error code */
int ext4_umount(const char *mount_point);

/**@brief   Starts journaling. Journaling start/stop functions are transparent
 *          and might be used on filesystems without journaling support.
 * @warning Usage:
 *              ext4_mount("sda1", "/");
 *              ext4_journal_start("/");
 *
 *              //File operations here...
 *
 *              ext4_journal_stop("/");
 *              ext4_umount("/");
 * @param   mount_point Mount point.
 *
 * @return  Standard error code. */
int ext4_journal_start(const char *mount_point);

/**@brief   Stops journaling. Journaling start/stop functions are transparent
 *          and might be used on filesystems without journaling support.
 *
 * @param   mount_point Mount point name.
 *
 * @return  Standard error code. */
int ext4_journal_stop(const char *mount_point);

/**@brief   Journal recovery.
 * @warning Must be called after @ref ext4_mount.
 *
 * @param   mount_point Mount point.
 *
 * @return Standard error code. */
int ext4_recover(const char *mount_point);

/**@brief   Some of the filesystem stats. */
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

/**@brief   Get file mount point stats.
 *
 * @param   mount_point Mount point.
 * @param   stats Filesystem stats.
 *
 * @return Standard error code. */
int ext4_mount_point_stats(const char *mount_point,
			   struct ext4_mount_stats *stats);

/**@brief   Setup OS lock routines.
 *
 * @param   mount_point Mount point.
 * @param   locks  Lock and unlock functions
 *
 * @return Standard error code. */
int ext4_mount_setup_locks(const char *mount_point,
			   const struct ext4_lock *locks);

/**@brief   Acquire the filesystem superblock pointer of a mp.
 *
 * @param   mount_point Mount point.
 * @param   sb Superblock handle
 *
 * @return Standard error code. */
int ext4_get_sblock(const char *mount_point, struct ext4_sblock **sb);

/**@brief   Enable/disable write back cache mode.
 * @warning Default model of cache is write trough. It means that when You do:
 *
 *          ext4_fopen(...);
 *          ext4_fwrite(...);
 *                           < --- data is flushed to physical drive
 *
 *          When you do:
 *          ext4_cache_write_back(..., 1);
 *          ext4_fopen(...);
 *          ext4_fwrite(...);
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
 * @param   path Mount point.
 * @param   on Enable/disable cache writeback mode.
 *
 * @return Standard error code. */
int ext4_cache_write_back(const char *path, bool on);


/**@brief   Force cache flush.
 *
 * @param   path Mount point.
 *
 * @return  Standard error code. */
int ext4_cache_flush(const char *path);

/********************************FILE OPERATIONS*****************************/

/**@brief   Remove file by path.
 *
 * @param   path Path to file.
 *
 * @return  Standard error code. */
int ext4_fremove(const char *path);

/**@brief   Create a hardlink for a file.
 *
 * @param   path Path to file.
 * @param   hardlink_path Path of hardlink.
 *
 * @return  Standard error code. */
int ext4_flink(const char *path, const char *hardlink_path);

/**@brief Rename file.
 * @param path Source.
 * @param new_path Destination.
 * @return  Standard error code. */
int ext4_frename(const char *path, const char *new_path);

/**@brief   File open function.
 *
 * @param   file  File handle.
 * @param   path  File path, has to start from mount point:/my_partition/file.
 * @param   flags File open flags.
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
 * @return  Standard error code.*/
int ext4_fopen(ext4_file *file, const char *path, const char *flags);

/**@brief   Alternate file open function.
 *
 * @param   file  File handle.
 * @param   path  File path, has to start from mount point:/my_partition/file.
 * @param   flags File open flags.
 *
 * @return  Standard error code.*/
int ext4_fopen2(ext4_file *file, const char *path, int flags);

/**@brief   File close function.
 *
 * @param   file File handle.
 *
 * @return  Standard error code.*/
int ext4_fclose(ext4_file *file);


/**@brief   File truncate function.
 *
 * @param   file File handle.
 * @param   size New file size.
 *
 * @return  Standard error code.*/
int ext4_ftruncate(ext4_file *file, uint64_t size);

/**@brief   Read data from file.
 *
 * @param   file File handle.
 * @param   buf  Output buffer.
 * @param   size Bytes to read.
 * @param   rcnt Bytes read (NULL allowed).
 *
 * @return  Standard error code.*/
int ext4_fread(ext4_file *file, void *buf, size_t size, size_t *rcnt);

/**@brief   Write data to file.
 *
 * @param   file File handle.
 * @param   buf  Data to write
 * @param   size Write length..
 * @param   wcnt Bytes written (NULL allowed).
 *
 * @return  Standard error code.*/
int ext4_fwrite(ext4_file *file, const void *buf, size_t size, size_t *wcnt);

/**@brief   File seek operation.
 *
 * @param   file File handle.
 * @param   offset Offset to seek.
 * @param   origin Seek type:
 *              @ref SEEK_SET
 *              @ref SEEK_CUR
 *              @ref SEEK_END
 *
 * @return  Standard error code.*/
int ext4_fseek(ext4_file *file, int64_t offset, uint32_t origin);

/**@brief   Get file position.
 *
 * @param   file File handle.
 *
 * @return  Actual file position */
uint64_t ext4_ftell(ext4_file *file);

/**@brief   Get file size.
 *
 * @param   file File handle.
 *
 * @return  File size. */
uint64_t ext4_fsize(ext4_file *file);


/**@brief Get inode of file/directory/link.
 *
 * @param path    Parh to file/dir/link.
 * @param ret_ino Inode number.
 * @param inode   Inode internals.
 *
 * @return  Standard error code.*/
int ext4_raw_inode_fill(const char *path, uint32_t *ret_ino,
			struct ext4_inode *inode);

/**@brief Check if inode exists.
 *
 * @param path    Parh to file/dir/link.
 * @param type    Inode type.
 *                @ref EXT4_DIRENTRY_UNKNOWN
 *                @ref EXT4_DE_REG_FILE
 *                @ref EXT4_DE_DIR
 *                @ref EXT4_DE_CHRDEV
 *                @ref EXT4_DE_BLKDEV
 *                @ref EXT4_DE_FIFO
 *                @ref EXT4_DE_SOCK
 *                @ref EXT4_DE_SYMLINK
 *
 * @return  Standard error code.*/
int ext4_inode_exist(const char *path, int type);

/**@brief Change file/directory/link mode bits.
 *
 * @param path Path to file/dir/link.
 * @param mode New mode bits (for example 0777).
 *
 * @return  Standard error code.*/
int ext4_mode_set(const char *path, uint32_t mode);


/**@brief Get file/directory/link mode bits.
 *
 * @param path Path to file/dir/link.
 * @param mode New mode bits (for example 0777).
 *
 * @return  Standard error code.*/
int ext4_mode_get(const char *path, uint32_t *mode);

/**@brief Change file owner and group.
 *
 * @param path Path to file/dir/link.
 * @param uid  User id.
 * @param gid  Group id.
 *
 * @return  Standard error code.*/
int ext4_owner_set(const char *path, uint32_t uid, uint32_t gid);

/**@brief Get file/directory/link owner and group.
 *
 * @param path Path to file/dir/link.
 * @param uid  User id.
 * @param gid  Group id.
 *
 * @return  Standard error code.*/
int ext4_owner_get(const char *path, uint32_t *uid, uint32_t *gid);

/**@brief Set file/directory/link access time.
 *
 * @param path  Path to file/dir/link.
 * @param atime Access timestamp.
 *
 * @return  Standard error code.*/
int ext4_atime_set(const char *path, uint32_t atime);

/**@brief Set file/directory/link modify time.
 *
 * @param path  Path to file/dir/link.
 * @param mtime Modify timestamp.
 *
 * @return  Standard error code.*/
int ext4_mtime_set(const char *path, uint32_t mtime);

/**@brief Set file/directory/link change time.
 *
 * @param path  Path to file/dir/link.
 * @param ctime Change timestamp.
 *
 * @return  Standard error code.*/
int ext4_ctime_set(const char *path, uint32_t ctime);

/**@brief Get file/directory/link access time.
 *
 * @param path  Path to file/dir/link.
 * @param atime Access timestamp.
 *
 * @return  Standard error code.*/
int ext4_atime_get(const char *path, uint32_t *atime);

/**@brief Get file/directory/link modify time.
 *
 * @param path  Path to file/dir/link.
 * @param mtime Modify timestamp.
 *
 * @return  Standard error code.*/
int ext4_mtime_get(const char *path, uint32_t *mtime);

/**@brief Get file/directory/link change time.
 *
 * @param path  Pathto file/dir/link.
 * @param ctime Change timestamp.
 *
 * @return  standard error code*/
int ext4_ctime_get(const char *path, uint32_t *ctime);

/**@brief Create symbolic link.
 *
 * @param target Destination entry path.
 * @param path   Source entry path.
 *
 * @return  Standard error code.*/
int ext4_fsymlink(const char *target, const char *path);

/**@brief Create special file.
 * @param path     Path to new special file.
 * @param filetype Filetype of the new special file.
 * 	           (that must not be regular file, directory, or unknown type)
 * @param dev      If filetype is char device or block device,
 * 	           the device number will become the payload in the inode.
 * @return  Standard error code.*/
int ext4_mknod(const char *path, int filetype, uint32_t dev);

/**@brief Read symbolic link payload.
 *
 * @param path    Path to symlink.
 * @param buf     Output buffer.
 * @param bufsize Output buffer max size.
 * @param rcnt    Bytes read.
 *
 * @return  Standard error code.*/
int ext4_readlink(const char *path, char *buf, size_t bufsize, size_t *rcnt);

/**@brief Set extended attribute.
 *
 * @param path      Path to file/directory
 * @param name      Name of the entry to add.
 * @param name_len  Length of @name in bytes.
 * @param data      Data of the entry to add.
 * @param data_size Size of data to add.
 *
 * @return  Standard error code.*/
int ext4_setxattr(const char *path, const char *name, size_t name_len,
		  const void *data, size_t data_size);

/**@brief Get extended attribute.
 *
 * @param path      Path to file/directory.
 * @param name      Name of the entry to get.
 * @param name_len  Length of @name in bytes.
 * @param buf      Data of the entry to get.
 * @param buf_size Size of data to get.
 *
 * @return  Standard error code.*/
int ext4_getxattr(const char *path, const char *name, size_t name_len,
		  void *buf, size_t buf_size, size_t *data_size);

/**@brief List extended attributes.
 *
 * @param path     Path to file/directory.
 * @param list     List to hold the name of entries.
 * @param size     Size of @list in bytes.
 * @param ret_size Used bytes of @list.
 *
 * @return  Standard error code.*/
int ext4_listxattr(const char *path, char *list, size_t size, size_t *ret_size);

/**@brief Remove extended attribute.
 *
 * @param path     Path to file/directory.
 * @param name     Name of the entry to remove.
 * @param name_len Length of @name in bytes.
 *
 * @return  Standard error code.*/
int ext4_removexattr(const char *path, const char *name, size_t name_len);


/*********************************DIRECTORY OPERATION***********************/

/**@brief   Recursive directory remove.
 *
 * @param   path Directory path to remove
 *
 * @return  Standard error code.*/
int ext4_dir_rm(const char *path);

/**@brief Rename/move directory.
 *
 * @param path     Source path.
 * @param new_path Destination path.
 *
 * @return  Standard error code. */
int ext4_dir_mv(const char *path, const char *new_path);

/**@brief   Create new directory.
 *
 * @param   path Directory name.
 *
 * @return  Standard error code.*/
int ext4_dir_mk(const char *path);

/**@brief   Directory open.
 *
 * @param   dir  Directory handle.
 * @param   path Directory path.
 *
 * @return  Standard error code.*/
int ext4_dir_open(ext4_dir *dir, const char *path);

/**@brief   Directory close.
 *
 * @param   dir directory handle.
 *
 * @return  Standard error code.*/
int ext4_dir_close(ext4_dir *dir);

/**@brief   Return next directory entry.
 *
 * @param   dir Directory handle.
 *
 * @return  Directory entry id (NULL if no entry)*/
const ext4_direntry *ext4_dir_entry_next(ext4_dir *dir);

/**@brief   Rewine directory entry offset.
 *
 * @param   dir Directory handle.*/
void ext4_dir_entry_rewind(ext4_dir *dir);


#ifdef __cplusplus
}
#endif

#endif /* EXT4_H_ */

/**
 * @}
 */
