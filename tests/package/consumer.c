/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * A program built against an *installed* lwext4 (see install_package.sh):
 * formats an image file with ext4_mkfs through the file block device of
 * libblockdev, mounts it, writes a file and reads it back.
 *
 * Only the installed headers are used: <ext4.h>, <ext4_mkfs.h> and
 * <blockdev/file_dev.h>.
 */

#include <ext4.h>
#include <ext4_mkfs.h>
#include <blockdev/file_dev.h>

#include <stdio.h>
#include <string.h>

#define IMAGE_SIZE (8 * 1024 * 1024)

#define CHECK(call)                                                            \
	do {                                                                   \
		int r_ = (call);                                               \
		if (r_ != EOK) {                                               \
			fprintf(stderr, "%s: error %d\n", #call, r_);          \
			return 1;                                              \
		}                                                              \
	} while (0)

static int create_image(const char *path)
{
	FILE *f = fopen(path, "wb");

	if (!f)
		return 1;
	if (fseek(f, IMAGE_SIZE - 1, SEEK_SET) || fputc(0, f) == EOF) {
		fclose(f);
		return 1;
	}
	return fclose(f) ? 1 : 0;
}

int main(int argc, char **argv)
{
	static struct ext4_fs fs;
	struct ext4_mkfs_info info;
	struct ext4_blockdev *bd;
	const char msg[] = "installed lwext4 works\n";
	char buf[64];
	ext4_file f;
	size_t n;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <image file>\n", argv[0]);
		return 2;
	}
	if (create_image(argv[1])) {
		perror(argv[1]);
		return 1;
	}

	file_dev_name_set(argv[1]);
	bd = file_dev_get();

	memset(&info, 0, sizeof(info));
	info.block_size = 1024;
	info.journal = true;
	CHECK(ext4_mkfs(&fs, bd, &info, F_SET_EXT4));

	CHECK(ext4_device_register(bd, "dev"));
	CHECK(ext4_mount("dev", "/mp/", false));

	CHECK(ext4_fopen(&f, "/mp/hello.txt", "wb"));
	CHECK(ext4_fwrite(&f, msg, sizeof(msg) - 1, &n));
	CHECK(ext4_fclose(&f));

	CHECK(ext4_fopen(&f, "/mp/hello.txt", "rb"));
	CHECK(ext4_fread(&f, buf, sizeof(buf), &n));
	CHECK(ext4_fclose(&f));

	CHECK(ext4_umount("/mp/"));
	CHECK(ext4_device_unregister("dev"));

	if (n != sizeof(msg) - 1 || memcmp(buf, msg, n) != 0) {
		fprintf(stderr, "read back %u bytes that differ\n", (unsigned)n);
		return 1;
	}
	fwrite(buf, 1, n, stdout);
	return 0;
}
