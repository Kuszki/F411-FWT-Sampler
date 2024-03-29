/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#define TIMER2_CNT 2000-1
#define LED_OUT_Pin GPIO_PIN_13
#define LED_OUT_GPIO_Port GPIOC
#define ADC_0_IN_Pin GPIO_PIN_0
#define ADC_0_IN_GPIO_Port GPIOA
#define ADC_2_IN_Pin GPIO_PIN_2
#define ADC_2_IN_GPIO_Port GPIOA
#define TRIGGER_IN_Pin GPIO_PIN_2
#define TRIGGER_IN_GPIO_Port GPIOB
#define TRIGGER_IN_EXTI_IRQn EXTI2_IRQn
#define UART_1_TX_Pin GPIO_PIN_9
#define UART_1_TX_GPIO_Port GPIOA
#define UART_1_RX_Pin GPIO_PIN_10
#define UART_1_RX_GPIO_Port GPIOA
#define DEBUG_0_OUT_Pin GPIO_PIN_7
#define DEBUG_0_OUT_GPIO_Port GPIOB
#define DEBUG_1_OUT_Pin GPIO_PIN_8
#define DEBUG_1_OUT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
