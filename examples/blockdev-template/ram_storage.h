/* SPDX-License-Identifier: BSD-3-Clause */
/* The RAM disk behind storage.h on a PC, see ram_storage.c. */

#ifndef RAM_STORAGE_H_
#define RAM_STORAGE_H_

/* 16 MiB: room for an ext4 filesystem with a journal of 1024 blocks. */
#define RAM_STORAGE_SIZE (16u * 1024 * 1024)

/* Write the whole RAM disk to a file. Returns 0 on success. */
int ram_storage_save(const char *path);

#endif /* RAM_STORAGE_H_ */
