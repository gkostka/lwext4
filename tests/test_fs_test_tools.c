/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Command line behaviour of the fs_test tools README.md documents:
 *
 *  - "lwext4-generic --help" and "lwext4-mkfs --help" (README: "Show full
 *    option set") print the usage and succeed; so do lwext4-mbr,
 *    lwext4-server and lwext4-client. They used to reject --help as an
 *    unknown option and exit with an error.
 *  - lwext4-generic's usage lists -w twice (for --rw_size and --wpart);
 *    --rw_size is -s.
 *  - "lwext4-mkfs -e 2" creates ext2, i.e. a filesystem without a journal
 *    (it created one: has_journal, which makes it ext3).
 *  - lwext4-generic prints library debug output only with --verbose.
 *
 * The tools are run from the build tree (<build>/fs_test, next to this
 * test's <build>/tests). When they cannot be executed (cross builds run
 * under an emulator) the test is skipped.
 */

#include "test_util.h"

#include <ext4_misc.h>
#include <ext4_types.h>

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char tools[PATH_MAX];
static char out[PATH_MAX];

/* Run a tool with its output in `out`, return its exit status. */
static int run(const char *tool, const char *args)
{
	char cmd[3 * PATH_MAX];
	int st;

	snprintf(cmd, sizeof(cmd), "'%s/%s' %s >'%s' 2>&1", tools, tool, args,
		 out);
	st = system(cmd);
	TEST_ASSERT(st != -1 && WIFEXITED(st));
	return WEXITSTATUS(st);
}

static bool output_has(const char *s)
{
	char line[512];
	bool found = false;
	FILE *f = fopen(out, "r");

	TEST_ASSERT(f != NULL);
	while (!found && fgets(line, sizeof(line), f))
		found = strstr(line, s) != NULL;
	fclose(f);
	return found;
}

static int count_lines_with(const char *s)
{
	char line[512];
	int n = 0;
	FILE *f = fopen(out, "r");

	TEST_ASSERT(f != NULL);
	while (fgets(line, sizeof(line), f))
		if (strstr(line, s))
			n++;
	fclose(f);
	return n;
}

int main(int argc, char **argv)
{
	const char *image = test_image_arg(argc, argv);
	static const char *const all[] = {"lwext4-generic", "lwext4-mkfs",
					  "lwext4-mbr", "lwext4-server",
					  "lwext4-client"};
	struct ext4_sblock sb;
	char self[PATH_MAX], args[PATH_MAX + 64];
	FILE *img;
	size_t i;

	snprintf(self, sizeof(self), "%s", argv[0]);
	snprintf(tools, sizeof(tools), "%s/../fs_test", dirname(self));
	snprintf(out, sizeof(out), "%s.out", image);

	snprintf(args, sizeof(args), "%s/lwext4-mkfs", tools);
	if (access(args, X_OK) != 0 || run("lwext4-mkfs", "--version") != 0) {
		printf("fs_test tools not runnable here, skipped\n");
		return 0;
	}

	for (i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
		TEST_ASSERT_EQ(0, run(all[i], "--help"));
		TEST_ASSERT(output_has("Usage"));
		TEST_ASSERT_EQ(0, run(all[i], "-h"));
		TEST_ASSERT(output_has("Usage"));
	}

	TEST_ASSERT_EQ(0, run("lwext4-generic", "--help"));
	TEST_ASSERT(output_has("[-s] --rw_size"));
	TEST_ASSERT_EQ(1, count_lines_with("[-w]"));
	TEST_ASSERT(output_has("--verbose"));

	/* lwext4-mkfs -e 2: no journal */
	snprintf(args, sizeof(args), "-i '%s' -e 2", image);
	TEST_ASSERT_EQ(0, run("lwext4-mkfs", args));
	img = fopen(image, "rb");
	TEST_ASSERT(img != NULL);
	TEST_ASSERT(fseek(img, 1024, SEEK_SET) == 0);
	TEST_ASSERT_EQ(1, fread(&sb, sizeof(sb), 1, img));
	fclose(img);
	TEST_ASSERT_EQ(0, to_le32(sb.features_compatible) &
			      EXT4_FCOM_HAS_JOURNAL);

	/* debug output ("ext4_...: l: <line>") only with --verbose */
	snprintf(args, sizeof(args), "-i '%s' -c 1 -s 4096", image);
	TEST_ASSERT_EQ(0, run("lwext4-generic", args));
	TEST_ASSERT(!output_has(" l: "));
	snprintf(args, sizeof(args), "-i '%s' -c 1 -s 4096 --verbose", image);
	TEST_ASSERT_EQ(0, run("lwext4-generic", args));
	TEST_ASSERT(output_has(" l: "));
	return 0;
}
