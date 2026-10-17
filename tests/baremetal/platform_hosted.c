/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Platform hooks for simulators that provide a C library with a console and
 * exit() (newlib rdimon on qemu-arm, libgloss on the MSP430 GDB simulator).
 */

#include <stdio.h>
#include <stdlib.h>

#include "check.h"

void platform_init(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
}

void platform_exit(int status)
{
	fflush(stdout);
	exit(status);
}
