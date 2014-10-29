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

#include <config.h>
#include <hw_init.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include <usb_msc_lwext4.h>
#include <ext4.h>

/**@brief   Read-write size*/
#define READ_WRITE_SZIZE 1024 * 16

/**@brief   Delay test (slower LCD scroll)*/
#define TEST_DELAY_MS    2000

/**@brief   Input stream name.*/
char input_name[128] = "ext2";

/**@brief   Read-write size*/
static int rw_szie  = READ_WRITE_SZIZE;

/**@brief   Read-write size*/
static int rw_count = 1000;

/**@brief   Directory test count*/
static int dir_cnt  = 500;

/**@brief   Static or dynamic cache mode*/
static bool cache_mode = false;

/**@brief   Cleanup after test.*/
static bool cleanup_flag = false;

/**@brief   Block device stats.*/
static bool bstat = false;

/**@brief   Superblock stats.*/
static bool sbstat = false;

/**@brief   File write buffer*/
static uint8_t wr_buff[READ_WRITE_SZIZE];

/**@brief   File read buffer.*/
static uint8_t rd_buff[READ_WRITE_SZIZE];

/**@brief   Block device handle.*/
static struct ext4_blockdev *bd;

/**@brief   Block cache handle.*/
static struct ext4_bcache   *bc;

static char* entry_to_str(uint8_t type)
{
    switch(type){
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
    int j = 0;
    char sss[255];
    ext4_dir d;
    ext4_direntry *de;

    printf("********************\n");

    ext4_dir_open(&d, path);
    de = ext4_dir_entry_get(&d, j++);
    printf("ls %s\n", path);

    while(de){
        memcpy(sss, de->name, de->name_length);
        sss[de->name_length] = 0;
        printf("%s", entry_to_str(de->inode_type));
        printf("%s", sss);
        printf("\n");
        de = ext4_dir_entry_get(&d, j++);
    }
    printf("********************\n");
    ext4_dir_close(&d);
}

static void mp_stats(void)
{
    struct ext4_mount_stats stats;
    ext4_mount_point_stats("/mp/", &stats);

    printf("********************\n");
    printf("ext4_mount_point_stats\n");
    printf("inodes_count = %u\n", stats.inodes_count);
    printf("free_inodes_count = %u\n", stats.free_inodes_count);
    printf("blocks_count = %u\n", (uint32_t)stats.blocks_count);
    printf("free_blocks_count = %u\n", (uint32_t)stats.free_blocks_count);
    printf("block_size = %u\n", stats.block_size);
    printf("block_group_count = %u\n", stats.block_group_count);
    printf("blocks_per_group= %u\n", stats.blocks_per_group);
    printf("inodes_per_group = %u\n", stats.inodes_per_group);
    printf("volume_name = %s\n", stats.volume_name);

    printf("********************\n");

}

static void block_stats(void)
{
    uint32_t i;

    printf("********************\n");
    printf("ext4 blockdev stats\n");
    printf("bdev->bread_ctr = %u\n", bd->bread_ctr);
    printf("bdev->bwrite_ctr = %u\n", bd->bwrite_ctr);


    printf("bcache->ref_blocks = %u\n", bc->ref_blocks);
    printf("bcache->max_ref_blocks = %u\n", bc->max_ref_blocks);
    printf("bcache->lru_ctr = %u\n", bc->lru_ctr);

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->refctr[%d]= %u\n", i, bc->refctr[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->lru_id[%d] = %u\n", i, bc->lru_id[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->free_delay[%d] = %d\n", i, bc->free_delay[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->lba[%d] = %u\n", i, (uint32_t)bc->lba[i]);
    }

    printf("********************\n");
}

static clock_t get_ms(void)
{
    return hw_get_ms();
}

static bool dir_test(int len)
{
    ext4_file f;
    int       r;
    int       i;
    char path[64];
    clock_t diff;
    clock_t stop;
    clock_t start;
    start = get_ms();

    printf("Directory create: /mp/dir1\n");
    r = ext4_dir_mk("/mp/dir1");
    if(r != EOK){
        printf("Unable to create directory: /mp/dir1\n");
        return false;
    }



    printf("Add files to: /mp/dir1\n");
    for (i = 0; i < len; ++i) {
        sprintf(path, "/mp/dir1/f%d", i);
        r = ext4_fopen(&f, path, "wb");
        if(r != EOK){
            printf("Unable to create file in directory: /mp/dir1 r = %d\n", r);
            return false;
        }
    }

    stop =  get_ms();
    diff = stop - start;
    dir_ls("/mp/dir1");
    printf("dir_test time: %d ms\n", (int)diff);
    return true;
}


static bool file_test(void)
{
    int r;
    uint32_t  size;
    ext4_file f;
    int i;
    clock_t start;
    clock_t stop;
    clock_t diff;
    uint32_t kbps;
    uint64_t size_bytes;
    /*Add hello world file.*/
    r = ext4_fopen(&f, "/mp/hello.txt", "wb");
    r = ext4_fwrite(&f, "Hello World !\n", strlen("Hello World !\n"), 0);
    r = ext4_fclose(&f);


    printf("ext4_fopen: test1\n");

    start = get_ms();
    r = ext4_fopen(&f, "/mp/test1", "wb");
    if(r != EOK){
        printf("ext4_fopen ERROR = %d\n", r);
        return false;
    }

    printf("ext4_write: %d * %d ...\n" , rw_szie, rw_count);
    for (i = 0; i < rw_count; ++i) {

        memset(wr_buff, i % 10 + '0', rw_szie);

        r = ext4_fwrite(&f, wr_buff, rw_szie, &size);

        if((r != EOK) || (size != rw_szie))
            break;

        if(!(i % (rw_count >> 3))){
            printf("*");
            fflush(stdout);
        }
    }

    if(i != rw_count){
        printf("ERROR: rw_count = %d\n", i);
        return false;
    }

    printf(" OK\n");
    stop = get_ms();
    diff = stop - start;
    size_bytes = rw_szie * rw_count;
    size_bytes = (size_bytes * 1000) / 1024;
    kbps = (size_bytes) / (diff + 1);
    printf("file_test write time: %d ms\n", (int)diff);
    printf("file_test write speed: %d KB/s\n", (int)kbps);
    r = ext4_fclose(&f);
    printf("ext4_fopen: test1\n");


    start = get_ms();
    r = ext4_fopen(&f, "/mp/test1", "r+");
    if(r != EOK){
        printf("ext4_fopen ERROR = %d\n", r);
        return false;
    }

    printf("ext4_read: %d * %d ...\n" , rw_szie, rw_count);

    for (i = 0; i < rw_count; ++i) {
        memset(wr_buff, i % 10 + '0', rw_szie);
        r = ext4_fread(&f, rd_buff, rw_szie, &size);

        if((r != EOK) || (size != rw_szie))
            break;

        if(memcmp(rd_buff, wr_buff, rw_szie))
            break;


        if(!(i % (rw_count >> 3)))
            printf("*");
    }
    if(i != rw_count){
        printf("ERROR: rw_count = %d\n", i);
        return false;
    }
    printf(" OK\n");
    stop = get_ms();
    diff = stop - start;
    size_bytes = rw_szie * rw_count;
    size_bytes = (size_bytes * 1000) / 1024;
    kbps = (size_bytes) / (diff + 1);
    printf("file_test read time: %d ms\n", (int)diff);
    printf("file_test read speed: %d KB/s\n", kbps);
    r = ext4_fclose(&f);

    return true;

}
static void cleanup(void)
{
    clock_t start;
    clock_t stop;
    clock_t diff;

    ext4_fremove("/mp/hello.txt");

    printf("cleanup: remove /mp/test1\n");
    start = get_ms();
    ext4_fremove("/mp/test1");
    stop = get_ms();
    diff = stop - start;
    printf("cleanup: time: %d ms\n", (int)diff);


    printf("cleanup: remove /mp/dir1\n");
    start = get_ms();
    ext4_dir_rm("/mp/dir1");
    stop = get_ms();
    diff = stop - start;
    printf("cleanup: time: %d ms\n", (int)diff);
}

static bool open_filedev(void)
{

    bd = ext4_usb_msc_get();
    bc = ext4_usb_msc_cache_get();
    if(!bd || !bc){
        printf("Block device ERROR\n");
        return false;
    }
    return true;
}

static bool mount(void)
{
    int r;

    if(!open_filedev())
        return false;

    ext4_dmask_set(EXT4_DEBUG_ALL);

    r = ext4_device_register(bd, cache_mode ? 0 : bc, "ext4_filesim");
    if(r != EOK){
        printf("ext4_device_register ERROR = %d\n", r);
        return false;
    }

    r = ext4_mount("ext4_filesim", "/mp/");
    if(r != EOK){
        printf("ext4_mount ERROR = %d\n", r);
        return false;
    }

    return true;
}

static bool umount(void)
{
    int r = ext4_umount("/mp/");
    if(r != EOK){
        printf("ext4_umount: FAIL %d", r);
        return false;
    }
    return true;
}


int main(void)
{
    hw_init();

    setbuf(stdout, 0);
    printf("Connect USB drive...\n");

    while(!hw_usb_connected())
        hw_usb_process();
    printf("USB drive connected\n");

    while(!hw_usb_enum_done())
        hw_usb_process();
    printf("USB drive enum done\n");

    hw_led_red(1);

    printf("Test conditions:\n");
    printf("Imput name: %s\n", input_name);
    printf("RW size: %d\n",  rw_szie);
    printf("RW count: %d\n", rw_count);
    printf("Cache mode: %s\n", cache_mode ? "dynamic" : "static");


    hw_wait_ms(TEST_DELAY_MS);
    if(!mount())
        return EXIT_FAILURE;

    ext4_cache_write_back("/mp/", 1);
    cleanup();

    if(sbstat){
        hw_wait_ms(TEST_DELAY_MS);
        mp_stats();
    }

    hw_wait_ms(TEST_DELAY_MS);
    dir_ls("/mp/");
    if(!dir_test(dir_cnt))
        return EXIT_FAILURE;

    hw_wait_ms(TEST_DELAY_MS);
    if(!file_test())
        return EXIT_FAILURE;

    dir_ls("/mp/");

    if(sbstat){
        hw_wait_ms(TEST_DELAY_MS);
        mp_stats();
    }

    if(cleanup_flag){
        hw_wait_ms(TEST_DELAY_MS);
        cleanup();
    }

    if(bstat){
        hw_wait_ms(TEST_DELAY_MS);
        block_stats();
    }

    ext4_cache_write_back("/mp/", 0);
    if(!umount())
        return EXIT_FAILURE;

    printf("\nTest finished: OK\n");
    printf("Press RESET to restart\n");

    while (1) {
        hw_wait_ms(500);
        hw_led_green(1);
        hw_wait_ms(500);
        hw_led_green(0);

    }
}

