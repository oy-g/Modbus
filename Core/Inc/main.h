/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#define LED1_Pin GPIO_PIN_9
#define LED1_GPIO_Port GPIOF
#define LED2_Pin GPIO_PIN_10
#define LED2_GPIO_Port GPIOF
#define USART2_DIR_Pin GPIO_PIN_1
#define USART2_DIR_GPIO_Port GPIOA
#define OU8_Pin GPIO_PIN_6
#define OU8_GPIO_Port GPIOA
#define OU7_Pin GPIO_PIN_7
#define OU7_GPIO_Port GPIOA
#define OU6_Pin GPIO_PIN_0
#define OU6_GPIO_Port GPIOB
#define OU5_Pin GPIO_PIN_1
#define OU5_GPIO_Port GPIOB
#define OU4_Pin GPIO_PIN_12
#define OU4_GPIO_Port GPIOD
#define OU3_Pin GPIO_PIN_13
#define OU3_GPIO_Port GPIOD
#define OU2_Pin GPIO_PIN_14
#define OU2_GPIO_Port GPIOD
#define OU1_Pin GPIO_PIN_15
#define OU1_GPIO_Port GPIOD
#define USART1_DIR_Pin GPIO_PIN_8
#define USART1_DIR_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
// #define OU4_Pin GPIO_PIN_12 //PD12
// #define OU4_GPIO_Port GPIOD
// #define OU3_Pin GPIO_PIN_13 //PD13
// #define OU3_GPIO_Port GPIOD
// #define OU2_Pin GPIO_PIN_14 //PD14
// #define OU2_GPIO_Port GPIOD
// #define OU1_Pin GPIO_PIN_15 //PD15
// #define OU1_GPIO_Port GPIOD


// #define OU8_Pin GPIO_PIN_6 //PA6
// #define OU8_GPIO_Port GPIOA
// #define OU7_Pin GPIO_PIN_7 //PA7
// #define OU7_GPIO_Port GPIOA
// #define OU6_Pin GPIO_PIN_0 //PB0
// #define OU6_GPIO_Port GPIOB
// #define OU5_Pin GPIO_PIN_1 //PB1
// #define OU5_GPIO_Port GPIOB
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
