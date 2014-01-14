/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

    LCD_LOG_SetHeader((uint8_t *)"STM32 LWEXT4 DEMO");
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

void hw_wait_ms(uint32_t ms)
{
    volatile uint32_t t = _systick_;

    while((t + ms) > _systick_)
        ;
}
