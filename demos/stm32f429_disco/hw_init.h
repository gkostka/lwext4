/**
 * @file    hw_init.h
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

#ifndef HW_INIT_H_
#define HW_INIT_H_

#include <config.h>
#include <stdbool.h>

void hw_init(void);
void hw_usb_process(void);
bool hw_usb_connected(void);

void hw_led_red(bool on);
void hw_led_green(bool on);


#endif /* HW_INIT_H_ */
