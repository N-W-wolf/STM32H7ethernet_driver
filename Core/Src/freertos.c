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
#include <string.h>

#include "board_ethernet.h"
#include "ethernet_driver.h"
#include "lan8720.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LAN8720_PHY_ADDRESS                 0U

#define PHY_READY_TIMEOUT_MS             100U
#define PHY_READY_POLL_PERIOD_MS         5U

#define AUTO_NEGOTIATION_TIMEOUT_MS      5000U
#define AUTO_NEGOTIATION_POLL_PERIOD_MS  100U

#define PHY_LINK_POLL_PERIOD_MS  200U

#define RAW_RX_TEST_TIMEOUT_MS      10000U
#define RAW_RX_TEST_POLL_PERIOD_MS  5U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static uint8_t g_ethernet_test_rx_frame[ETHERNET_FRAME_BUFFER_SIZE];
static bool EthernetTest_WaitRawFrames(uint32_t expected_count, uint32_t timeout_ms);
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
static bool EthernetTest_SendRawFrame(const Lan8720Status *phy_status);
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
  bool phy_ready = false;

  Lan8720Status last_phy_status = {0};
  bool last_status_valid = false;

  uint32_t elapsed_ms = 0U;

  printf("[M1] BootstrapTask started\r\n");

  // MX_GPIO_Init() 已经将 PHY nRST 拉低。等待上电稳定后释放 PHY 硬件复位。
  osDelay(25U);
  BoardEthernet_PhyResetRelease();

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

        if (EthernetTest_SendRawFrame(&phy_status))
        {
            printf("[ETH] Raw frame TX OK\r\n");

            printf("[ETH] Waiting for raw RX test frame...\r\n");

            if (EthernetTest_WaitRawFrames(1000U, 15000U))
            {
              printf("[ETH] Continuous raw RX OK\r\n");
            }
            else
            {
              printf("[ETH] Continuous raw RX failed\r\n");
            }
        }
        else
        {
          printf("[ETH] Raw frame TX failed\r\n");
        }

        last_phy_status = phy_status;
        last_status_valid = true;
      }
      else
      {
        printf("[M1] Auto-negotiation timeout or link down\r\n");
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

/* Private application code --------------------------------------------------*/

/* USER CODE BEGIN Application */
/**
 * @brief  根据 PHY 协商结果启动 MAC/DMA，并发送一帧测试 Ethernet Frame。
 *
 * @param[in] phy_status PHY 当前链路状态。
 *
 * @retval true   MAC 配置、启动和 Frame 发送均成功。
 * @retval false  PHY 状态无效或 Ethernet 操作失败。
 */
static bool EthernetTest_SendRawFrame(const Lan8720Status *phy_status)
{
    EthernetLinkSpeed speed;
    EthernetDuplexMode duplex;

    uint8_t frame[60] = {0};

    static const char payload[] = "STM32H7 raw Ethernet TX";

    if ((phy_status == NULL) ||
        !phy_status->link_up ||
        !phy_status->auto_negotiation_complete)
    {
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

    /*
     * Destination MAC:
     * FF:FF:FF:FF:FF:FF
     */
    memset(&frame[0], 0xFF, 6U);

    /*
     * Source MAC:
     * frame[6] ~ frame[11] 保持为 0。
     *
     * EthernetDriver_Transmit() 使用 ETH_SRC_ADDR_REPLACE，
     * MAC 硬件会用 MAC Address 0 替换这里的内容。
     * 当前实际 MAC Address 0 由 CubeMX 的 MX_ETH_Init() 配置。
     */

    /*
     * EtherType:
     * 0x88B5，用于本地实验 Frame。
     */
    frame[12] = 0x88U;
    frame[13] = 0xB5U;

    memcpy(&frame[14], payload, sizeof(payload) - 1U);

    if (!EthernetDriver_Transmit(frame, sizeof(frame), 100U))
    {
        printf("[ETH] HAL transmit failed\r\n");
        return false;
    }

    return true;
}

/**
 * @brief  Polling 接收指定数量的 0x88B5 测试 Ethernet Frame。
 *
 * @param[in] expected_count 期望收到的测试 Frame 数量。
 * @param[in] timeout_ms     最大测试时间。
 *
 * @retval true   收到全部预期 Frame。
 * @retval false  超时或 RX Driver 发生错误。
 */
static bool EthernetTest_WaitRawFrames(uint32_t expected_count, uint32_t timeout_ms)
{
    static const char expected_payload[] = "PC -> STM32H7 raw Ethernet RX";

    uint32_t received_count = 0U;
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < timeout_ms)
    {
        uint16_t frame_length = 0U;

        EthernetRxResult result = EthernetDriver_Receive(
            g_ethernet_test_rx_frame,
            sizeof(g_ethernet_test_rx_frame),
            &frame_length);

        if (result == ETHERNET_RX_ERROR)
        {
            printf("[ETH] RX driver error, count=%lu\r\n",
                   (unsigned long)received_count);
            return false;
        }

        if (result == ETHERNET_RX_FRAME)
        {
            if ((frame_length >= (14U + sizeof(expected_payload) - 1U)) &&
                (g_ethernet_test_rx_frame[12] == 0x88U) &&
                (g_ethernet_test_rx_frame[13] == 0xB5U) &&
                (memcmp(&g_ethernet_test_rx_frame[14],
                        expected_payload,
                        sizeof(expected_payload) - 1U) == 0))
            {
                received_count++;

                if ((received_count % 100U) == 0U)
                {
                    printf("[ETH] RX count=%lu\r\n",
                           (unsigned long)received_count);
                }

                if (received_count >= expected_count)
                {
                    printf("[ETH] RX total=%lu\r\n",
                           (unsigned long)received_count);
                    return true;
                }
            }
        }

        osDelay(1U);
        elapsed_ms += 1U;
    }

    printf("[ETH] RX timeout, count=%lu/%lu\r\n",
           (unsigned long)received_count,
           (unsigned long)expected_count);

    return false;
}

/* USER CODE END Application */

