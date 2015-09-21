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

#include <ext4.h>
#include "../../blockdev/linux/ext4_filedev.h"
#include "../../blockdev/windows/io_raw.h"
#include "../../blockdev/test_lwext4.h"

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

void io_timings_clear(void)
{
}

const struct ext4_io_stats *io_timings_get(uint32_t time_sum_ms)
{
	return NULL;
}

uint32_t tim_get_ms(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

uint64_t tim_get_us(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (t.tv_sec * 1000000) + (t.tv_usec);
}

static bool open_linux(void)
{
	ext4_filedev_filename(input_name);
	bd = ext4_filedev_get();
	if (!bd) {
		printf("open_filedev: fail\n");
		return false;
	}
	return true;
}

static bool open_windows(void)
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
	printf("open_winpartition: this mode should be used only under windows "
	       "!\n");
	return false;
#endif
}


static bool parse_opt(int argc, char **argv)
{
	int option_index = 0;
	int c;

	static struct option long_options[] = {
	    {"in", required_argument, 0, 'a'},
	    {"rws", required_argument, 0, 'b'},
	    {"rwc", required_argument, 0, 'c'},
	    {"cache", required_argument, 0, 'd'},
	    {"dirs", required_argument, 0, 'e'},
	    {"clean", no_argument, 0, 'f'},
	    {"bstat", no_argument, 0, 'g'},
	    {"sbstat", no_argument, 0, 'h'},
	    {"wpart", no_argument, 0, 'i'},
	    {0, 0, 0, 0}};

	while (-1 != (c = getopt_long(argc, argv, "a:b:c:d:e:fghi",
				      long_options, &option_index))) {

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

	if (winpart) {
		if (!open_windows())
			return EXIT_FAILURE;
	} else {
		if (!open_linux())
			return EXIT_FAILURE;
	}


	if (!test_lwext4_mount(bd, bc))
		return EXIT_FAILURE;

	test_lwext4_cleanup();

	if (sbstat)
		test_lwext4_mp_stats();

	test_lwext4_dir_ls("/mp/");
	fflush(stdout);
	if (!test_lwext4_dir_test(dir_cnt))
		return EXIT_FAILURE;

	fflush(stdout);
	if (!test_lwext4_file_test(rw_count, rw_szie))
		return EXIT_FAILURE;

	fflush(stdout);
	test_lwext4_dir_ls("/mp/");

	if (sbstat)
		test_lwext4_mp_stats();

	if (cleanup_flag)
		test_lwext4_cleanup();

	if (bstat)
		test_lwext4_block_stats();

	if (!test_lwext4_umount())
		return EXIT_FAILURE;

	printf("\ntest finished\n");
	return EXIT_SUCCESS;
}
