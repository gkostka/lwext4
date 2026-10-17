/* SPDX-License-Identifier: BSD-3-Clause */
/* Template block device, see my_blockdev.c. */

#ifndef MY_BLOCKDEV_H_
#define MY_BLOCKDEV_H_

#include <ext4_config.h>
#include <ext4_blockdev.h>

/* The block device, ready for ext4_mkfs() and ext4_device_register(). */
struct ext4_blockdev *my_blockdev_get(void);

#endif /* MY_BLOCKDEV_H_ */
