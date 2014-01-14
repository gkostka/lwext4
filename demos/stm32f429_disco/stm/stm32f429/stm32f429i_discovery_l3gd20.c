/**
  ******************************************************************************
  * @file    stm32f429i_discovery_l3gd20.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    28-October-2013
  * @brief   This file provides a set of functions needed to manage the l3gd20
  *          MEMS three-axis digital output gyroscope available on STM32F429I-DISCO Kit.
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
/* Includes ------------------------------------------------------------------*/
#include "stm32f429i_discovery_l3gd20.h"
/** @addtogroup Utilities
  * @{
  */ 

/** @addtogroup STM32F4_DISCOVERY
  * @{
  */ 
  
/** @addtogroup STM32429I_DISCO
  * @{
  */  

/** @addtogroup STM32F429I_DISCOVERY_L3GD20
  * @{
  */


/** @defgroup STM32F429I_DISCOVERY_L3GD20_Private_TypesDefinitions
  * @{
  */

/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_L3GD20_Private_Defines
  * @{
  */

/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_L3GD20_Private_Macros
  * @{
  */

/**
  * @}
  */ 
  
/** @defgroup STM32F429I_DISCOVERY_L3GD20_Private_Variables
  * @{
  */ 
__IO uint32_t  L3GD20Timeout = L3GD20_FLAG_TIMEOUT;  
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_L3GD20_Private_FunctionPrototypes
  * @{
  */
static uint8_t L3GD20_SendByte(uint8_t byte);
static void L3GD20_LowLevel_Init(void);
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_L3GD20_Private_Functions
  * @{
  */

/**
  * @brief  Set L3GD20 Initialization.
  * @param  L3GD20_InitStruct: pointer to a L3GD20_InitTypeDef structure 
  *         that contains the configuration setting for the L3GD20.
  * @retval None
  */
void L3GD20_Init(L3GD20_InitTypeDef *L3GD20_InitStruct)
{  
  uint8_t ctrl1 = 0x00, ctrl4 = 0x00;
  
  /* Configure the low level interface ---------------------------------------*/
  L3GD20_LowLevel_Init();
  
  /* Configure MEMS: data rate, power mode, full scale and axes */
  ctrl1 |= (uint8_t) (L3GD20_InitStruct->Power_Mode | L3GD20_InitStruct->Output_DataRate | \
                    L3GD20_InitStruct->Axes_Enable | L3GD20_InitStruct->Band_Width);
  
  ctrl4 |= (uint8_t) (L3GD20_InitStruct->BlockData_Update | L3GD20_InitStruct->Endianness | \
                    L3GD20_InitStruct->Full_Scale);
                    
  /* Write value to MEMS CTRL_REG1 regsister */
  L3GD20_Write(&ctrl1, L3GD20_CTRL_REG1_ADDR, 1);
  
  /* Write value to MEMS CTRL_REG4 regsister */
  L3GD20_Write(&ctrl4, L3GD20_CTRL_REG4_ADDR, 1);
}

/**
  * @brief  Reboot memory content of L3GD20
  * @param  None
  * @retval None
  */
void L3GD20_RebootCmd(void)
{
  uint8_t tmpreg;
  
  /* Read CTRL_REG5 register */
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG5_ADDR, 1);
  
  /* Enable the reboot memory */
  tmpreg |= L3GD20_BOOT_REBOOTMEMORY;
  
  /* Write value to MEMS CTRL_REG5 regsister */
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG5_ADDR, 1);
}

/**
  * @brief Set L3GD20 Interrupt configuration
  * @param  L3GD20_InterruptConfig_TypeDef: pointer to a L3GD20_InterruptConfig_TypeDef 
  *         structure that contains the configuration setting for the L3GD20 Interrupt.
  * @retval None
  */
void L3GD20_INT1InterruptConfig(L3GD20_InterruptConfigTypeDef *L3GD20_IntConfigStruct)
{
  uint8_t ctrl_cfr = 0x00, ctrl3 = 0x00;
  
  /* Read INT1_CFG register */
  L3GD20_Read(&ctrl_cfr, L3GD20_INT1_CFG_ADDR, 1);
  
  /* Read CTRL_REG3 register */
  L3GD20_Read(&ctrl3, L3GD20_CTRL_REG3_ADDR, 1);
  
  ctrl_cfr &= 0x80;
  
  ctrl3 &= 0xDF;
  
  /* Configure latch Interrupt request and axe interrupts */                   
  ctrl_cfr |= (uint8_t)(L3GD20_IntConfigStruct->Latch_Request| \
                   L3GD20_IntConfigStruct->Interrupt_Axes);
                   
  ctrl3 |= (uint8_t)(L3GD20_IntConfigStruct->Interrupt_ActiveEdge);
  
  /* Write value to MEMS INT1_CFG register */
  L3GD20_Write(&ctrl_cfr, L3GD20_INT1_CFG_ADDR, 1);
  
  /* Write value to MEMS CTRL_REG3 register */
  L3GD20_Write(&ctrl3, L3GD20_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Enable or disable INT1 interrupt
  * @param  InterruptState: State of INT1 Interrupt 
  *      This parameter can be: 
  *        @arg L3GD20_INT1INTERRUPT_DISABLE
  *        @arg L3GD20_INT1INTERRUPT_ENABLE    
  * @retval None
  */
void L3GD20_INT1InterruptCmd(uint8_t InterruptState)
{  
  uint8_t tmpreg;
  
  /* Read CTRL_REG3 register */
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);
                  
  tmpreg &= 0x7F;	
  tmpreg |= InterruptState;
  
  /* Write value to MEMS CTRL_REG3 regsister */
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Enable or disable INT2 interrupt
  * @param  InterruptState: State of INT1 Interrupt 
  *      This parameter can be: 
  *        @arg L3GD20_INT2INTERRUPT_DISABLE
  *        @arg L3GD20_INT2INTERRUPT_ENABLE    
  * @retval None
  */
void L3GD20_INT2InterruptCmd(uint8_t InterruptState)
{  
  uint8_t tmpreg;
  
  /* Read CTRL_REG3 register */
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);
                  
  tmpreg &= 0xF7;	
  tmpreg |= InterruptState;
  
  /* Write value to MEMS CTRL_REG3 regsister */
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Set High Pass Filter Modality
  * @param  L3GD20_FilterStruct: pointer to a L3GD20_FilterConfigTypeDef structure 
  *         that contains the configuration setting for the L3GD20.        
  * @retval None
  */
void L3GD20_FilterConfig(L3GD20_FilterConfigTypeDef *L3GD20_FilterStruct) 
{
  uint8_t tmpreg;
  
  /* Read CTRL_REG2 register */
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG2_ADDR, 1);
  
  tmpreg &= 0xC0;
  
  /* Configure MEMS: mode and cutoff frquency */
  tmpreg |= (uint8_t) (L3GD20_FilterStruct->HighPassFilter_Mode_Selection |\
                      L3GD20_FilterStruct->HighPassFilter_CutOff_Frequency);                             

  /* Write value to MEMS CTRL_REG2 regsister */
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG2_ADDR, 1);
}

/**
  * @brief  Enable or Disable High Pass Filter
  * @param  HighPassFilterState: new state of the High Pass Filter feature.
  *      This parameter can be: 
  *         @arg: L3GD20_HIGHPASSFILTER_DISABLE 
  *         @arg: L3GD20_HIGHPASSFILTER_ENABLE          
  * @retval None
  */
void L3GD20_FilterCmd(uint8_t HighPassFilterState)
 {
  uint8_t tmpreg;
  
  /* Read CTRL_REG5 register */
  L3GD20_Read(&tmpreg, L3GD20_CTRL_REG5_ADDR, 1);
                  
  tmpreg &= 0xEF;

  tmpreg |= HighPassFilterState;

  /* Write value to MEMS CTRL_REG5 regsister */
  L3GD20_Write(&tmpreg, L3GD20_CTRL_REG5_ADDR, 1);
}

/**
  * @brief  Get status for L3GD20 data
  * @param  None         
  * @retval L3GD20 status
  */
uint8_t L3GD20_GetDataStatus(void)
{
  uint8_t tmpreg;
  
  /* Read STATUS_REG register */
  L3GD20_Read(&tmpreg, L3GD20_STATUS_REG_ADDR, 1);
                  
  return tmpreg;
}

/**
  * @brief  Writes a block of data to the L3GD20.
  * @param  pBuffer : pointer to the buffer containing the data to be written to the L3GD20.
  * @param  WriteAddr : L3GD20's internal address to write to.
  * @param  NumByteToWrite: Number of bytes to write.
  * @retval None
  */
void L3GD20_Write(uint8_t* pBuffer, uint8_t WriteAddr, uint16_t NumByteToWrite)
{
  /* Configure the MS bit: 
       - When 0, the address will remain unchanged in multiple read/write commands.
       - When 1, the address will be auto incremented in multiple read/write commands.
  */
  if(NumByteToWrite > 0x01)
  {
    WriteAddr |= (uint8_t)MULTIPLEBYTE_CMD;
  }
  /* Set chip select Low at the start of the transmission */
  L3GD20_CS_LOW();
  
  /* Send the Address of the indexed register */
  L3GD20_SendByte(WriteAddr);

  /* Send the data that will be written into the device (MSB First) */
  while(NumByteToWrite >= 0x01)
  {
    L3GD20_SendByte(*pBuffer);
    NumByteToWrite--;
    pBuffer++;
  }
  
  /* Set chip select High at the end of the transmission */ 
  L3GD20_CS_HIGH();
}

/**
  * @brief  Reads a block of data from the L3GD20.
  * @param  pBuffer : pointer to the buffer that receives the data read from the L3GD20.
  * @param  ReadAddr : L3GD20's internal address to read from.
  * @param  NumByteToRead : number of bytes to read from the L3GD20.
  * @retval None
  */
void L3GD20_Read(uint8_t* pBuffer, uint8_t ReadAddr, uint16_t NumByteToRead)
{  
  if(NumByteToRead > 0x01)
  {
    ReadAddr |= (uint8_t)(READWRITE_CMD | MULTIPLEBYTE_CMD);
  }
  else
  {
    ReadAddr |= (uint8_t)READWRITE_CMD;
  }
  /* Set chip select Low at the start of the transmission */
  L3GD20_CS_LOW();
  
  /* Send the Address of the indexed register */
  L3GD20_SendByte(ReadAddr);
  
  /* Receive the data that will be read from the device (MSB First) */
  while(NumByteToRead > 0x00)
  {
    /* Send dummy byte (0x00) to generate the SPI clock to L3GD20 (Slave device) */
    *pBuffer = L3GD20_SendByte(DUMMY_BYTE);
    NumByteToRead--;
    pBuffer++;
  }
  
  /* Set chip select High at the end of the transmission */ 
  L3GD20_CS_HIGH();
}  

/**
  * @brief  Initializes the low level interface used to drive the L3GD20
  * @param  None
  * @retval None
  */
static void L3GD20_LowLevel_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  SPI_InitTypeDef  SPI_InitStructure;

  /* Enable the SPI periph */
  RCC_APB2PeriphClockCmd(L3GD20_SPI_CLK, ENABLE);

  /* Enable SCK, MOSI and MISO GPIO clocks */
  RCC_AHB1PeriphClockCmd(L3GD20_SPI_SCK_GPIO_CLK | L3GD20_SPI_MISO_GPIO_CLK | L3GD20_SPI_MOSI_GPIO_CLK, ENABLE);

  /* Enable CS GPIO clock */
  RCC_AHB1PeriphClockCmd(L3GD20_SPI_CS_GPIO_CLK, ENABLE);
  
  /* Enable INT1 GPIO clock */
  RCC_AHB1PeriphClockCmd(L3GD20_SPI_INT1_GPIO_CLK, ENABLE);
  
  /* Enable INT2 GPIO clock */
  RCC_AHB1PeriphClockCmd(L3GD20_SPI_INT2_GPIO_CLK, ENABLE);

  GPIO_PinAFConfig(L3GD20_SPI_SCK_GPIO_PORT, L3GD20_SPI_SCK_SOURCE, L3GD20_SPI_SCK_AF);
  GPIO_PinAFConfig(L3GD20_SPI_MISO_GPIO_PORT, L3GD20_SPI_MISO_SOURCE, L3GD20_SPI_MISO_AF);
  GPIO_PinAFConfig(L3GD20_SPI_MOSI_GPIO_PORT, L3GD20_SPI_MOSI_SOURCE, L3GD20_SPI_MOSI_AF);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;

  /* SPI SCK pin configuration */
  GPIO_InitStructure.GPIO_Pin = L3GD20_SPI_SCK_PIN;
  GPIO_Init(L3GD20_SPI_SCK_GPIO_PORT, &GPIO_InitStructure);

  /* SPI  MOSI pin configuration */
  GPIO_InitStructure.GPIO_Pin =  L3GD20_SPI_MOSI_PIN;
  GPIO_Init(L3GD20_SPI_MOSI_GPIO_PORT, &GPIO_InitStructure);

  /* SPI MISO pin configuration */
  GPIO_InitStructure.GPIO_Pin = L3GD20_SPI_MISO_PIN;
  GPIO_Init(L3GD20_SPI_MISO_GPIO_PORT, &GPIO_InitStructure);

  /* SPI configuration -------------------------------------------------------*/
  SPI_I2S_DeInit(L3GD20_SPI);
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  /* SPI baudrate is set to 5.6 MHz (PCLK2/SPI_BaudRatePrescaler = 90/16 = 5.625 MHz) 
     to verify these constraints:
        - ILI9341 LCD SPI interface max baudrate is 10MHz for write and 6.66MHz for read
        - l3gd20 SPI interface max baudrate is 10MHz for write/read
        - PCLK2 frequency is set to 90 MHz 
    */
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(L3GD20_SPI, &SPI_InitStructure);

  /* Enable L3GD20_SPI  */
  SPI_Cmd(L3GD20_SPI, ENABLE);
  
  /* Configure GPIO PIN for Lis Chip select */
  GPIO_InitStructure.GPIO_Pin = L3GD20_SPI_CS_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(L3GD20_SPI_CS_GPIO_PORT, &GPIO_InitStructure);

  /* Deselect : Chip Select high */
  GPIO_SetBits(L3GD20_SPI_CS_GPIO_PORT, L3GD20_SPI_CS_PIN);
  
  /* Configure GPIO PINs to detect Interrupts */
  GPIO_InitStructure.GPIO_Pin = L3GD20_SPI_INT1_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_Init(L3GD20_SPI_INT1_GPIO_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin = L3GD20_SPI_INT2_PIN;
  GPIO_Init(L3GD20_SPI_INT2_GPIO_PORT, &GPIO_InitStructure);
}  

/**
  * @brief  Sends a Byte through the SPI interface and return the Byte received 
  *         from the SPI bus.
  * @param  Byte : Byte send.
  * @retval The received byte value
  */
static uint8_t L3GD20_SendByte(uint8_t byte)
{
  /* Loop while DR register in not empty */
  L3GD20Timeout = L3GD20_FLAG_TIMEOUT;
  while (SPI_I2S_GetFlagStatus(L3GD20_SPI, SPI_I2S_FLAG_TXE) == RESET)
  {
    if((L3GD20Timeout--) == 0) return L3GD20_TIMEOUT_UserCallback();
  }
  
  /* Send a Byte through the SPI peripheral */
  SPI_I2S_SendData(L3GD20_SPI, (uint16_t)byte);
  /* Wait to receive a Byte */
  L3GD20Timeout = L3GD20_FLAG_TIMEOUT;
  while (SPI_I2S_GetFlagStatus(L3GD20_SPI, SPI_I2S_FLAG_RXNE) == RESET)
  {
    if((L3GD20Timeout--) == 0) return L3GD20_TIMEOUT_UserCallback();
  }
  
  /* Return the Byte read from the SPI bus */
  return (uint8_t)SPI_I2S_ReceiveData(L3GD20_SPI);
}

#ifdef USE_DEFAULT_TIMEOUT_CALLBACK
/**
  * @brief  Basic management of the timeout situation.
  * @param  None.
  * @retval None.
  */
uint32_t L3GD20_TIMEOUT_UserCallback(void)
{
  /* Block communication and all processes */
  while (1)
  {   
  }
}
#endif /* USE_DEFAULT_TIMEOUT_CALLBACK */

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
