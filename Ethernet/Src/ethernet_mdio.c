#include "ethernet_mdio.h"

#include <stddef.h>

#include "ethernet_port.h"

#define ETHERNET_MDIO_PHY_ADDRESS_MAX       31U
#define ETHERNET_MDIO_REGISTER_ADDRESS_MAX  31U

/**
 * @brief  检查 MDIO PHY 地址和寄存器地址是否合法。
 */
static bool EthernetMdio_IsAddressValid(uint32_t phy_address, uint32_t register_address)
{
    return (phy_address <= ETHERNET_MDIO_PHY_ADDRESS_MAX) &&
           (register_address <= ETHERNET_MDIO_REGISTER_ADDRESS_MAX);
}

/**
 * @brief  读取 PHY Clause 22 寄存器。
 */
bool EthernetMdio_Read(uint32_t phy_address, uint32_t register_address, uint32_t *value)
{
    ETH_HandleTypeDef *eth_handle = EthernetPort_GetHandle();

    if ((eth_handle == NULL) ||
        (value == NULL) ||
        !EthernetMdio_IsAddressValid(phy_address, register_address))
    {
        return false;
    }

    return HAL_ETH_ReadPHYRegister(eth_handle, phy_address, register_address, value) == HAL_OK;
}

/**
 * @brief  写入 PHY Clause 22 寄存器。
 */
bool EthernetMdio_Write(uint32_t phy_address, uint32_t register_address, uint32_t value)
{
    ETH_HandleTypeDef *eth_handle = EthernetPort_GetHandle();

    if ((eth_handle == NULL) ||
        !EthernetMdio_IsAddressValid(phy_address, register_address))
    {
        return false;
    }

    return HAL_ETH_WritePHYRegister(eth_handle, phy_address, register_address, value) == HAL_OK;
}
