/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * A symlink target that does not fit into the inode is stored in a data
 * block. Only the target bytes were written, the rest of the block kept
 * whatever it contained before (e.g. data of a deleted file), so the target
 * was not NUL terminated within the block and e2fsck cleared the symlink as
 * invalid. The block must be zero padded.
 */

#include "test_util.h"

#include <ext4_inode.h>

#include <string.h>

#define TARGET                                                                 \
	"/a/symlink/target/that/is/long/enough/to/need/a/data/block/of/its/"   \
	"own/instead/of/the/inode"
#define BLOCK_SIZE 4096

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static unsigned char buf[65536];
	struct ext4_inode inode;
	ext4_file f;
	uint32_t ino, blk;
	size_t n, i;
	FILE *img;

	TEST_ASSERT_EQ(EOK, test_mount(image, false));

	/* Leave non-zero data in free blocks */
	memset(buf, 0xaa, sizeof(buf));
	TEST_ASSERT_EQ(EOK, ext4_fopen(&f, TEST_MP "junk", "wb"));
	TEST_ASSERT_EQ(EOK, ext4_fwrite(&f, buf, sizeof(buf), &n));
	TEST_ASSERT_EQ(sizeof(buf), n);
	TEST_ASSERT_EQ(EOK, ext4_fclose(&f));
	TEST_ASSERT_EQ(EOK, ext4_fremove(TEST_MP "junk"));

	TEST_ASSERT_EQ(EOK, ext4_fsymlink(TARGET, TEST_MP "link"));
	TEST_ASSERT_EQ(EOK, ext4_raw_inode_fill(TEST_MP "link", &ino, &inode));
	blk = ext4_inode_get_direct_block(&inode, 0);
	TEST_ASSERT(blk != 0);
	test_umount();

	img = fopen(image, "rb");
	TEST_ASSERT(img != NULL);
	TEST_ASSERT(fseek(img, (long)blk * BLOCK_SIZE, SEEK_SET) == 0);
	TEST_ASSERT_EQ(BLOCK_SIZE, fread(buf, 1, BLOCK_SIZE, img));
	fclose(img);

	TEST_ASSERT(memcmp(buf, TARGET, strlen(TARGET)) == 0);
	for (i = strlen(TARGET); i < BLOCK_SIZE; i++) {
		if (buf[i]) {
			fprintf(stderr, "symlink block byte %zu is 0x%02x\n", i,
				buf[i]);
			return 1;
		}
	}
	return 0;
}
