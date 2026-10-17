/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * The toolchain files define SIZE, which adds a lib_size target that prints
 * the size of the library after every build. It ran "size liblwext4.a" even
 * for the shared library (LWEXT4_BUILD_SHARED_LIB=ON), so that build always
 * failed. test_shared_lib_size.sh builds the shared library; here its build
 * output has to show the size of the library that was built.
 */

#include "test_util.h"

#include <string.h>

int main(int argc, char **argv)
{
	const char *log = test_image_arg(argc, argv);
	static char buf[1 << 16], out[1 << 16];
	char *line, *next;
	size_t len;
	FILE *f;

	f = fopen(log, "rb");
	TEST_ASSERT(f != NULL);
	len = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[len] = '\0';
	memcpy(out, buf, len + 1);

	/*
	 * size(1) prints a header with "dec" and "hex" columns ("text data bss
	 * dec hex filename" with binutils, "__TEXT __DATA __OBJC others dec
	 * hex" on macOS), then a line of numbers for the library.
	 */
	for (line = buf; line; line = next) {
		next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		if (next && strstr(line, "dec") && strstr(line, "hex")) {
			line = next + strspn(next, " \t");
			if (*line >= '0' && *line <= '9')
				return 0;
		}
	}
	fprintf(stderr, "no size table in the build output:\n%s", out);
	return 1;
}
