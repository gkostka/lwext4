/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Assertions and platform hooks shared by the embedded tests. Every failure
 * prints a line starting with "FAIL" and stops the machine with a non-zero
 * status; the runner also treats a missing "PASS" line as a failure.
 */

#ifndef LWEXT4_TEST_CHECK_H_
#define LWEXT4_TEST_CHECK_H_

#include <ext4_errno.h>

#include <stdio.h>

#ifdef __AVR__
/* Keep the assertion strings in flash, AVR has only a few KiB of RAM. */
#include <avr/pgmspace.h>
#define TEST_STR(s) PSTR(s)
#else
#define TEST_STR(s) (s)
#endif

/**@brief Initialise the console (stdout).*/
void platform_init(void);

/**@brief Stop the (emulated) machine: 0 success, anything else failure.*/
void platform_exit(int status) __attribute__((noreturn));

/**@brief Report a failed check and stop. file/expr are TEST_STR() strings.*/
void test_fail(const char *file, int line, const char *expr, long value)
	__attribute__((noreturn));

/**@brief expr must return EOK.*/
#define CHECK(expr)                                                            \
	do {                                                                   \
		int r_ = (expr);                                               \
		if (r_ != EOK)                                                 \
			test_fail(TEST_STR(__FILE__), __LINE__,                \
				  TEST_STR(#expr), r_);                        \
	} while (0)

#define ASSERT(cond)                                                           \
	do {                                                                   \
		if (!(cond))                                                   \
			test_fail(TEST_STR(__FILE__), __LINE__,                \
				  TEST_STR(#cond), 0);                         \
	} while (0)

/* Test groups, see unit.c, images.c and main.c. */
void unit_tests(void);
void image_tests(void);

#endif /* LWEXT4_TEST_CHECK_H_ */
