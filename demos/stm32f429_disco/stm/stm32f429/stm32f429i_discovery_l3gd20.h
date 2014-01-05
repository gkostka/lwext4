/**
  ******************************************************************************
  * @file    stm32f429i_discovery_l3gd20.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    28-October-2013
  * @brief   This file contains definitions for stm32f429i_discovery_l3gd20.c 
  *          firmware driver.
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
#ifndef __STM32F429I_DISCOVERY_L3GD20_H
#define __STM32F429I_DISCOVERY_L3GD20_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f429i_discovery.h"

/** @addtogroup Utilities
   * @{
   */
   
/** @addtogroup STM32F4_DISCOVERY
  * @{
  */ 

/** @addtogroup STM32F429I_DISCOVERY
  * @{
  */
  
/** @addtogroup STM32F429I_DISCOVERY_L3GD20
  * @{
  */
  
/** @defgroup STM32F429I_DISCOVERY_L3GD20_Exported_Types
  * @{
  */

/* L3GD20 struct */
typedef struct
{
  uint8_t Power_Mode;                         /* Power-down/Sleep/Normal Mode */
  uint8_t Output_DataRate;                    /* OUT data rate */
  uint8_t Axes_Enable;                        /* Axes enable */
  uint8_t Band_Width;                         /* Bandwidth selection */
  uint8_t BlockData_Update;                   /* Block Data Update */
  uint8_t Endianness;                         /* Endian Data selection */
  uint8_t Full_Scale;                         /* Full Scale selection */
}L3GD20_InitTypeDef;

/* L3GD20 High Pass Filter struct */
typedef struct
{
  uint8_t HighPassFilter_Mode_Selection;      /* Internal filter mode */
  uint8_t HighPassFilter_CutOff_Frequency;    /* High pass filter cut-off frequency */
}L3GD20_FilterConfigTypeDef;

/* L3GD20 Interrupt struct */
typedef struct
{
  uint8_t Latch_Request;                      /* Latch interrupt request into CLICK_SRC register */
  uint8_t Interrupt_Axes;                     /* X, Y, Z Axes Interrupts */ 
  uint8_t Interrupt_ActiveEdge;               /*  Interrupt Active edge */
}L3GD20_InterruptConfigTypeDef;  

/**
  * @}
  */ 


/**
  * @}
  */
  
/** @defgroup STM32F429I-DISCO_L3GD20_Exported_Constants
  * @{
  */

/* Read/Write command */
#define READWRITE_CMD              ((uint8_t)0x80) 
/* Multiple byte read/write command */ 
#define MULTIPLEBYTE_CMD           ((uint8_t)0x40)
/* Dummy Byte Send by the SPI Master device in order to generate the Clock to the Slave device */
#define DUMMY_BYTE                 ((uint8_t)0x00)

/* Uncomment the following line to use the default L3GD20_TIMEOUT_UserCallback() 
   function implemented in stm32f429i_discovery_lgd20.c file.
   L3GD20_TIMEOUT_UserCallback() function is called whenever a timeout condition 
   occure during communication (waiting transmit data register empty flag(TXE)
   or waiting receive data register is not empty flag (RXNE)). */   
/* #define USE_DEFAULT_TIMEOUT_CALLBACK */

/* Maximum Timeout values for flags waiting loops. These timeouts are not based
   on accurate values, they just guarantee that the application will not remain
   stuck if the SPI communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */   
#define L3GD20_FLAG_TIMEOUT             ((uint32_t)0x1000)

/**
  * @brief  L3GD20 SPI Interface pins
  */
#define L3GD20_SPI                       SPI5
#define L3GD20_SPI_CLK                   RCC_APB2Periph_SPI5

#define L3GD20_SPI_SCK_PIN               GPIO_Pin_7                  /* PF.07 */
#define L3GD20_SPI_SCK_GPIO_PORT         GPIOF                       /* GPIOF */
#define L3GD20_SPI_SCK_GPIO_CLK          RCC_AHB1Periph_GPIOF
#define L3GD20_SPI_SCK_SOURCE            GPIO_PinSource7
#define L3GD20_SPI_SCK_AF                GPIO_AF_SPI5

#define L3GD20_SPI_MISO_PIN              GPIO_Pin_8                  /* PF.08 */
#define L3GD20_SPI_MISO_GPIO_PORT        GPIOF                       /* GPIOF */
#define L3GD20_SPI_MISO_GPIO_CLK         RCC_AHB1Periph_GPIOF
#define L3GD20_SPI_MISO_SOURCE           GPIO_PinSource8
#define L3GD20_SPI_MISO_AF               GPIO_AF_SPI5

#define L3GD20_SPI_MOSI_PIN              GPIO_Pin_9                  /* PF.09 */
#define L3GD20_SPI_MOSI_GPIO_PORT        GPIOF                       /* GPIOF */
#define L3GD20_SPI_MOSI_GPIO_CLK         RCC_AHB1Periph_GPIOF
#define L3GD20_SPI_MOSI_SOURCE           GPIO_PinSource9
#define L3GD20_SPI_MOSI_AF               GPIO_AF_SPI5

#define L3GD20_SPI_CS_PIN                GPIO_Pin_1                  /* PC.01 */
#define L3GD20_SPI_CS_GPIO_PORT          GPIOC                       /* GPIOC */
#define L3GD20_SPI_CS_GPIO_CLK           RCC_AHB1Periph_GPIOC

#define L3GD20_SPI_INT1_PIN              GPIO_Pin_1                  /* PA.01 */
#define L3GD20_SPI_INT1_GPIO_PORT        GPIOA                       /* GPIOA */
#define L3GD20_SPI_INT1_GPIO_CLK         RCC_AHB1Periph_GPIOA
#define L3GD20_SPI_INT1_EXTI_LINE        EXTI_Line1
#define L3GD20_SPI_INT1_EXTI_PORT_SOURCE EXTI_PortSourceGPIOA
#define L3GD20_SPI_INT1_EXTI_PIN_SOURCE  EXTI_PinSource1
#define L3GD20_SPI_INT1_EXTI_IRQn        EXTI1_IRQn 

#define L3GD20_SPI_INT2_PIN              GPIO_Pin_2                  /* PA.02 */
#define L3GD20_SPI_INT2_GPIO_PORT        GPIOA                       /* GPIOA */
#define L3GD20_SPI_INT2_GPIO_CLK         RCC_AHB1Periph_GPIOA
#define L3GD20_SPI_INT2_EXTI_LINE        EXTI_Line2
#define L3GD20_SPI_INT2_EXTI_PORT_SOURCE EXTI_PortSourceGPIOA
#define L3GD20_SPI_INT2_EXTI_PIN_SOURCE  EXTI_PinSource2
#define L3GD20_SPI_INT2_EXTI_IRQn        EXTI2_IRQn 

/******************************************************************************/
/*************************** START REGISTER MAPPING  **************************/
/******************************************************************************/
#define L3GD20_WHO_AM_I_ADDR          0x0F  /* device identification register */
#define L3GD20_CTRL_REG1_ADDR         0x20  /* Control register 1 */
#define L3GD20_CTRL_REG2_ADDR         0x21  /* Control register 2 */
#define L3GD20_CTRL_REG3_ADDR         0x22  /* Control register 3 */
#define L3GD20_CTRL_REG4_ADDR         0x23  /* Control register 4 */
#define L3GD20_CTRL_REG5_ADDR         0x24  /* Control register 5 */
#define L3GD20_REFERENCE_REG_ADDR     0x25  /* Reference register */
#define L3GD20_OUT_TEMP_ADDR          0x26  /* Out temp register */
#define L3GD20_STATUS_REG_ADDR        0x27  /* Status register */
#define L3GD20_OUT_X_L_ADDR           0x28  /* Output Register X */
#define L3GD20_OUT_X_H_ADDR           0x29  /* Output Register X */
#define L3GD20_OUT_Y_L_ADDR           0x2A  /* Output Register Y */
#define L3GD20_OUT_Y_H_ADDR           0x2B  /* Output Register Y */
#define L3GD20_OUT_Z_L_ADDR           0x2C  /* Output Register Z */
#define L3GD20_OUT_Z_H_ADDR           0x2D  /* Output Register Z */ 
#define L3GD20_FIFO_CTRL_REG_ADDR     0x2E  /* Fifo control Register */
#define L3GD20_FIFO_SRC_REG_ADDR      0x2F  /* Fifo src Register */

#define L3GD20_INT1_CFG_ADDR          0x30  /* Interrupt 1 configuration Register */
#define L3GD20_INT1_SRC_ADDR          0x31  /* Interrupt 1 source Register */
#define L3GD20_INT1_TSH_XH_ADDR       0x32  /* Interrupt 1 Threshold X register */
#define L3GD20_INT1_TSH_XL_ADDR       0x33  /* Interrupt 1 Threshold X register */
#define L3GD20_INT1_TSH_YH_ADDR       0x34  /* Interrupt 1 Threshold Y register */
#define L3GD20_INT1_TSH_YL_ADDR       0x35  /* Interrupt 1 Threshold Y register */
#define L3GD20_INT1_TSH_ZH_ADDR       0x36  /* Interrupt 1 Threshold Z register */
#define L3GD20_INT1_TSH_ZL_ADDR       0x37  /* Interrupt 1 Threshold Z register */
#define L3GD20_INT1_DURATION_ADDR     0x38  /* Interrupt 1 DURATION register */

/******************************************************************************/
/**************************** END REGISTER MAPPING  ***************************/
/******************************************************************************/

#define I_AM_L3GD20		    ((uint8_t)0xD4)

/** @defgroup Power_Mode_selection 
  * @{
  */
#define L3GD20_MODE_POWERDOWN       ((uint8_t)0x00)
#define L3GD20_MODE_ACTIVE          ((uint8_t)0x08)
/**
  * @}
  */

/** @defgroup OutPut_DataRate_Selection 
  * @{
  */
#define L3GD20_OUTPUT_DATARATE_1    ((uint8_t)0x00)
#define L3GD20_OUTPUT_DATARATE_2    ((uint8_t)0x40)
#define L3GD20_OUTPUT_DATARATE_3    ((uint8_t)0x80)
#define L3GD20_OUTPUT_DATARATE_4    ((uint8_t)0xC0)
/**
  * @}
  */

/** @defgroup Axes_Selection 
  * @{
  */
#define L3GD20_X_ENABLE            ((uint8_t)0x02)
#define L3GD20_Y_ENABLE            ((uint8_t)0x01)
#define L3GD20_Z_ENABLE            ((uint8_t)0x04)
#define L3GD20_AXES_ENABLE         ((uint8_t)0x07)
#define L3GD20_AXES_DISABLE        ((uint8_t)0x00)
/**
  * @}
  */

/** @defgroup BandWidth_Selection 
  * @{
  */
#define L3GD20_BANDWIDTH_1         ((uint8_t)0x00)
#define L3GD20_BANDWIDTH_2         ((uint8_t)0x10)
#define L3GD20_BANDWIDTH_3         ((uint8_t)0x20)
#define L3GD20_BANDWIDTH_4         ((uint8_t)0x30)
/**
  * @}
  */

/** @defgroup Full_Scale_Selection 
  * @{
  */
#define L3GD20_FULLSCALE_250               ((uint8_t)0x00)
#define L3GD20_FULLSCALE_500               ((uint8_t)0x10)
#define L3GD20_FULLSCALE_2000              ((uint8_t)0x20) 
/**
  * @}
  */
  
/** @defgroup Block_Data_Update 
  * @{
  */  
#define L3GD20_BlockDataUpdate_Continous   ((uint8_t)0x00)
#define L3GD20_BlockDataUpdate_Single      ((uint8_t)0x80)
/**
  * @}
  */
  
/** @defgroup Endian_Data_selection
  * @{
  */  
#define L3GD20_BLE_LSB                     ((uint8_t)0x00)
#define L3GD20_BLE_MSB	                   ((uint8_t)0x40)
/**
  * @}
  */
  
/** @defgroup High_Pass_Filter_status 
  * @{
  */   
#define L3GD20_HIGHPASSFILTER_DISABLE      ((uint8_t)0x00)
#define L3GD20_HIGHPASSFILTER_ENABLE	     ((uint8_t)0x10)
/**
  * @}
  */

/** @defgroup INT1_Interrupt_status 
  * @{
  */   
#define L3GD20_INT1INTERRUPT_DISABLE       ((uint8_t)0x00)
#define L3GD20_INT1INTERRUPT_ENABLE        ((uint8_t)0x80)
/**
  * @}
  */

/** @defgroup INT2_Interrupt_status 
  * @{
  */   
#define L3GD20_INT2INTERRUPT_DISABLE       ((uint8_t)0x00)
#define L3GD20_INT2INTERRUPT_ENABLE        ((uint8_t)0x08)
/**
  * @}
  */

/** @defgroup INT1_Interrupt_ActiveEdge 
  * @{
  */   
#define L3GD20_INT1INTERRUPT_LOW_EDGE      ((uint8_t)0x20)
#define L3GD20_INT1INTERRUPT_HIGH_EDGE     ((uint8_t)0x00)
/**
  * @}
  */
  
/** @defgroup Boot_Mode_selection 
  * @{
  */
#define L3GD20_BOOT_NORMALMODE             ((uint8_t)0x00)
#define L3GD20_BOOT_REBOOTMEMORY           ((uint8_t)0x80)
/**
  * @}
  */  
 
/** @defgroup High_Pass_Filter_Mode 
  * @{
  */   
#define L3GD20_HPM_NORMAL_MODE_RES         ((uint8_t)0x00)
#define L3GD20_HPM_REF_SIGNAL              ((uint8_t)0x10)
#define L3GD20_HPM_NORMAL_MODE             ((uint8_t)0x20)
#define L3GD20_HPM_AUTORESET_INT           ((uint8_t)0x30)
/**
  * @}
  */

/** @defgroup High_Pass_CUT OFF_Frequency 
  * @{
  */   
#define L3GD20_HPFCF_0              0x00
#define L3GD20_HPFCF_1              0x01
#define L3GD20_HPFCF_2              0x02
#define L3GD20_HPFCF_3              0x03
#define L3GD20_HPFCF_4              0x04
#define L3GD20_HPFCF_5              0x05
#define L3GD20_HPFCF_6              0x06
#define L3GD20_HPFCF_7              0x07
#define L3GD20_HPFCF_8              0x08
#define L3GD20_HPFCF_9              0x09
/**
  * @}
  */


/** @defgroup STM32F429I-DISCO_L3GD20_Exported_Macros
  * @{
  */
#define L3GD20_CS_LOW()       GPIO_ResetBits(L3GD20_SPI_CS_GPIO_PORT, L3GD20_SPI_CS_PIN)
#define L3GD20_CS_HIGH()      GPIO_SetBits(L3GD20_SPI_CS_GPIO_PORT, L3GD20_SPI_CS_PIN)
/**
  * @}
  */
 
/** @defgroup STM32F429I-DISCO_L3GD20_Exported_Functions
  * @{
  */
/* Sensor Configuration Functions */ 
void L3GD20_Init(L3GD20_InitTypeDef *L3GD20_InitStruct);
void L3GD20_RebootCmd(void);

/*INT1 Interrupt Configuration Functions */
void L3GD20_INT1InterruptCmd(uint8_t InterruptState);
void L3GD20_INT2InterruptCmd(uint8_t InterruptState);
void L3GD20_INT1InterruptConfig(L3GD20_InterruptConfigTypeDef *L3GD20_IntConfigStruct);
uint8_t L3GD20_GetDataStatus(void);

/* High Pass Filter Configuration Functions */
void L3GD20_FilterConfig(L3GD20_FilterConfigTypeDef *L3GD20_FilterStruct);
void L3GD20_FilterCmd(uint8_t HighPassFilterState);
void L3GD20_Write(uint8_t* pBuffer, uint8_t WriteAddr, uint16_t NumByteToWrite);
void L3GD20_Read(uint8_t* pBuffer, uint8_t ReadAddr, uint16_t NumByteToRead);

/* USER Callbacks: This is function for which prototype only is declared in
   MEMS accelerometre driver and that should be implemented into user applicaiton. */  
/* L3GD20_TIMEOUT_UserCallback() function is called whenever a timeout condition 
   occure during communication (waiting transmit data register empty flag(TXE)
   or waiting receive data register is not empty flag (RXNE)).
   You can use the default timeout callback implementation by uncommenting the 
   define USE_DEFAULT_TIMEOUT_CALLBACK in stm32f429i_discovery_l3gd20.h file.
   Typically the user implementation of this callback should reset MEMS peripheral
   and re-initialize communication or in worst case reset all the application. */
uint32_t L3GD20_TIMEOUT_UserCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F429I_DISCOVERY_L3GD20_H */
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */ 

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/ 
