#include "board_ethernet.h"

#include "main.h"

/**
 * @brief  拉低 PHY 硬件复位信号。
 *
 * @details
 * 当前验证板使用 STM32H743VIT6 的 PC0 控制 LAN8720AI nRST。
 * GPIO 定义由 CubeMX 生成在 main.h 中。
 *
 * @return 无。
 */
void BoardEthernet_PhyResetAssert(void)
{
    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port, ETH_RESET_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  释放 PHY 硬件复位信号。
 *
 * @details
 * 将 LAN8720AI nRST 拉高。
 * 本函数不执行阻塞等待，复位释放后的延时由上层调用方负责。
 *
 * @return 无。
 */
void BoardEthernet_PhyResetRelease(void)
{
    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port, ETH_RESET_Pin, GPIO_PIN_SET);
}

/**
 * @brief  准备 Ethernet DMA 使用的板级内存。
 *
 * @details
 * 当前验证板将 STM32H743 SRAM3 用作 Ethernet DMA 专用内存，
 * 在 Ethernet 初始化前显式使能 D2 SRAM3 时钟。
 *
 * @return 无。
 */
void BoardEthernet_PrepareDmaMemory(void)
{
    __HAL_RCC_D2SRAM3_CLK_ENABLE();
}