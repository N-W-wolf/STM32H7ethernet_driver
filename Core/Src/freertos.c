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
#include "lan8720.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LAN8720_PHY_ADDRESS                 0U

#define M1_AUTO_NEGOTIATION_TIMEOUT_MS      5000U
#define M1_AUTO_NEGOTIATION_POLL_PERIOD_MS  100U
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
  Lan8720Status phy_status = {0};
  uint32_t elapsed_ms = 0U;

  printf("[M1] BootstrapTask started\r\n");

  // MX_GPIO_Init() 已经将 PHY nRST 拉低。等待上电稳定后释放 PHY 硬件复位。
  osDelay(25U);

  BoardEthernet_PhyResetRelease();
  osDelay(10U);

  if (!Lan8720_RestartAutoNegotiation(LAN8720_PHY_ADDRESS))
  {
    printf("[M1] Auto-negotiation restart failed\r\n");
  }
  else
  {
    printf("[M1] Auto-negotiation started\r\n");
    while (elapsed_ms < M1_AUTO_NEGOTIATION_TIMEOUT_MS)
    {
      if (!Lan8720_GetStatus(LAN8720_PHY_ADDRESS, &phy_status))
      {
        printf("[M1] PHY status read failed\r\n");
        break;
      }

      if (phy_status.auto_negotiation_complete && phy_status.link_up)
      {
        break;
      }

      osDelay(M1_AUTO_NEGOTIATION_POLL_PERIOD_MS);
      elapsed_ms += M1_AUTO_NEGOTIATION_POLL_PERIOD_MS;
    }

    if (phy_status.auto_negotiation_complete && phy_status.link_up)
    {
      printf("[M1] Link up\r\n");

      uint32_t phy_special_status = 0U;

      if (EthernetMdio_Read(LAN8720_PHY_ADDRESS, 31U, &phy_special_status))
      {
        printf("[M1] Reg31=0x%04lX HCDSPEED=0x%lX\r\n", (unsigned long)phy_special_status, (unsigned long)((phy_special_status >> 2U) & 0x07U));
      }

      if (phy_status.speed == LAN8720_SPEED_100M)
      {
        printf("[M1] Speed=100M\r\n");
      }
      else if (phy_status.speed == LAN8720_SPEED_10M)
      {
        printf("[M1] Speed=10M\r\n");
      }

      if (phy_status.duplex == LAN8720_DUPLEX_FULL)
      {
        printf("[M1] Duplex=Full\r\n");
      }
      else if (phy_status.duplex == LAN8720_DUPLEX_HALF)
      {
        printf("[M1] Duplex=Half\r\n");
      }
    }
    else
    {
      printf("[M1] Auto-negotiation timeout or link down\r\n");
    }

  }

  /* Infinite loop */
  for(;;)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
    
    osDelay(1000);
  }
  
  /* USER CODE END StartBootstrapTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

