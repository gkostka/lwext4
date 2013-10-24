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

#include <ext4_filedev.h>
#include <ext4.h>

char input_name[128] = "ext2";

/**@brief	Read-write size*/
static int rw_szie  = 1024;

/**@brief	Read-write size*/
static int rw_count = 10000;

/**@brief   Directory test count*/
static int dir_cnt  = 10;

/**@brief   Static or dynamic cache mode*/
static bool cache_mode = false;

/**@brief   Cleanup after test.*/
static bool cleanup_flag = false;

/**@brief   Block device stats.*/
static bool bstat = false;

/**@brief   Superblock stats.*/
static bool sbstat = false;

/**@brief	File write buffer*/
static uint8_t	*wr_buff;

/**@brief	File read buffer.*/
static uint8_t	*rd_buff;

/**@brief	Block device handle.*/
static struct ext4_blockdev *bd;

/**@brief	Block cache handle.*/
static struct ext4_bcache   *bc;

static const char *usage = "									\n\
Welcome in ext4 generic demo.									\n\
Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)	\n\
Usage:															\n\
	-i   - input file             (default = ext2)				\n\
	-rws - single R/W size        (default = 1024)				\n\
	-rwc - R/W count              (default = 10000) 			\n\
	-cache  - 0 static, 1 dynamic  (default = 0)                \n\
    -dirs   - directory test count (default = 10)               \n\
    -clean  - clean up after test                               \n\
    -bstat  - block device stats                                \n\
    -sbstat - superblock stats                                  \n\
\n";

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

	printf("**********************************************\n");

	ext4_dir_open(&d, path);
	de = ext4_dir_entry_get(&d, j++);
	printf("ls %s\n", path);

	while(de){
		memcpy(sss, de->name, de->name_length);
		sss[de->name_length] = 0;
		printf(entry_to_str(de->inode_type));
		printf(sss);
		printf("\n");
		de = ext4_dir_entry_get(&d, j++);
	}
	printf("**********************************************\n");
	ext4_dir_close(&d);
}

static void mp_stats(void)
{
    struct ext4_mount_stats stats;
    ext4_mount_point_stats("/mp/", &stats);

    printf("**********************************************\n");
    printf("ext4_mount_point_stats\n");
    printf("inodes_count        = %u\n", stats.inodes_count);
    printf("free_inodes_count   = %u\n", stats.free_inodes_count);
    printf("blocks_count        = %u\n", (uint32_t)stats.blocks_count);
    printf("free_blocks_count   = %u\n", (uint32_t)stats.free_blocks_count);
    printf("block_size          = %u\n", stats.block_size);
    printf("block_group_count   = %u\n", stats.block_group_count);
    printf("blocks_per_group    = %u\n", stats.blocks_per_group);
    printf("inodes_per_group    = %u\n", stats.inodes_per_group);
    printf("volume_name         = %s\n", stats.volume_name);

    printf("**********************************************\n");

}

static void block_stats(void)
{
    uint32_t i;

    printf("**********************************************\n");
    printf("ext4 blockdev stats\n");
    printf("bdev->bread_ctr          = %u\n", bd->bread_ctr);
    printf("bdev->bwrite_ctr         = %u\n", bd->bwrite_ctr);


    printf("bcache->ref_blocks       = %u\n", bc->ref_blocks);
    printf("bcache->max_ref_blocks   = %u\n", bc->max_ref_blocks);
    printf("bcache->lru_ctr          = %u\n", bc->lru_ctr);

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->refctr[%d]     = %u\n", i, bc->refctr[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->lru_id[%d]     = %u\n", i, bc->lru_id[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->free_delay[%d] = %d\n", i, bc->free_delay[i]);
    }

    printf("\n");
    for (i = 0; i < bc->cnt; ++i) {
        printf("bcache->lba[%d]        = %u\n", i, (uint32_t)bc->lba[i]);
    }



    printf("**********************************************\n");
}

static bool dir_test(int len)
{
    ext4_file f;
    int       r;
    int       i;
    char path[64];

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
            printf("Unable to create file in directory: /mp/dir1\n");
            return false;
        }
    }

    dir_ls("/mp/dir1");
    return true;
}

static void cleanup(void)
{
    ext4_fremove("/mp/hello.txt");
    ext4_fremove("/mp/test1");
    ext4_dir_rm("/mp/dir1");
}

int main(int argc, char **argv)
{
	int option_index = 0;
	int	c;
	int	r;
	int	i;
	uint32_t  size;
	ext4_file f;

    static struct option long_options[] =
      {
        {"in",     	required_argument, 0, 'a'},
        {"rws",     required_argument, 0, 'b'},
        {"rwc",		required_argument, 0, 'c'},
        {"cache",   required_argument, 0, 'd'},
        {"dirs",    required_argument, 0, 'e'},
        {"clean",   no_argument,       0, 'f'},
        {"bstat",   no_argument,       0, 'g'},
        {"sbstat",  no_argument,       0, 'h'},
        {0, 0, 0, 0}
      };

    while(-1 != (c = getopt_long (argc, argv, "a:b:c:d:e:fgh", long_options, &option_index))) {

    	switch(c){
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
    		default:
    			printf(usage);
    			return EXIT_FAILURE;

    	}
    }

    printf("Test conditions:\n");
    printf("Imput name: %s\n", input_name);
    printf("RW size: %d\n",  rw_szie);
    printf("RW count: %d\n", rw_count);
    printf("Cache mode: %s\n", cache_mode ? "dynamic" : "static");



    ext4_filedev_filename(input_name);

    wr_buff = malloc(rw_szie);
    rd_buff = malloc(rw_szie);

    if(!wr_buff || !rd_buff){
    	printf("Read-Write allocation ERROR\n");
    	return EXIT_FAILURE;
    }

	bd = ext4_filedev_get();
	bc = ext4_filecache_get();

    if(!bd || !bc){
    	printf("Block device ERROR\n");
    	return EXIT_FAILURE;
    }

	ext4_dmask_set(EXT4_DEBUG_ALL);

	r = ext4_device_register(bd, cache_mode ? 0 : bc, "ext4_filesim");
	if(r != EOK){
		printf("ext4_device_register ERROR = %d\n", r);
		return EXIT_FAILURE;
	}

	r = ext4_mount("ext4_filesim", "/mp/");
	if(r != EOK){
		printf("ext4_mount ERROR = %d\n", r);
		return EXIT_FAILURE;
	}

	cleanup();

    if(sbstat)
        mp_stats();


    dir_ls("/mp/");
	dir_test(dir_cnt);

    /*Add hello world file.*/
    r = ext4_fopen(&f, "/mp/hello.txt", "wb");
    r = ext4_fwrite(&f, "Hello World !\n", strlen("Hello World !\n"), 0);
    r = ext4_fclose(&f);


	printf("ext4_fopen: test1\n");

	r = ext4_fopen(&f, "/mp/test1", "wb");
	if(r != EOK){
		printf("ext4_fopen ERROR = %d\n", r);
		return EXIT_FAILURE;
	}

	printf("ext4_write: %d * %d ..." , rw_count, rw_szie);

	for (i = 0; i < rw_count; ++i) {

		memset(wr_buff, i & 0xFF, rw_szie);

		r = ext4_fwrite(&f, wr_buff, rw_szie, &size);

		if((r != EOK) || (size != rw_szie))
			break;
	}

	if(i != rw_count){
		printf("ERROR: rw_count = %d\n", i);
		return EXIT_FAILURE;
	}

	printf("OK\n");
	r = ext4_fclose(&f);
	printf("ext4_fopen: test1\n");

	r = ext4_fopen(&f, "/mp/test1", "r+");
	if(r != EOK){
		printf("ext4_fopen ERROR = %d\n", r);
		return EXIT_FAILURE;
	}

	printf("ext4_read: %d * %d ..." , rw_count, rw_szie);

	for (i = 0; i < rw_count; ++i) {
		memset(wr_buff, i & 0xFF, rw_szie);
		r = ext4_fread(&f, rd_buff, rw_szie, &size);

		if((r != EOK) || (size != rw_szie))
			break;

		if(memcmp(rd_buff, wr_buff, rw_szie)){
			break;
		}
	}
	if(i != rw_count){
		printf("ERROR: rw_count = %d\n", i);
		return EXIT_FAILURE;
	}

	printf("OK\n");
	r = ext4_fclose(&f);

	dir_ls("/mp/");

	if(sbstat)
	    mp_stats();

	if(bstat)
	    block_stats();

	if(cleanup_flag)
	    cleanup();

	r = ext4_umount("/mp/");
	printf("Test finish: OK\n");
    return EXIT_SUCCESS;

}
