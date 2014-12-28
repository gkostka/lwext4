/*
 * spi_lwext4.h
 *
 *  Created on: 21 gru 2014
 *      Author: gko
 */

#ifndef SPI_LWEXT4_H_
#define SPI_LWEXT4_H_

#include <config.h>
#include <ext4_config.h>
#include <ext4_blockdev.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief   SPI cache get.*/
struct ext4_bcache*   spi_cache_get(void);

/**@brief   SPI blockdev get.*/
struct ext4_blockdev* spi_bdev_get(void);

struct ext4_io_stats {
    float io_read;
    float io_write;
    float cpu;
};

void io_timings_clear(void);
const struct ext4_io_stats * io_timings_get(uint32_t time_sum_ms);


#endif /* SPI_LWEXT4_H_ */
