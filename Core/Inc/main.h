/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SMSN_FM_CS_OBC_Pin GPIO_PIN_6
#define SMSN_FM_CS_OBC_GPIO_Port GPIOI
#define MSN_FM_MODE_Pin GPIO_PIN_7
#define MSN_FM_MODE_GPIO_Port GPIOI
#define SMSN_FM_SCK_OBC_Pin GPIO_PIN_2
#define SMSN_FM_SCK_OBC_GPIO_Port GPIOE
#define OBC_RX_COM_TX_Pin GPIO_PIN_7
#define OBC_RX_COM_TX_GPIO_Port GPIOB
#define OBC_RX_COM_TXD6_Pin GPIO_PIN_6
#define OBC_RX_COM_TXD6_GPIO_Port GPIOD
#define OBC_RX_1_Pin GPIO_PIN_11
#define OBC_RX_1_GPIO_Port GPIOC
#define MUX_EN_Pin GPIO_PIN_0
#define MUX_EN_GPIO_Port GPIOI
#define EN_5V_DCDC_Pin GPIO_PIN_3
#define EN_5V_DCDC_GPIO_Port GPIOE
#define OBC_TX_COM_RX_Pin GPIO_PIN_6
#define OBC_TX_COM_RX_GPIO_Port GPIOB
#define OBC_TX_COM_RXD5_Pin GPIO_PIN_5
#define OBC_TX_COM_RXD5_GPIO_Port GPIOD
#define OBC_TX_1_Pin GPIO_PIN_10
#define OBC_TX_1_GPIO_Port GPIOC
#define EN_3V3_2_DCDC_Pin GPIO_PIN_14
#define EN_3V3_2_DCDC_GPIO_Port GPIOH
#define SMSN_FM_MISO_OBC_Pin GPIO_PIN_5
#define SMSN_FM_MISO_OBC_GPIO_Port GPIOE
#define SMSN_FM_MOSI_OBC_Pin GPIO_PIN_6
#define SMSN_FM_MOSI_OBC_GPIO_Port GPIOE
#define EN_GLOBAL_RESET_Pin GPIO_PIN_10
#define EN_GLOBAL_RESET_GPIO_Port GPIOI
#define OBC_TX_2_Pin GPIO_PIN_6
#define OBC_TX_2_GPIO_Port GPIOC
#define OBC_RX_2_Pin GPIO_PIN_7
#define OBC_RX_2_GPIO_Port GPIOC
#define GPIO3_Pin GPIO_PIN_10
#define GPIO3_GPIO_Port GPIOF
#define DEBUG_TX_OBC_Pin GPIO_PIN_8
#define DEBUG_TX_OBC_GPIO_Port GPIOE
#define EN_UNREG_Pin GPIO_PIN_5
#define EN_UNREG_GPIO_Port GPIOH
#define GPIO1_Pin GPIO_PIN_11
#define GPIO1_GPIO_Port GPIOE
#define EN_3V3_COM_Pin GPIO_PIN_11
#define EN_3V3_COM_GPIO_Port GPIOH
#define DEBUG_RX_OBC_Pin GPIO_PIN_7
#define DEBUG_RX_OBC_GPIO_Port GPIOE
#define EN_5V_Pin GPIO_PIN_12
#define EN_5V_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
