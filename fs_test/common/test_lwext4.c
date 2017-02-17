/*
 * Copyright (c) 2014 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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

#include "../common/test_lwext4.h"

#include <ext4.h>

#include <stdio.h>
#include <inttypes.h>
#include <string.h>


/**@brief   Block device handle.*/
static struct ext4_blockdev *bd;

/**@brief   Block cache handle.*/
static struct ext4_bcache *bc;

static char *entry_to_str(uint8_t type)
{
	switch (type) {
	case EXT4_DE_UNKNOWN:
		return "[unk] ";
	case EXT4_DE_REG_FILE:
		return "[fil] ";
	case EXT4_DE_DIR:
		return "[dir] ";
	case EXT4_DE_CHRDEV:
		return "[cha] ";
	case EXT4_DE_BLKDEV:
		return "[blk] ";
	case EXT4_DE_FIFO:
		return "[fif] ";
	case EXT4_DE_SOCK:
		return "[soc] ";
	case EXT4_DE_SYMLINK:
		return "[sym] ";
	default:
		break;
	}
	return "[???]";
}

static long int get_ms(void) { return tim_get_ms(); }

static void printf_io_timings(long int diff)
{
	const struct ext4_io_stats *stats = io_timings_get(diff);
	if (!stats)
		return;

	printf("io_timings:\n");
	printf("  io_read: %.3f%%\n", (double)stats->io_read);
	printf("  io_write: %.3f%%\n", (double)stats->io_write);
	printf("  io_cpu: %.3f%%\n", (double)stats->cpu);
}

void test_lwext4_dir_ls(const char *path)
{
	char sss[255];
	ext4_dir d;
	const ext4_direntry *de;

	printf("ls %s\n", path);

	ext4_dir_open(&d, path);
	de = ext4_dir_entry_next(&d);

	while (de) {
		memcpy(sss, de->name, de->name_length);
		sss[de->name_length] = 0;
		printf("  %s%s\n", entry_to_str(de->inode_type), sss);
		de = ext4_dir_entry_next(&d);
	}
	ext4_dir_close(&d);
}

void test_lwext4_mp_stats(void)
{
	struct ext4_mount_stats stats;
	ext4_mount_point_stats("/mp/", &stats);

	printf("********************\n");
	printf("ext4_mount_point_stats\n");
	printf("inodes_count = %" PRIu32 "\n", stats.inodes_count);
	printf("free_inodes_count = %" PRIu32 "\n", stats.free_inodes_count);
	printf("blocks_count = %" PRIu32 "\n", (uint32_t)stats.blocks_count);
	printf("free_blocks_count = %" PRIu32 "\n",
	       (uint32_t)stats.free_blocks_count);
	printf("block_size = %" PRIu32 "\n", stats.block_size);
	printf("block_group_count = %" PRIu32 "\n", stats.block_group_count);
	printf("blocks_per_group= %" PRIu32 "\n", stats.blocks_per_group);
	printf("inodes_per_group = %" PRIu32 "\n", stats.inodes_per_group);
	printf("volume_name = %s\n", stats.volume_name);
	printf("********************\n");
}

void test_lwext4_block_stats(void)
{
	if (!bd)
		return;

	printf("********************\n");
	printf("ext4 blockdev stats\n");
	printf("bdev->bread_ctr = %" PRIu32 "\n", bd->bdif->bread_ctr);
	printf("bdev->bwrite_ctr = %" PRIu32 "\n", bd->bdif->bwrite_ctr);

	printf("bcache->ref_blocks = %" PRIu32 "\n", bd->bc->ref_blocks);
	printf("bcache->max_ref_blocks = %" PRIu32 "\n", bd->bc->max_ref_blocks);
	printf("bcache->lru_ctr = %" PRIu32 "\n", bd->bc->lru_ctr);

	printf("\n");

	printf("********************\n");
}

bool test_lwext4_dir_test(int len)
{
	ext4_file f;
	int r;
	int i;
	char path[64];
	long int diff;
	long int stop;
	long int start;

	printf("test_lwext4_dir_test: %d\n", len);
	io_timings_clear();
	start = get_ms();

	printf("directory create: /mp/dir1\n");
	r = ext4_dir_mk("/mp/dir1");
	if (r != EOK) {
		printf("ext4_dir_mk: rc = %d\n", r);
		return false;
	}

	printf("add files to: /mp/dir1\n");
	for (i = 0; i < len; ++i) {
		sprintf(path, "/mp/dir1/f%d", i);
		r = ext4_fopen(&f, path, "wb");
		if (r != EOK) {
			printf("ext4_fopen: rc = %d\n", r);
			return false;
		}
	}

	stop = get_ms();
	diff = stop - start;
	test_lwext4_dir_ls("/mp/dir1");
	printf("test_lwext4_dir_test: time: %d ms\n", (int)diff);
	printf("test_lwext4_dir_test: av: %d ms/entry\n", (int)diff / (len + 1));
	printf_io_timings(diff);
	return true;
}

static int verify_buf(const unsigned char *b, size_t len, unsigned char c)
{
	size_t i;
	for (i = 0; i < len; ++i) {
		if (b[i] != c)
			return c - b[i];
	}

	return 0;
}

bool test_lwext4_file_test(uint8_t *rw_buff, uint32_t rw_size, uint32_t rw_count)
{
	int r;
	size_t size;
	uint32_t i;
	long int start;
	long int stop;
	long int diff;
	uint32_t kbps;
	uint64_t size_bytes;

	ext4_file f;

	printf("file_test:\n");
	printf("  rw size: %" PRIu32 "\n", rw_size);
	printf("  rw count: %" PRIu32 "\n", rw_count);

	/*Add hello world file.*/
	r = ext4_fopen(&f, "/mp/hello.txt", "wb");
	r = ext4_fwrite(&f, "Hello World !\n", strlen("Hello World !\n"), 0);
	r = ext4_fclose(&f);

	io_timings_clear();
	start = get_ms();
	r = ext4_fopen(&f, "/mp/test1", "wb");
	if (r != EOK) {
		printf("ext4_fopen ERROR = %d\n", r);
		return false;
	}

	printf("ext4_write: %" PRIu32 " * %" PRIu32 " ...\n", rw_size,
	       rw_count);
	for (i = 0; i < rw_count; ++i) {

		memset(rw_buff, i % 10 + '0', rw_size);

		r = ext4_fwrite(&f, rw_buff, rw_size, &size);

		if ((r != EOK) || (size != rw_size))
			break;
	}

	if (i != rw_count) {
		printf("  file_test: rw_count = %" PRIu32 "\n", i);
		return false;
	}

	stop = get_ms();
	diff = stop - start;
	size_bytes = rw_size * rw_count;
	size_bytes = (size_bytes * 1000) / 1024;
	kbps = (size_bytes) / (diff + 1);
	printf("  write time: %d ms\n", (int)diff);
	printf("  write speed: %" PRIu32 " KB/s\n", kbps);
	printf_io_timings(diff);
	r = ext4_fclose(&f);

	io_timings_clear();
	start = get_ms();
	r = ext4_fopen(&f, "/mp/test1", "r+");
	if (r != EOK) {
		printf("ext4_fopen ERROR = %d\n", r);
		return false;
	}

	printf("ext4_read: %" PRIu32 " * %" PRIu32 " ...\n", rw_size, rw_count);

	for (i = 0; i < rw_count; ++i) {
		r = ext4_fread(&f, rw_buff, rw_size, &size);

		if ((r != EOK) || (size != rw_size))
			break;

		if (verify_buf(rw_buff, rw_size, i % 10 + '0'))
			break;
	}

	if (i != rw_count) {
		printf("  file_test: rw_count = %" PRIu32 "\n", i);
		return false;
	}

	stop = get_ms();
	diff = stop - start;
	size_bytes = rw_size * rw_count;
	size_bytes = (size_bytes * 1000) / 1024;
	kbps = (size_bytes) / (diff + 1);
	printf("  read time: %d ms\n", (int)diff);
	printf("  read speed: %d KB/s\n", (int)kbps);
	printf_io_timings(diff);

	r = ext4_fclose(&f);
	return true;
}
void test_lwext4_cleanup(void)
{
	long int start;
	long int stop;
	long int diff;
	int r;

	printf("\ncleanup:\n");
	r = ext4_fremove("/mp/hello.txt");
	if (r != EOK && r != ENOENT) {
		printf("ext4_fremove error: rc = %d\n", r);
	}

	printf("remove /mp/test1\n");
	r = ext4_fremove("/mp/test1");
	if (r != EOK && r != ENOENT) {
		printf("ext4_fremove error: rc = %d\n", r);
	}

	printf("remove /mp/dir1\n");
	io_timings_clear();
	start = get_ms();
	r = ext4_dir_rm("/mp/dir1");
	if (r != EOK && r != ENOENT) {
		printf("ext4_fremove ext4_dir_rm: rc = %d\n", r);
	}
	stop = get_ms();
	diff = stop - start;
	printf("cleanup: time: %d ms\n", (int)diff);
	printf_io_timings(diff);
}

bool test_lwext4_mount(struct ext4_blockdev *bdev, struct ext4_bcache *bcache)
{
	int r;

	bc = bcache;
	bd = bdev;

	if (!bd) {
		printf("test_lwext4_mount: no block device\n");
		return false;
	}

	ext4_dmask_set(DEBUG_ALL);

	r = ext4_device_register(bd, "ext4_fs");
	if (r != EOK) {
		printf("ext4_device_register: rc = %d\n", r);
		return false;
	}

	r = ext4_mount("ext4_fs", "/mp/", false);
	if (r != EOK) {
		printf("ext4_mount: rc = %d\n", r);
		return false;
	}

	r = ext4_recover("/mp/");
	if (r != EOK && r != ENOTSUP) {
		printf("ext4_recover: rc = %d\n", r);
		return false;
	}

	r = ext4_journal_start("/mp/");
	if (r != EOK) {
		printf("ext4_journal_start: rc = %d\n", r);
		return false;
	}

	ext4_cache_write_back("/mp/", 1);
	return true;
}

bool test_lwext4_umount(void)
{
	int r;

	ext4_cache_write_back("/mp/", 0);

	r = ext4_journal_stop("/mp/");
	if (r != EOK) {
		printf("ext4_journal_stop: fail %d", r);
		return false;
	}

	r = ext4_umount("/mp/");
	if (r != EOK) {
		printf("ext4_umount: fail %d", r);
		return false;
	}
	return true;
}
