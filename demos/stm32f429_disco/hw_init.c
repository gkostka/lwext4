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
#include <stdint.h>

#include <usbh_core.h>
#include <usbh_msc.h>



#include <hw_init.h>
#include <stdbool.h>

USBH_HandleTypeDef hUSB_Host;


static bool msc_connected = false;
static bool enum_done = false;

static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id)
{
  switch(id)
  {
  case HOST_USER_SELECT_CONFIGURATION:
      break;

  case HOST_USER_DISCONNECTION:
      msc_connected = false;
      enum_done = false;
      break;

  case HOST_USER_CLASS_ACTIVE:
	  enum_done = true;
      break;

  case HOST_USER_CONNECTION:
	  msc_connected = true;
      break;
  default:
      break;
  }
}

#define LCD_FRAME_BUFFER_LAYER0                  (LCD_FRAME_BUFFER+0x130000)
#define LCD_FRAME_BUFFER_LAYER1                  LCD_FRAME_BUFFER
#define CONVERTED_FRAME_BUFFER                   (LCD_FRAME_BUFFER+0x260000)

static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig (&RCC_OscInitStruct);

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}


void hw_init(void)
{
	SystemInit();
    HAL_Init();
    SystemClock_Config();

    /* Initialize the LEDs */
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4);

    SysTick_Config(CFG_CCLK_FREQ / 1000);

    USBH_Init(&hUSB_Host, USBH_UserProcess, 0);
    USBH_RegisterClass(&hUSB_Host, USBH_MSC_CLASS);
    USBH_Start(&hUSB_Host);

    BSP_LCD_Init();

    /* Layer2 Init */
    BSP_LCD_LayerDefaultInit(1, LCD_FRAME_BUFFER_LAYER1);
    BSP_LCD_SelectLayer(1);
    BSP_LCD_Clear(LCD_COLOR_WHITE);
    BSP_LCD_SetColorKeying(1, LCD_COLOR_WHITE);
    BSP_LCD_SetLayerVisible(1, DISABLE);

    /* Layer1 Init */
    BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER_LAYER0);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();
    BSP_LCD_Clear(LCD_COLOR_BLACK);


    LCD_LOG_Init();
    LCD_LOG_SetHeader((uint8_t *)"STM32 LWEXT4 DEMO  ");
}

void hw_usb_process(void)
{
    USBH_Process(&hUSB_Host);
}

bool hw_usb_connected(void)
{
    return msc_connected;
}

bool hw_usb_enum_done(void)
{
    return enum_done;
}

void hw_led_red(bool on)
{
    on ? BSP_LED_On(LED4) : BSP_LED_Off(LED4);
}

void hw_led_green(bool on)
{
    on ? BSP_LED_On(LED3) : BSP_LED_Off(LED3);
}

uint32_t hw_get_ms(void)
{
    return HAL_GetTick();
}

void hw_wait_ms(uint32_t ms)
{
    volatile uint32_t t = HAL_GetTick();

    while((t + ms) > HAL_GetTick())
        ;
}

