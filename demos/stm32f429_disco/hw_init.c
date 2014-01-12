/**
 * @file    hw_init.c
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
#include <stm32f4xx.h>
#include <stm32f429i_discovery_lcd.h>
#include <lcd_log.h>
#include <pll.h>
#include <stdint.h>

#include <usb_core.h>
#include <usbh_core.h>
#include <usbh_msc_core.h>
#include <usbh_usr.h>

#include <pll.h>
#include <hw_init.h>

USB_OTG_CORE_HANDLE     USB_OTG_Core;
USBH_HOST               USB_Host;

static volatile uint32_t _systick_;

void SysTick_Handler(void)
{
    _systick_++;
}

void hw_init(void)
{
    pll_init();

    /* Initialize the LEDs */
    STM_EVAL_LEDInit(LED3);
    STM_EVAL_LEDInit(LED4);

    SysTick_Config(CFG_CCLK_FREQ / 1000);

    /*Init USB Host */
    USBH_Init(&USB_OTG_Core,
        USB_OTG_HS_CORE_ID,
        &USB_Host,
        &USBH_MSC_cb,
        &USBH_USR_cb);

    LCD_Init();
    LCD_LayerInit();
    LCD_DisplayOn();
    LTDC_Cmd(ENABLE);
    LCD_SetLayer(LCD_FOREGROUND_LAYER);
    LCD_LOG_Init();

    LCD_LOG_SetHeader((uint8_t *)"LWEXT4 DEMO");
}

void hw_usb_process(void)
{
    USBH_Process(&USB_OTG_Core, &USB_Host);
}

bool hw_usb_connected(void)
{
    return HCD_IsDeviceConnected(&USB_OTG_Core);
}

bool hw_usb_enum_done(void)
{
    return USB_Host_Application_Ready;
}

void hw_led_red(bool on)
{
    on ? STM_EVAL_LEDOn(LED4) : STM_EVAL_LEDOff(LED4);
}

void hw_led_green(bool on)
{
    on ? STM_EVAL_LEDOn(LED3) : STM_EVAL_LEDOff(LED3);
}

uint32_t hw_get_ms(void)
{
    return _systick_;
}
