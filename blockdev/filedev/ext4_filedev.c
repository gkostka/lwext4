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
#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

/**@brief   Default filename.*/
static const char *fname = "ext2";

/**@brief   Image block size.*/
#define EXT4_FILEDEV_BSIZE      512

/**@brief   Image file descriptor.*/
static FILE *dev_file;

#define DROP_LINUXCACHE_BUFFERS 1


/**********************BLOCKDEV INTERFACE**************************************/
static int filedev_open(struct ext4_blockdev *bdev);
static int filedev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int filedev_bwrite(struct ext4_blockdev *bdev, const void *buf,
    uint64_t blk_id, uint32_t blk_cnt);
static int filedev_close(struct  ext4_blockdev *bdev);


/******************************************************************************/
EXT4_BLOCKDEV_STATIC_INSTANCE(
    _filedev,
    EXT4_FILEDEV_BSIZE,
    0,
    filedev_open,
    filedev_bread,
    filedev_bwrite,
    filedev_close
);

/******************************************************************************/
EXT4_BCACHE_STATIC_INSTANCE(__cache, CONFIG_BLOCK_DEV_CACHE_SIZE, 1024);

/******************************************************************************/
static int filedev_open(struct ext4_blockdev *bdev)
{
    dev_file = fopen(fname, "r+b");

    if(!dev_file)
        return EIO;

    /*No buffering at file.*/
    setbuf(dev_file, 0);

    if(fseek(dev_file, 0, SEEK_END))
        return EFAULT;

    _filedev.ph_bcnt = ftell(dev_file) / _filedev.ph_bsize;

    return EOK;
}

/******************************************************************************/

static int filedev_bread(struct  ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt)
{
    if(fseek(dev_file, blk_id * bdev->ph_bsize, SEEK_SET))
        return EIO;

    if(!fread(buf, bdev->ph_bsize * blk_cnt, 1, dev_file))
        return EIO;

    return EOK;
}

static void drop_cache(void)
{
#if defined(__linux__) && DROP_LINUXCACHE_BUFFERS
    int fd;
    char* data = "3";

    sync();
    fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    write(fd, data, sizeof(char));
    close(fd);
#endif
}

/******************************************************************************/
static int filedev_bwrite(struct ext4_blockdev *bdev, const void *buf,
    uint64_t blk_id, uint32_t blk_cnt)
{
    if(fseek(dev_file, blk_id * bdev->ph_bsize, SEEK_SET))
        return EIO;

    if(!fwrite(buf, bdev->ph_bsize * blk_cnt, 1, dev_file))
        return EIO;

    drop_cache();
    return EOK;
}
/******************************************************************************/
static int filedev_close(struct  ext4_blockdev *bdev)
{
    fclose(dev_file);
    return EOK;
}


/******************************************************************************/

struct ext4_bcache* ext4_filecache_get(void)
{
    return &__cache;
}
/******************************************************************************/
struct ext4_blockdev* ext4_filedev_get(void)
{
    return &_filedev;
}
/******************************************************************************/
void ext4_filedev_filename(const char *n)
{
    fname = n;
}

/******************************************************************************/
