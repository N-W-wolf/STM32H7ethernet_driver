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
#include <stdbool.h>
#include <stdio.h>

#include "ethernet_driver.h"
#include "ethernet_port.h"
#include "ethernet_rtos.h"
#include "lan8720.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LAN8720_PHY_ADDRESS                 0U

#define PHY_READY_TIMEOUT_MS             100U
#define PHY_READY_POLL_PERIOD_MS           5U

#define AUTO_NEGOTIATION_TIMEOUT_MS      5000U
#define AUTO_NEGOTIATION_POLL_PERIOD_MS   100U

#define PHY_LINK_POLL_PERIOD_MS           200U

#define ETHERNET_RX_TEST_ETHERTYPE      0x88B5U
#define ETHERNET_RX_TEST_TARGET_COUNT   1000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static uint32_t g_ethernet_rx_frame_count;
static uint32_t g_ethernet_rx_test_frame_count;
/* USER CODE END Variables */
/* Definitions for BootstrapTask */
osThreadId_t BootstrapTaskHandle;
const osThreadAttr_t BootstrapTask_attributes = {
  .name = "BootstrapTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for EthernetRxTask */
osThreadId_t EthernetRxTaskHandle;
const osThreadAttr_t EthernetRxTask_attributes = {
  .name = "EthernetRxTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static bool EthernetBootstrap_StartMac(const Lan8720Status *phy_status);
static void EthernetDemo_RxFrameHandler(const uint8_t *frame, uint16_t length, void *context);
/* USER CODE END FunctionPrototypes */

void StartBootstrapTask(void *argument);
void StartEthernetRxTask(void *argument);

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

  /* creation of EthernetRxTask */
  EthernetRxTaskHandle = osThreadNew(StartEthernetRxTask, NULL, &EthernetRxTask_attributes);

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
  bool phy_ready = false;

  Lan8720Status last_phy_status = {0};
  bool last_status_valid = false;

  uint32_t elapsed_ms = 0U;

  printf("[ETH] BootstrapTask started\r\n");

  /* MX_GPIO_Init() 已经将 PHY nRST 拉低。等待上电稳定后释放 PHY 硬件复位。 */
  osDelay(25U);
  EthernetPort_PhyResetRelease();

  while (elapsed_ms < PHY_READY_TIMEOUT_MS)
  {
    if (Lan8720_IsReady(LAN8720_PHY_ADDRESS))
    {
      phy_ready = true;
      break;
    }

    osDelay(PHY_READY_POLL_PERIOD_MS);
    elapsed_ms += PHY_READY_POLL_PERIOD_MS;
  }

  if (!phy_ready)
  {
    printf("[ETH] PHY ready timeout\r\n");
  }
  else
  {
    printf("[ETH] PHY ready\r\n");

    if (!Lan8720_RestartAutoNegotiation(LAN8720_PHY_ADDRESS))
    {
      printf("[ETH] Auto-negotiation restart failed\r\n");
    }
    else
    {
      printf("[ETH] Auto-negotiation started\r\n");

      elapsed_ms = 0U;

      while (elapsed_ms < AUTO_NEGOTIATION_TIMEOUT_MS)
      {
        if (!Lan8720_GetStatus(LAN8720_PHY_ADDRESS, &phy_status))
        {
          printf("[ETH] PHY status read failed\r\n");
          break;
        }

        if (phy_status.auto_negotiation_complete && phy_status.link_up)
        {
          break;
        }

        osDelay(AUTO_NEGOTIATION_POLL_PERIOD_MS);
        elapsed_ms += AUTO_NEGOTIATION_POLL_PERIOD_MS;
      }

      if (phy_status.auto_negotiation_complete && phy_status.link_up)
      {
        printf("[ETH] Link up\r\n");

        if (phy_status.speed == LAN8720_SPEED_100M)
        {
          printf("[ETH] Speed=100M\r\n");
        }
        else if (phy_status.speed == LAN8720_SPEED_10M)
        {
          printf("[ETH] Speed=10M\r\n");
        }

        if (phy_status.duplex == LAN8720_DUPLEX_FULL)
        {
          printf("[ETH] Duplex=Full\r\n");
        }
        else if (phy_status.duplex == LAN8720_DUPLEX_HALF)
        {
          printf("[ETH] Duplex=Half\r\n");
        }

        if (EthernetBootstrap_StartMac(&phy_status))
        {
          printf("[ETH] MAC/DMA started\r\n");
        }

        last_phy_status = phy_status;
        last_status_valid = true;
      }
      else
      {
        printf("[ETH] Auto-negotiation timeout or link down\r\n");
      }
    }
  }

  /* Infinite loop */
  for(;;)
  {
    Lan8720Status current_phy_status = {0};

    if (Lan8720_GetStatus(LAN8720_PHY_ADDRESS, &current_phy_status))
    {
      if (!last_status_valid || current_phy_status.link_up != last_phy_status.link_up)
      {
        if (current_phy_status.link_up)
        {
          printf("[PHY] Link up\r\n");

          if (current_phy_status.speed == LAN8720_SPEED_100M)
          {
            printf("[PHY] Speed=100M\r\n");
          }
          else if (current_phy_status.speed == LAN8720_SPEED_10M)
          {
            printf("[PHY] Speed=10M\r\n");
          }

          if (current_phy_status.duplex == LAN8720_DUPLEX_FULL)
          {
            printf("[PHY] Duplex=Full\r\n");
          }
          else if (current_phy_status.duplex == LAN8720_DUPLEX_HALF)
          {
            printf("[PHY] Duplex=Half\r\n");
          }
        }
        else
        {
          printf("[PHY] Link down\r\n");
        }

        last_phy_status = current_phy_status;
        last_status_valid = true;
      }
    }

    osDelay(PHY_LINK_POLL_PERIOD_MS);
  }

  /* USER CODE END StartBootstrapTask */
}

/* USER CODE BEGIN Header_StartEthernetRxTask */
/**
* @brief Function implementing the EthernetRxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartEthernetRxTask */
void StartEthernetRxTask(void *argument)
{
  /* USER CODE BEGIN StartEthernetRxTask */
  EthernetRtos_SetRxFrameHandler(EthernetDemo_RxFrameHandler, NULL);

  printf("[ETH] EthernetRxTask started\r\n");

  EthernetRtos_RxTask(argument);
  /* USER CODE END StartEthernetRxTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/**
 * @brief  根据 PHY 协商结果配置并启动 Ethernet MAC/DMA。
 *
 * @param[in] phy_status PHY 当��链路状态。
 *
 * @retval true   MAC 配置并启动成功。
 * @retval false  PHY 状态无效、RX Runtime 未就绪或 Ethernet 操作失败。
 */
static bool EthernetBootstrap_StartMac(const Lan8720Status *phy_status)
{
  EthernetLinkSpeed speed;
  EthernetDuplexMode duplex;

  if ((phy_status == NULL) ||
      !phy_status->link_up ||
      !phy_status->auto_negotiation_complete)
  {
    return false;
  }

  if (!EthernetRtos_IsReady())
  {
    printf("[ETH] Ethernet RX runtime not ready\r\n");
    return false;
  }

  if (phy_status->speed == LAN8720_SPEED_100M)
  {
    speed = ETHERNET_LINK_SPEED_100M;
  }
  else if (phy_status->speed == LAN8720_SPEED_10M)
  {
    speed = ETHERNET_LINK_SPEED_10M;
  }
  else
  {
    printf("[ETH] Unsupported PHY speed\r\n");
    return false;
  }

  if (phy_status->duplex == LAN8720_DUPLEX_FULL)
  {
    duplex = ETHERNET_DUPLEX_FULL;
  }
  else if (phy_status->duplex == LAN8720_DUPLEX_HALF)
  {
    duplex = ETHERNET_DUPLEX_HALF;
  }
  else
  {
    printf("[ETH] Unsupported PHY duplex mode\r\n");
    return false;
  }

  if (!EthernetDriver_ConfigureLink(speed, duplex))
  {
    printf("[ETH] MAC link config failed\r\n");
    return false;
  }

  if (!EthernetDriver_Start())
  {
    printf("[ETH] MAC/DMA start failed\r\n");
    return false;
  }

  return true;
}

/**
 * @brief  Demo RX Frame 处理函数。
 */
static void EthernetDemo_RxFrameHandler(const uint8_t *frame, uint16_t length, void *context)
{
  uint16_t ether_type;

  (void)context;

  if (frame == NULL)
  {
    return;
  }

  g_ethernet_rx_frame_count++;

  if (length < 14U)
  {
    return;
  }

  ether_type = ((uint16_t)frame[12] << 8) | (uint16_t)frame[13];

  if (ether_type != ETHERNET_RX_TEST_ETHERTYPE)
  {
    return;
  }

  g_ethernet_rx_test_frame_count++;

  if (g_ethernet_rx_test_frame_count == ETHERNET_RX_TEST_TARGET_COUNT)
  {
    printf("[ETH] Async RX test 1000/1000 PASS, total=%lu\r\n",
           (unsigned long)g_ethernet_rx_frame_count);
  }
}

/* USER CODE END Application */

