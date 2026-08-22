#include "ethernet_port.h"

#include "eth.h"
#include "main.h"

ETH_HandleTypeDef *EthernetPort_GetHandle(void)
{
    return &heth;
}

void EthernetPort_PrepareDmaMemory(void)
{
    __HAL_RCC_D2SRAM3_CLK_ENABLE();
}

void EthernetPort_PhyResetAssert(void)
{
    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port, ETH_RESET_Pin, GPIO_PIN_RESET);
}

void EthernetPort_PhyResetRelease(void)
{
    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port, ETH_RESET_Pin, GPIO_PIN_SET);
}
