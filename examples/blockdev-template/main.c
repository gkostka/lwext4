/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Runs lwext4 on the template block device (my_blockdev.c, backed by the
 * RAM disk of ram_storage.c):
 *
 *   lwext4-example-blockdev-template <image file>
 *
 * formats the RAM disk, writes a file, unmounts, mounts again and reads the
 * file back, then saves the RAM disk to <image file>, where e2fsck and
 * debugfs can check it (CI does). On a board the same sequence runs on
 * your storage driver.
 */

#include "my_blockdev.h"
#include "ram_storage.h"

#include <ext4.h>
#include <ext4_mkfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(call)                                                            \
	do {                                                                   \
		int r_ = (call);                                               \
		if (r_ != EOK) {                                               \
			fprintf(stderr, "%s:%d: %s failed: %d\n", __FILE__,    \
				__LINE__, #call, r_);                          \
			exit(EXIT_FAILURE);                                    \
		}                                                              \
	} while (0)

static const char text[] = "Written through my_blockdev.\n";

int main(int argc, char **argv)
{
	static struct ext4_fs fs;
	struct ext4_mkfs_info info;
	struct ext4_blockdev *bd = my_blockdev_get();
	char buf[sizeof(text)];
	ext4_file f;
	size_t n;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <image file>\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Format: opens the device (my_open), writes the filesystem and
	 * closes it again (my_close). */
	memset(&info, 0, sizeof(info));
	info.block_size = 1024;
	info.journal = true;
	info.label = "my_blockdev";
	CHECK(ext4_mkfs(&fs, bd, &info, F_SET_EXT4));

	/* Mount and write a file */
	CHECK(ext4_device_register(bd, "my_dev"));
	CHECK(ext4_mount("my_dev", "/", false));
	CHECK(ext4_recover("/"));
	CHECK(ext4_journal_start("/"));
	CHECK(ext4_dir_mk("/data"));
	CHECK(ext4_fopen(&f, "/data/hello.txt", "wb"));
	CHECK(ext4_fwrite(&f, text, strlen(text), &n));
	CHECK(ext4_fclose(&f));
	CHECK(ext4_journal_stop("/"));
	CHECK(ext4_umount("/"));

	/* Mount again (my_open runs again) and read the file back */
	CHECK(ext4_mount("my_dev", "/", true));
	CHECK(ext4_fopen(&f, "/data/hello.txt", "rb"));
	CHECK(ext4_fread(&f, buf, sizeof(buf), &n));
	CHECK(ext4_fclose(&f));
	CHECK(ext4_umount("/"));
	CHECK(ext4_device_unregister("my_dev"));

	if (n != strlen(text) || memcmp(buf, text, n) != 0) {
		fprintf(stderr, "read back %u bytes that differ\n", (unsigned)n);
		return EXIT_FAILURE;
	}
	printf("read back: %s", text);

	if (ram_storage_save(argv[1]) != 0) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}
	printf("done: %s\n", argv[1]);
	return EXIT_SUCCESS;
}
