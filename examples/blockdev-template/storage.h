/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * The storage driver interface the block device template
 * (my_blockdev.c) is written against: whatever your hardware offers (SD
 * card over SPI/SDIO, eMMC, USB mass storage, NAND behind an FTL, a disk
 * image in RAM...) boils down to reading and writing whole sectors.
 *
 * ram_storage.c implements it with a buffer in RAM, so the template can be
 * built and tested on a PC. On your board, implement these functions with
 * your driver instead (or call the driver directly from my_blockdev.c).
 */

#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdint.h>

/* The sector size of the medium: the unit storage_read()/storage_write()
 * transfer. 512 for SD cards and most disks. It must be a power of two
 * and not larger than the filesystem block size (1024..65536 bytes). */
#define STORAGE_SECTOR_SIZE 512

/* Bring the hardware up (clocks, pins, card identification...).
 * Returns 0 on success. */
int storage_init(void);

/* Number of STORAGE_SECTOR_SIZE sectors on the medium. */
uint64_t storage_sector_count(void);

/* Transfer count consecutive sectors starting at sector. Return 0 on
 * success, anything else on an I/O error. */
int storage_read(uint64_t sector, void *buf, uint32_t count);
int storage_write(uint64_t sector, const void *buf, uint32_t count);

/* Make sure everything written so far is on the medium (drain write
 * buffers, wait for the card to finish programming). Returns 0 on
 * success. */
int storage_sync(void);

/* Power the hardware down. */
void storage_deinit(void);

#endif /* STORAGE_H_ */
