#include "ethernet_mdio.h"

#include <stddef.h>

#include "eth.h"

#define ETHERNET_MDIO_PHY_ADDRESS_MAX       31U
#define ETHERNET_MDIO_REGISTER_ADDRESS_MAX  31U

/**
 * @brief  检查 MDIO PHY 地址和寄存器地址是否合法。
 *
 * @param[in] phy_address       PHY 地址。
 * @param[in] register_address  PHY 寄存器地址。
 *
 * @retval true   参数位于 Clause 22 MDIO 地址范围内。
 * @retval false  参数超出允许范围。
 */
static bool EthernetMdio_IsAddressValid(uint32_t phy_address, uint32_t register_address)
{
    return (phy_address <= ETHERNET_MDIO_PHY_ADDRESS_MAX) &&
           (register_address <= ETHERNET_MDIO_REGISTER_ADDRESS_MAX);
}

/**
 * @brief  读取 PHY Clause 22 寄存器。
 *
 * @param[in]  phy_address       PHY 地址，范围为 0~31。
 * @param[in]  register_address  PHY 寄存器地址，范围为 0~31。
 * @param[out] value             读取结果输出指针。
 *
 * @retval true   读取成功。
 * @retval false  参数无效或 HAL MDIO 操作失败。
 */
bool EthernetMdio_Read(uint32_t phy_address, uint32_t register_address, uint32_t *value)
{
    if ((value == NULL) || !EthernetMdio_IsAddressValid(phy_address, register_address))
    {
        return false;
    }

    return HAL_ETH_ReadPHYRegister(&heth, phy_address, register_address, value) == HAL_OK;
}

/**
 * @brief  写入 PHY Clause 22 寄存器。
 *
 * @param[in] phy_address       PHY 地址，范围为 0~31。
 * @param[in] register_address  PHY 寄存器地址，范围为 0~31。
 * @param[in] value             写入值，HAL 最终使用低 16 位。
 *
 * @retval true   写入成功。
 * @retval false  参数无效或 HAL MDIO 操作失败。
 */
bool EthernetMdio_Write(uint32_t phy_address, uint32_t register_address, uint32_t value)
{
    if (!EthernetMdio_IsAddressValid(phy_address, register_address))
    {
        return false;
    }

    return HAL_ETH_WritePHYRegister(&heth, phy_address, register_address, value) == HAL_OK;
}