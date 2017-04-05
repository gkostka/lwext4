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
#include "../blockdev/linux/file_dev.h"
#include "../blockdev/windows/file_windows.h"
#include "common/test_lwext4.h"

#ifdef WIN32
#include <windows.h>
#endif

/**@brief   Input stream name.*/
char input_name[128] = "ext_images/ext2";

/**@brief   Read-write size*/
static int rw_szie = 1024 * 1024;

/**@brief   Read-write size*/
static int rw_count = 10;

/**@brief   Directory test count*/
static int dir_cnt = 0;

/**@brief   Cleanup after test.*/
static bool cleanup_flag = false;

/**@brief   Block device stats.*/
static bool bstat = false;

/**@brief   Superblock stats.*/
static bool sbstat = false;

/**@brief   Indicates that input is windows partition.*/
static bool winpart = false;

/**@brief   Verbose mode*/
static bool verbose = 0;

/**@brief   Block device handle.*/
static struct ext4_blockdev *bd;

/**@brief   Block cache handle.*/
static struct ext4_bcache *bc;

static const char *usage = "                                    \n\
Welcome in ext4 generic demo.                                   \n\
Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)  \n\
Usage:                                                          \n\
[-i] --input    - input file         (default = ext2)           \n\
[-w] --rw_size  - single R/W size    (default = 1024 * 1024)    \n\
[-c] --rw_count - R/W count          (default = 10)             \n\
[-d] --dirs   - directory test count (default = 0)              \n\
[-l] --clean  - clean up after test                             \n\
[-b] --bstat  - block device stats                              \n\
[-t] --sbstat - superblock stats                                \n\
[-w] --wpart  - windows partition mode                          \n\
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
	file_dev_name_set(input_name);
	bd = file_dev_get();
	if (!bd) {
		printf("open_filedev: fail\n");
		return false;
	}
	return true;
}

static bool open_windows(void)
{
#ifdef WIN32
	file_windows_name_set(input_name);
	bd = file_windows_dev_get();
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

static bool open_filedev(void)
{
	return winpart ? open_windows() : open_linux();
}

static bool parse_opt(int argc, char **argv)
{
	int option_index = 0;
	int c;

	static struct option long_options[] = {
	    {"input", required_argument, 0, 'i'},
	    {"rw_size", required_argument, 0, 's'},
	    {"rw_count", required_argument, 0, 'c'},
	    {"dirs", required_argument, 0, 'd'},
	    {"clean", no_argument, 0, 'l'},
	    {"bstat", no_argument, 0, 'b'},
	    {"sbstat", no_argument, 0, 't'},
	    {"wpart", no_argument, 0, 'w'},
	    {"verbose", no_argument, 0, 'v'},
	    {"version", no_argument, 0, 'x'},
	    {0, 0, 0, 0}};

	while (-1 != (c = getopt_long(argc, argv, "i:s:c:q:d:lbtwvx",
				      long_options, &option_index))) {

		switch (c) {
		case 'i':
			strcpy(input_name, optarg);
			break;
		case 's':
			rw_szie = atoi(optarg);
			break;
		case 'c':
			rw_count = atoi(optarg);
			break;
		case 'd':
			dir_cnt = atoi(optarg);
			break;
		case 'l':
			cleanup_flag = true;
			break;
		case 'b':
			bstat = true;
			break;
		case 't':
			sbstat = true;
			break;
		case 'w':
			winpart = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'x':
			puts(VERSION);
			exit(0);
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

	printf("ext4_generic\n");
	printf("test conditions:\n");
	printf("\timput name: %s\n", input_name);
	printf("\trw size: %d\n", rw_szie);
	printf("\trw count: %d\n", rw_count);

	if (!open_filedev()) {
		printf("open_filedev error\n");
		return EXIT_FAILURE;
	}

	if (verbose)
		ext4_dmask_set(DEBUG_ALL);

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
	uint8_t *rw_buff = malloc(rw_szie);
	if (!rw_buff) {
		free(rw_buff);
		return EXIT_FAILURE;
	}
	if (!test_lwext4_file_test(rw_buff, rw_szie, rw_count)) {
		free(rw_buff);
		return EXIT_FAILURE;
	}

	free(rw_buff);

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
