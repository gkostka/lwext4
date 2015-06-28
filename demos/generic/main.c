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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include <ext4_filedev.h>
#include <io_raw.h>
#include <ext4.h>

#ifdef WIN32
#include <windows.h>
#endif

/**@brief   Input stream name.*/
char input_name[128] = "ext2";

/**@brief   Read-write size*/
static int rw_szie = 1024 * 1024;

/**@brief   Read-write size*/
static int rw_count = 10;

/**@brief   Directory test count*/
static int dir_cnt = 0;

/**@brief   Static or dynamic cache mode*/
static bool cache_mode = true;

/**@brief   Cleanup after test.*/
static bool cleanup_flag = false;

/**@brief   Block device stats.*/
static bool bstat = false;

/**@brief   Superblock stats.*/
static bool sbstat = false;

/**@brief   Indicates that input is windows partition.*/
static bool winpart = false;

/**@brief   File write buffer*/
static uint8_t *wr_buff;

/**@brief   File read buffer.*/
static uint8_t *rd_buff;

/**@brief   Block device handle.*/
static struct ext4_blockdev *bd;

/**@brief   Static cache instance*/
EXT4_BCACHE_STATIC_INSTANCE(_lwext4_cache, CONFIG_BLOCK_DEV_CACHE_SIZE, 1024);

/**@brief   Block cache handle.*/
static struct ext4_bcache *bc = &_lwext4_cache;

static const char *usage = "                                    \n\
Welcome in ext4 generic demo.                                   \n\
Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)  \n\
Usage:                                                          \n\
    --i   - input file              (default = ext2)            \n\
    --rws - single R/W size         (default = 1024 * 1024)     \n\
    --rwc - R/W count               (default = 10)              \n\
    --cache  - 0 static, 1 dynamic  (default = 1)               \n\
    --dirs   - directory test count (default = 0)               \n\
    --clean  - clean up after test                              \n\
    --bstat  - block device stats                               \n\
    --sbstat - superblock stats                                 \n\
    --wpart  - windows partition mode                           \n\
\n";

static char *entry_to_str(uint8_t type)
{
    switch (type) {
    case EXT4_DIRENTRY_UNKNOWN:
        return "[UNK] ";
    case EXT4_DIRENTRY_REG_FILE:
        return "[FIL] ";
    case EXT4_DIRENTRY_DIR:
        return "[DIR] ";
    case EXT4_DIRENTRY_CHRDEV:
        return "[CHA] ";
    case EXT4_DIRENTRY_BLKDEV:
        return "[BLK] ";
    case EXT4_DIRENTRY_FIFO:
        return "[FIF] ";
    case EXT4_DIRENTRY_SOCK:
        return "[SOC] ";
    case EXT4_DIRENTRY_SYMLINK:
        return "[SYM] ";
    default:
        break;
    }
    return "[???]";
}

static void dir_ls(const char *path)
{
    char sss[255];
    ext4_dir d;
    const ext4_direntry *de;

    printf("ls %s:\n", path);

    ext4_dir_open(&d, path);
    de = ext4_dir_entry_next(&d);

    while (de) {
        memcpy(sss, de->name, de->name_length);
        sss[de->name_length] = 0;
        printf("\t%s", entry_to_str(de->inode_type));
        printf("%s", sss);
        printf("\n");
        de = ext4_dir_entry_next(&d);
    }
    ext4_dir_close(&d);
}

static void mp_stats(void)
{
    struct ext4_mount_stats stats;
    ext4_mount_point_stats("/mp/", &stats);

    printf("ext4_mount_point_stats:\n");
    printf("\tinodes_count        = %" PRIu32 "\n", stats.inodes_count);
    printf("\tfree_inodes_count   = %" PRIu32 "\n", stats.free_inodes_count);
    printf("\tblocks_count        = %" PRIu32 "\n",
           (uint32_t)stats.blocks_count);
    printf("\tfree_blocks_count   = %" PRIu32 "\n",
           (uint32_t)stats.free_blocks_count);
    printf("\tblock_size          = %" PRIu32 "\n", stats.block_size);
    printf("\tblock_group_count   = %" PRIu32 "\n", stats.block_group_count);
    printf("\tblocks_per_group    = %" PRIu32 "\n", stats.blocks_per_group);
    printf("\tinodes_per_group    = %" PRIu32 "\n", stats.inodes_per_group);
    printf("\tvolume_name         = %s\n", stats.volume_name);
}

static void block_stats(void)
{
    uint32_t i;

    printf("ext4 blockdev stats\n");
    printf("\tbdev->bread_ctr          = %" PRIu32 "\n", bd->bread_ctr);
    printf("\tbdev->bwrite_ctr         = %" PRIu32 "\n", bd->bwrite_ctr);

    printf("\tbcache->ref_blocks       = %" PRIu32 "\n", bc->ref_blocks);
    printf("\tbcache->max_ref_blocks   = %" PRIu32 "\n", bc->max_ref_blocks);
    printf("\tbcache->lru_ctr          = %" PRIu32 "\n", bc->lru_ctr);

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("\tbcache->refctr[%" PRIu32 "]     = %" PRIu32 "\n", i,
               bc->refctr[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("\tbcache->lru_id[%" PRIu32 "]     = %" PRIu32 "\n", i,
               bc->lru_id[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("\tbcache->free_delay[%" PRIu32 "] = %d\n", i,
               bc->free_delay[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("\tbcache->lba[%" PRIu32 "]        = %" PRIu32 "\n", i,
               (uint32_t)bc->lba[i]);
    }
}

static clock_t get_ms(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

static bool dir_test(int len)
{
    ext4_file f;
    int r;
    int i;
    char path[64];
    clock_t diff;
    clock_t stop;
    clock_t start;

    printf("\ndir_test: %d\n", len);
    printf("directory create: /mp/dir1\n");
    start = get_ms();
    r = ext4_dir_mk("/mp/dir1");
    if (r != EOK) {
        printf("\text4_dir_mk: rc = %d\n", r);
        return false;
    }

    ext4_cache_write_back("/mp/", 1);
    printf("add files to: /mp/dir1\n");
    for (i = 0; i < len; ++i) {
        sprintf(path, "/mp/dir1/f%d", i);
        r = ext4_fopen(&f, path, "wb");
        if (r != EOK) {
            printf("\text4_fopen: rc = %d\n", r);
            return false;
        }
    }
    ext4_cache_write_back("/mp/", 0);

    stop = get_ms();
    diff = stop - start;
    dir_ls("/mp/dir1");
    printf("dir_test: time: %d ms\n", (int)diff);
    return true;
}

static bool file_test(void)
{
    int r;
    uint32_t size;
    ext4_file f;
    int i;
    clock_t start;
    clock_t stop;
    clock_t diff;
    uint32_t kbps;
    uint64_t size_bytes;

    printf("\nfile_test:\n");
    /*Add hello world file.*/
    r = ext4_fopen(&f, "/mp/hello.txt", "wb");
    r = ext4_fwrite(&f, "Hello World !\n", strlen("Hello World !\n"), 0);
    r = ext4_fclose(&f);

    printf("ext4_fopen: write test\n");
    start = get_ms();
    r = ext4_fopen(&f, "/mp/test1", "wb");
    if (r != EOK) {
        printf("\text4_fopen rc = %d\n", r);
        return false;
    }

    printf("ext4_write: %d * %d ...\n", rw_szie, rw_count);
    for (i = 0; i < rw_count; ++i) {

        memset(wr_buff, i % 10 + '0', rw_szie);

        r = ext4_fwrite(&f, wr_buff, rw_szie, &size);

        if ((r != EOK) || (size != rw_szie))
            break;
    }

    if (i != rw_count) {
        printf("\tfile_test: rw_count = %d\n", i);
        return false;
    }

    stop = get_ms();
    diff = stop - start;
    size_bytes = rw_szie * rw_count;
    size_bytes = (size_bytes * 1000) / 1024;
    kbps = (size_bytes) / (diff + 1);
    printf("\twrite time: %d ms\n", (int)diff);
    printf("\twrite speed: %" PRIu32 " KB/s\n", kbps);
    r = ext4_fclose(&f);

    printf("ext4_fopen: read test\n");
    start = get_ms();
    r = ext4_fopen(&f, "/mp/test1", "r+");
    if (r != EOK) {
        printf("\text4_fopen rc = %d\n", r);
        return false;
    }

    printf("ext4_read: %d * %d ...\n", rw_szie, rw_count);
    for (i = 0; i < rw_count; ++i) {
        memset(wr_buff, i % 10 + '0', rw_szie);
        r = ext4_fread(&f, rd_buff, rw_szie, &size);

        if ((r != EOK) || (size != rw_szie))
            break;

        if (memcmp(rd_buff, wr_buff, rw_szie)) {
            break;
        }
    }
    if (i != rw_count) {
        printf("\tfile_test: rw_count = %d\n", i);
        return false;
    }
    stop = get_ms();
    diff = stop - start;
    size_bytes = rw_szie * rw_count;
    size_bytes = (size_bytes * 1000) / 1024;
    kbps = (size_bytes) / (diff + 1);
    printf("\tread time: %d ms\n", (int)diff);
    printf("\tread speed: %" PRIu32 " KB/s\n", kbps);
    r = ext4_fclose(&f);

    return true;
}
static void cleanup(void)
{
    clock_t start;
    clock_t stop;
    clock_t diff;

    printf("\ncleanup:\n");
    ext4_fremove("/mp/hello.txt");
    printf("cleanup: remove /mp/test1\n");
    ext4_fremove("/mp/test1");

    printf("cleanup: remove /mp/dir1\n");
    start = get_ms();
    ext4_dir_rm("/mp/dir1");
    stop = get_ms();
    diff = stop - start;
    printf("cleanup: time: %d ms\n", (int)diff);
}

static bool open_filedev(void)
{
    ext4_filedev_filename(input_name);
    bd = ext4_filedev_get();
    if (!bd) {
        printf("open_filedev: fail\n");
        return false;
    }
    return true;
}

static bool open_winpartition(void)
{
#ifdef WIN32
    ext4_io_raw_filename(input_name);
    bd = ext4_io_raw_dev_get();
    if (!bd) {
        printf("open_winpartition: fail\n");
        return false;
    }
    return true;
#else
    printf(
        "open_winpartition: this mode should be used only under windows !\n");
    return false;
#endif
}

static bool mount(void)
{
    int r;
    if (winpart) {
        if (!open_winpartition())
            return false;
    } else {
        if (!open_filedev())
            return false;
    }
    wr_buff = malloc(rw_szie);
    rd_buff = malloc(rw_szie);

    if (!wr_buff || !rd_buff) {
        printf("mount: allocation failed\n");
        return false;
    }

    ext4_dmask_set(EXT4_DEBUG_ALL);

    r = ext4_device_register(bd, cache_mode ? 0 : bc, "ext4_fs");
    if (r != EOK) {
        printf("ext4_device_register: rc = %d\n", r);
        return false;
    }

    r = ext4_mount("ext4_fs", "/mp/");
    if (r != EOK) {
        printf("ext4_mount: rc = %d\n", r);
        return false;
    }

    return true;
}

static bool umount(void)
{
    int r = ext4_umount("/mp/");
    if (r != EOK) {
        printf("ext4_umount: rc = %d", r);
        return false;
    }
    return true;
}

static bool parse_opt(int argc, char **argv)
{
    int option_index = 0;
    int c;

    static struct option long_options[] = {{"in", required_argument, 0, 'a'},
                                           {"rws", required_argument, 0, 'b'},
                                           {"rwc", required_argument, 0, 'c'},
                                           {"cache", required_argument, 0, 'd'},
                                           {"dirs", required_argument, 0, 'e'},
                                           {"clean", no_argument, 0, 'f'},
                                           {"bstat", no_argument, 0, 'g'},
                                           {"sbstat", no_argument, 0, 'h'},
                                           {"wpart", no_argument, 0, 'i'},
                                           {0, 0, 0, 0}};

    while (-1 != (c = getopt_long(argc, argv, "a:b:c:d:e:fghi", long_options,
                                  &option_index))) {

        switch (c) {
        case 'a':
            strcpy(input_name, optarg);
            break;
        case 'b':
            rw_szie = atoi(optarg);
            break;
        case 'c':
            rw_count = atoi(optarg);
            break;
        case 'd':
            cache_mode = atoi(optarg);
            break;
        case 'e':
            dir_cnt = atoi(optarg);
            break;
        case 'f':
            cleanup_flag = true;
            break;
        case 'g':
            bstat = true;
            break;
        case 'h':
            sbstat = true;
            break;
        case 'i':
            winpart = true;
            break;
        default:
            printf("%s", usage);
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    if (!parse_opt(argc, argv))
        return EXIT_FAILURE;

    printf("test conditions:\n");
    printf("\timput name: %s\n", input_name);
    printf("\trw size: %d\n", rw_szie);
    printf("\trw count: %d\n", rw_count);
    printf("\tcache mode: %s\n", cache_mode ? "dynamic" : "static");

    if (!mount())
        return EXIT_FAILURE;

    cleanup();

    if (sbstat)
        mp_stats();

    dir_ls("/mp/");
    fflush(stdout);
    if (!dir_test(dir_cnt))
        return EXIT_FAILURE;

    fflush(stdout);
    if (!file_test())
        return EXIT_FAILURE;

    fflush(stdout);
    dir_ls("/mp/");

    if (sbstat)
        mp_stats();

    if (cleanup_flag)
        cleanup();

    if (bstat)
        block_stats();

    if (!umount())
        return EXIT_FAILURE;

    printf("\ntest finished\n");
    return EXIT_SUCCESS;
}
