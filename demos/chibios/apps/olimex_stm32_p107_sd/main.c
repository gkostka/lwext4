

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "sdc.h"

#include <spi_lwext4.h>
#include <test_lwext4.h>
#include <timings.h>

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

MMCDriver MMCD1;

static const SPIConfig lscfg = {
        NULL,
        IOPORT1,
        GPIOA_SPI3_CS_MMC,
        SPI_CR1_BR_1 | SPI_CR1_BR_0
};
static const SPIConfig hscfg = {
        NULL,
        IOPORT1,
        GPIOA_SPI3_CS_MMC,
        0
};

static MMCConfig config = {
        &SPID3, &lscfg, &hscfg
};

#define TEST_DELAY_MS    1000

#define DIR_CNT     1000
#define FILE_CNT    1000
#define FILE_SIZE   8192

static void lwext4_tests(void)
{
    printf("lwext4_tests:\n");

    struct ext4_blockdev *bdev = spi_bdev_get();
    struct ext4_bcache *bcache = spi_cache_get();

    tim_wait_ms(TEST_DELAY_MS);
    if(!test_lwext4_mount(bdev, bcache))
        return;

    tim_wait_ms(TEST_DELAY_MS);
    test_lwext4_cleanup();
    tim_wait_ms(TEST_DELAY_MS);

    tim_wait_ms(TEST_DELAY_MS);
    if(!test_lwext4_dir_test(DIR_CNT))
        return;

    tim_wait_ms(TEST_DELAY_MS);
    if(!test_lwext4_file_test(FILE_SIZE, FILE_CNT))
        return;

    //test_lwext4_mp_stats();
    //test_lwext4_block_stats();
    //test_lwext4_cleanup();

    if(!test_lwext4_umount())
        return;

    printf("test finished\n\n");
}


int main(void)
{
    halInit();
    chSysInit();
    sdStart(&STDOUT_SD, NULL);

    mmcObjectInit(&MMCD1);
    mmcStart(&MMCD1, &config);

    printf("\n\n\n\n\nboard: %s\n", BOARD_NAME);
    lwext4_tests();

    while (TRUE) {
        chThdSleepMilliseconds(500);
    }
}

