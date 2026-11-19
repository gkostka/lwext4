/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * lwext4 basic example: the whole life cycle of a filesystem in one
 * program, on a host (Linux, macOS, Windows/MinGW).
 *
 *   lwext4-example-basic <image file>
 *
 * creates <image file> (32 MiB), formats it as ext4, mounts it and does the
 * usual file and directory work. The image is an ordinary ext4 filesystem
 * afterwards: check it with "e2fsck -fn <image file>", look inside with
 * "debugfs -R 'ls -l /docs' <image file>" or loop-mount it on Linux.
 *
 * Everything lwext4 does goes through a block device (struct
 * ext4_blockdev). Here it is the file block device from blockdev/linux
 * (file_dev_get()), which stores the filesystem in a regular file; on a
 * microcontroller you provide your own for the SD card, eMMC, USB stick...
 * (see ../blockdev-template). The rest of this program stays the same.
 */

#include <ext4.h>
#include <ext4_mkfs.h>
#include <blockdev/file_dev.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Size of the image file. 32 MiB of 1 KiB blocks are four complete block
 * groups, which leaves room for a 1024 block journal. */
#define IMAGE_SIZE (32u * 1024 * 1024)

/* Name under which the block device is registered, and where the
 * filesystem appears. All paths passed to lwext4 start with the mount
 * point, which must end in '/'. */
#define DEVICE "ext4_fs"
#define MP "/mp/"

/* Every lwext4 call returns EOK (0) or an errno value. */
#define CHECK(call)                                                            \
	do {                                                                   \
		int r_ = (call);                                               \
		if (r_ != EOK) {                                               \
			fprintf(stderr, "%s:%d: %s failed: %d\n", __FILE__,    \
				__LINE__, #call, r_);                          \
			exit(EXIT_FAILURE);                                    \
		}                                                              \
	} while (0)

/* The "disk": a zero filled file of IMAGE_SIZE bytes. */
static void create_image(const char *path)
{
	FILE *f = fopen(path, "wb");

	if (!f || fseek(f, IMAGE_SIZE - 1, SEEK_SET) || fputc(0, f) == EOF ||
	    fclose(f)) {
		perror(path);
		exit(EXIT_FAILURE);
	}
}

static void write_file(const char *path, const char *text)
{
	ext4_file f;
	size_t written;

	/* ext4_fopen() takes fopen() style modes: "wb" creates or truncates */
	CHECK(ext4_fopen(&f, path, "wb"));
	CHECK(ext4_fwrite(&f, text, strlen(text), &written));
	CHECK(ext4_fclose(&f));
	if (written != strlen(text)) {
		fprintf(stderr, "%s: short write\n", path);
		exit(EXIT_FAILURE);
	}
}

static void print_file(const char *path)
{
	ext4_file f;
	char buf[128];
	size_t n;

	CHECK(ext4_fopen(&f, path, "rb"));
	printf("%s (%llu bytes): ", path,
	       (unsigned long long)ext4_fsize(&f));
	do {
		CHECK(ext4_fread(&f, buf, sizeof(buf), &n));
		fwrite(buf, 1, n, stdout);
	} while (n == sizeof(buf));
	CHECK(ext4_fclose(&f));
}

static void list_dir(const char *path)
{
	const ext4_direntry *de;
	ext4_dir d;

	printf("%s:\n", path);
	CHECK(ext4_dir_open(&d, path));
	/* Entries come in on-disk order, including "." and ".." */
	while ((de = ext4_dir_entry_next(&d)) != NULL)
		printf("  %-5s %.*s\n",
		       de->inode_type == EXT4_DE_DIR ? "dir" : "file",
		       (int)de->name_length, (const char *)de->name);
	CHECK(ext4_dir_close(&d));
}

int main(int argc, char **argv)
{
	/* ext4_mkfs needs a struct ext4_fs to work in. It is large, so keep
	 * it out of the stack. */
	static struct ext4_fs fs;
	struct ext4_mkfs_info info;
	struct ext4_blockdev *bd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <image file>\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* 1. The block device: an image file. file_dev reads and writes
	 *    it in 512 byte sectors. */
	create_image(argv[1]);
	file_dev_name_set(argv[1]);
	bd = file_dev_get();

	/* 2. Format it. Fields left 0 get defaults computed from the device
	 *    size (inode count, journal size, ...). F_SET_EXT2/3/4 selects
	 *    the feature set; ext4 includes a journal. */
	memset(&info, 0, sizeof(info));
	info.block_size = 1024;
	info.journal = true;
	info.label = "lwext4-basic";
	CHECK(ext4_mkfs(&fs, bd, &info, F_SET_EXT4));

	/* 3. Register the device under a name and mount it. Up to
	 *    CONFIG_EXT4_BLOCKDEVS_COUNT devices and
	 *    CONFIG_EXT4_MOUNTPOINTS_COUNT mount points can exist at once. */
	CHECK(ext4_device_register(bd, DEVICE));
	CHECK(ext4_mount(DEVICE, MP, false));

	/* 4. Replay the journal if the last session did not unmount cleanly
	 *    (a no-op here, but always do it before writing), then start
	 *    journaling: from now on metadata updates are transactions that
	 *    survive a power loss. */
	CHECK(ext4_recover(MP));
	CHECK(ext4_journal_start(MP));

	/* 5. Write-back cache: blocks are written when the cache needs room
	 *    or on ext4_cache_flush()/ext4_cache_write_back(.., false)
	 *    instead of after every operation. Much faster on real media;
	 *    must be switched off again before unmounting. */
	CHECK(ext4_cache_write_back(MP, true));

	/* 6. Directories and files. */
	CHECK(ext4_dir_mk(MP "docs"));
	CHECK(ext4_dir_mk(MP "tmp"));
	write_file(MP "docs/hello.txt", "Hello from lwext4!\n");
	write_file(MP "docs/notes.txt", "This file is renamed below.\n");
	write_file(MP "tmp/scratch.txt", "This file is removed below.\n");

	print_file(MP "docs/hello.txt");
	list_dir(MP "docs");

	/* ext4_frename() renames or moves files; ext4_dir_mv() does the
	 * same for directories. */
	CHECK(ext4_frename(MP "docs/notes.txt", MP "docs/readme.txt"));

	/* ext4_fremove() deletes a file, ext4_dir_rm() a directory together
	 * with everything in it. */
	CHECK(ext4_fremove(MP "tmp/scratch.txt"));
	CHECK(ext4_dir_rm(MP "tmp"));

	list_dir(MP "docs");
	list_dir(MP);

	/* 7. Shut down in reverse order: write the cache back, stop the
	 *    journal, unmount, unregister. After ext4_umount() the image is
	 *    consistent and can be checked with e2fsck. */
	CHECK(ext4_cache_write_back(MP, false));
	CHECK(ext4_journal_stop(MP));
	CHECK(ext4_umount(MP));
	CHECK(ext4_device_unregister(DEVICE));

	printf("done: %s\n", argv[1]);
	return EXIT_SUCCESS;
}
