/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * A RAM disk implementing storage.h, so that the block device template
 * runs on a PC. On a board, replace this file with your storage driver.
 *
 * ram_storage_save() is not part of the storage interface: main.c uses it
 * to write the RAM disk to a file that e2fsck and debugfs can check.
 */

#include "ram_storage.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>

static uint8_t ram_disk[RAM_STORAGE_SIZE];
static int powered;

int storage_init(void)
{
	powered = 1;
	return 0;
}

uint64_t storage_sector_count(void)
{
	return RAM_STORAGE_SIZE / STORAGE_SECTOR_SIZE;
}

/* Accesses out of range or while powered down fail like a real driver
 * would, so that a bug in the block device shows up as an error. */
static int bad_access(uint64_t sector, uint32_t count)
{
	return !powered || sector > storage_sector_count() ||
	       count > storage_sector_count() - sector;
}

int storage_read(uint64_t sector, void *buf, uint32_t count)
{
	if (bad_access(sector, count))
		return -1;
	memcpy(buf, ram_disk + sector * STORAGE_SECTOR_SIZE,
	       (size_t)count * STORAGE_SECTOR_SIZE);
	return 0;
}

int storage_write(uint64_t sector, const void *buf, uint32_t count)
{
	if (bad_access(sector, count))
		return -1;
	memcpy(ram_disk + sector * STORAGE_SECTOR_SIZE, buf,
	       (size_t)count * STORAGE_SECTOR_SIZE);
	return 0;
}

int storage_sync(void)
{
	return powered ? 0 : -1;
}

void storage_deinit(void)
{
	powered = 0;
}

int ram_storage_save(const char *path)
{
	FILE *f = fopen(path, "wb");

	if (!f)
		return -1;
	if (fwrite(ram_disk, sizeof(ram_disk), 1, f) != 1) {
		fclose(f);
		return -1;
	}
	return fclose(f) ? -1 : 0;
}
