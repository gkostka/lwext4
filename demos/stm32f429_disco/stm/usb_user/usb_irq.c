

#include "usb_hcd_int.h"
#include "usbh_core.h"
#include "usb_core.h"
#include "usbd_core.h"
#include "usb_conf.h"


extern USB_OTG_CORE_HANDLE           USB_OTG_Core;

/**
  * @brief  TIM2_IRQHandler
  *         This function handles Timer2 Handler.
  * @param  None
  * @retval None
  */

void TIM2_IRQHandler(void)
{
    extern void USB_OTG_BSP_TimerIRQ (void);
  USB_OTG_BSP_TimerIRQ();
}

/**
  * @brief  This function handles OTG_HS Handler.
  * @param  None
  * @retval None
  */
void OTG_HS_IRQHandler(void)
{
    USBH_OTG_ISR_Handler (&USB_OTG_Core);
}



