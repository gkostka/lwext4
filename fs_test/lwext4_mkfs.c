/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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
#include <ext4_mkfs.h>
#include "../blockdev/linux/file_dev.h"
#include "../blockdev/windows/file_windows.h"

/**@brief   Input stream name.*/
const char *input_name = NULL;

/**@brief   Block device handle.*/
static struct ext4_blockdev *bd;

/**@brief   Indicates that input is windows partition.*/
static bool winpart = false;

static int fs_type = F_SET_EXT4;

static struct ext4_fs fs;
static struct ext4_mkfs_info info = {
	.block_size = 1024,
	.journal = true,
};

static bool verbose = false;

static const char *usage = "                                    \n\
Welcome in lwext4_mkfs tool .                                   \n\
Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)  \n\
Usage:                                                          \n\
[-i] --input   - input file name (or blockdevice)               \n\
[-w] --wpart   - windows partition mode                         \n\
[-v] --verbose - verbose mode		                        \n\
[-b] --block   - block size: 1024, 2048, 4096 (default 1024)    \n\
[-e] --ext     - fs type (ext2: 2, ext3: 3 ext4: 4))  	        \n\
\n";


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
	    {"block", required_argument, 0, 'b'},
	    {"ext", required_argument, 0, 'e'},
	    {"wpart", no_argument, 0, 'w'},
	    {"verbose", no_argument, 0, 'v'},
	    {"version", no_argument, 0, 'x'},
	    {0, 0, 0, 0}};

	while (-1 != (c = getopt_long(argc, argv, "i:b:e:wvx",
				      long_options, &option_index))) {

		switch (c) {
		case 'i':
			input_name = optarg;
			break;
		case 'b':
			info.block_size = atoi(optarg);
			break;
		case 'e':
			fs_type = atoi(optarg);
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

	switch (info.block_size) {
	case 1024:
	case 2048:
	case 4096:
		break;
	default:
		printf("parse_opt: block_size = %"PRIu32" unsupported\n",
				info.block_size);
		return false;
	}

	switch (fs_type) {
	case F_SET_EXT2:
	case F_SET_EXT3:
	case F_SET_EXT4:
		break;
	default:
		printf("parse_opt: fs_type = %"PRIu32" unsupported\n", fs_type);
		return false;
	}

	return true;
}

int main(int argc, char **argv)
{
	int r;
	if (!parse_opt(argc, argv)){
		printf("parse_opt error\n");
		return EXIT_FAILURE;
	}

	if (!open_filedev()) {
		printf("open_filedev error\n");
		return EXIT_FAILURE;
	}

	if (verbose)
		ext4_dmask_set(DEBUG_ALL);

	printf("ext4_mkfs: ext%d\n", fs_type);
	r = ext4_mkfs(&fs, bd, &info, fs_type);
	if (r != EOK) {
		printf("ext4_mkfs error: %d\n", r);
		return EXIT_FAILURE;
	}

	memset(&info, 0, sizeof(struct ext4_mkfs_info));
	r = ext4_mkfs_read_info(bd, &info);
	if (r != EOK) {
		printf("ext4_mkfs_read_info error: %d\n", r);
		return EXIT_FAILURE;
	}

	printf("Created filesystem with parameters:\n");
	printf("Size: %"PRIu64"\n", info.len);
	printf("Block size: %"PRIu32"\n", info.block_size);
	printf("Blocks per group: %"PRIu32"\n", info.blocks_per_group);
	printf("Inodes per group: %"PRIu32"\n",	info.inodes_per_group);
	printf("Inode size: %"PRIu32"\n", info.inode_size);
	printf("Inodes: %"PRIu32"\n", info.inodes);
	printf("Journal blocks: %"PRIu32"\n", info.journal_blocks);
	printf("Features ro_compat: 0x%x\n", info.feat_ro_compat);
	printf("Features compat: 0x%x\n", info.feat_compat);
	printf("Features incompat: 0x%x\n", info.feat_incompat);
	printf("BG desc reserve: %"PRIu32"\n", info.bg_desc_reserve_blocks);
	printf("Descriptor size: %"PRIu32"\n",info.dsc_size);
	printf("Label: %s\n", info.label);

	printf("\nDone ...\n");
	return EXIT_SUCCESS;
}
