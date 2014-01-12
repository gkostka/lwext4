/**
 * @file    main.c
 * @version 0.01
 * @date    Oct 2, 2012
 * @author  Grzegorz Kostka, kostka.grzegorz@gmail.com
 * @brief   ...
 *
 * @note
 *
 * @addtogroup group
 * @{
 *      @addtogroup subgroup
 *      @{
 **********************************************************/

#include <config.h>
#include <hw_init.h>

#include <stdio.h>

int main(void)
{
    volatile int count;
    hw_init();

    printf("Connect USB drive...\n");

    while(!hw_usb_connected())
        hw_usb_process();
    printf("USB drive connected\n");

    hw_led_red(1);

    int i = 0;
    while (1)
    {
        for (count = 0; count < 1000000; count++);
        hw_led_green(1);
        for (count = 0; count < 1000000; count++);
        hw_led_green(0);
        printf("%d\n", i++);
    }
}

/**     @} (end addtogroup subgroup)    */
/** @} (end addtogroup group)           */
