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

#include "config.h"
#include "stm32f4xx.h"
#include "stm32f429i_discovery_lcd.h"
#include "lcd_log.h"
#include "pll.h"
#include <stdint.h>


uint32_t sEE_TIMEOUT_UserCallback(void)
{
    return 0;
}

int32_t bsp_init (void)
{
    /* Initialize the LEDs */
    STM_EVAL_LEDInit(LED3);
    STM_EVAL_LEDInit(LED4);

    LCD_Init();
    LCD_LayerInit();
    LCD_DisplayOn();
    LTDC_Cmd(ENABLE);
    LCD_SetLayer(LCD_FOREGROUND_LAYER);
    LCD_LOG_Init();

    LCD_LOG_SetHeader((uint8_t *)"STM32F4 LWEXT4 DEMO");

    return 0;
}


int main(void)
{
    volatile uint32_t count, count_max = 10000000;

    pll_init();
    bsp_init();

    int i = 0;
    while (1)
    {
        for (count = 0; count < count_max; count++);
        STM_EVAL_LEDOn(LED3);
        STM_EVAL_LEDOn(LED4);
        for (count = 0; count < count_max; count++);
        STM_EVAL_LEDOff(LED3);
        STM_EVAL_LEDOff(LED4);

        __io_putchar((i++ % 10) + '0');__io_putchar('\n');

    }
}

/**     @} (end addtogroup subgroup)    */
/** @} (end addtogroup group)           */
