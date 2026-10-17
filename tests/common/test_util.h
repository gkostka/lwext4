/*
 * Minimal helpers shared by the regression tests in this directory.
 */

#ifndef LWEXT4_TEST_UTIL_H_
#define LWEXT4_TEST_UTIL_H_

#include <ext4.h>

#include <stdio.h>
#include <stdlib.h>

#define TEST_DEV "test_dev"
#define TEST_MP "/mp/"

#define TEST_ASSERT(cond)                                                      \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "%s:%d: assertion failed: %s\n",       \
				__FILE__, __LINE__, #cond);                    \
			exit(1);                                               \
		}                                                              \
	} while (0)

#define TEST_ASSERT_EQ(expected, actual)                                       \
	do {                                                                   \
		long long e_ = (long long)(expected);                          \
		long long a_ = (long long)(actual);                            \
		if (e_ != a_) {                                                \
			fprintf(stderr,                                        \
				"%s:%d: %s == %s failed: expected %lld, "      \
				"got %lld\n",                                  \
				__FILE__, __LINE__, #expected, #actual, e_,    \
				a_);                                           \
			exit(1);                                               \
		}                                                              \
	} while (0)

/**@brief Register the image file as TEST_DEV and mount it at TEST_MP.
 * @return result of ext4_mount (device registration failures abort).*/
int test_mount(const char *image, bool read_only);

/**@brief Unmount TEST_MP and unregister TEST_DEV.*/
void test_umount(void);

/**@brief Return the image path from argv or abort with a usage message.*/
const char *test_image_arg(int argc, char **argv);

#endif /* LWEXT4_TEST_UTIL_H_ */
