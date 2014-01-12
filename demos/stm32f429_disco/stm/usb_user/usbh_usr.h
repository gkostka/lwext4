/**
  ******************************************************************************
  * @file    usbh_usr.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    11-November-2013
  * @brief   Header for usbh_usr module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USH_USR_H__
#define __USH_USR_H__

/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include "usbh_msc_core.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* State Machine for the USBH_USR_ApplicationState */
#define USH_USR_FS_INIT       0
#define USH_USR_FS_READLIST   1
#define USH_USR_FS_WRITEFILE  2
#define USH_USR_FS_DRAW       3

/* Exported macro ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
extern  USBH_Usr_cb_TypeDef USBH_USR_cb;
extern  uint8_t USBH_USR_ApplicationState;
extern  __IO uint32_t USB_Host_Application_Ready;
/* Exported functions ------------------------------------------------------- */
void USBH_USR_ApplicationSelected(void);
void USBH_USR_Init(void);
void USBH_USR_DeInit(void);
void USBH_USR_DeviceAttached(void);
void USBH_USR_ResetDevice(void);
void USBH_USR_DeviceDisconnected (void);
void USBH_USR_OverCurrentDetected (void);
void USBH_USR_DeviceSpeedDetected(uint8_t DeviceSpeed);
void USBH_USR_Device_DescAvailable(void *);
void USBH_USR_DeviceAddressAssigned(void);
void USBH_USR_Configuration_DescAvailable(USBH_CfgDesc_TypeDef * cfgDesc,
    USBH_InterfaceDesc_TypeDef *itfDesc,
    USBH_EpDesc_TypeDef *epDesc);
void USBH_USR_Manufacturer_String(void *);
void USBH_USR_Product_String(void *);
void USBH_USR_SerialNum_String(void *);
void USBH_USR_EnumerationDone(void);
USBH_USR_Status USBH_USR_UserInput(void);
void USBH_USR_DeInit(void);
void USBH_USR_DeviceNotSupported(void);
void USBH_USR_UnrecoveredError(void);
int USBH_USR_MSC_Application(void);
void USBH_USR_BackgroundProcess (void);

#endif /*__USH_USR_H__*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
