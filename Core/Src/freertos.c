/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "board_ethernet.h"
#include "ethernet_mdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LAN8720_PHY_ADDRESS       0U
#define LAN8720_REG_ID1           2U
#define LAN8720_REG_ID2           3U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for BootstrapTask */
osThreadId_t BootstrapTaskHandle;
const osThreadAttr_t BootstrapTask_attributes = {
  .name = "BootstrapTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartBootstrapTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of BootstrapTask */
  BootstrapTaskHandle = osThreadNew(StartBootstrapTask, NULL, &BootstrapTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartBootstrapTask */
/**
  * @brief  Function implementing the BootstrapTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartBootstrapTask */
void StartBootstrapTask(void *argument)
{
  /* USER CODE BEGIN StartBootstrapTask */
  /* Infinite loop */
  for(;;)
  {
    
    osDelay(1);
  }
  uint32_t phy_id1 = 0U;
  uint32_t phy_id2 = 0U;

  osDelay(25U);

  BoardEthernet_PhyResetAssert();
  osDelay(1U);

  BoardEthernet_PhyResetRelease();
  osDelay(10U);

  if (EthernetMdio_Read(LAN8720_PHY_ADDRESS, LAN8720_REG_ID1, &phy_id1) && 
      EthernetMdio_Read(LAN8720_PHY_ADDRESS, LAN8720_REG_ID2, &phy_id2))
  {
      printf("[M1] PHY ID1=0x%04lX ID2=0x%04lX\r\n", (unsigned long)phy_id1, (unsigned long)phy_id2);
  }
  else
  {
      printf("[M1] PHY ID read failed\r\n");
  }
  /* USER CODE END StartBootstrapTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

