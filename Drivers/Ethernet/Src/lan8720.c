#include "lan8720.h"

#include <stddef.h>

#include "ethernet_mdio.h"

#define LAN8720_REG_BMCR                      0U
#define LAN8720_REG_BMSR                      1U
#define LAN8720_REG_PHY_SPECIAL_CONTROL       31U

#define LAN8720_BMCR_AUTO_NEGOTIATION_ENABLE  (1U << 12)
#define LAN8720_BMCR_RESTART_AUTO_NEGOTIATION (1U << 9)

#define LAN8720_BMSR_AUTO_NEGOTIATION_COMPLETE (1U << 5)
#define LAN8720_BMSR_LINK_STATUS                (1U << 2)

#define LAN8720_PHY_STATUS_AUTODONE            (1U << 12)
#define LAN8720_PHY_STATUS_HCDSPEED_SHIFT      2U
#define LAN8720_PHY_STATUS_HCDSPEED_MASK       0x07U

#define LAN8720_HCDSPEED_10M_HALF              0x01U
#define LAN8720_HCDSPEED_100M_HALF             0x02U
#define LAN8720_HCDSPEED_10M_FULL              0x05U
#define LAN8720_HCDSPEED_100M_FULL             0x06U

/**
 * @brief  重新启动 PHY 自动协商。
 *
 * @param[in] phy_address PHY 地址，范围为 0~31。
 *
 * @retval true   自动协商重新启动请求写入成功。
 * @retval false  MDIO 读写失败。
 */
bool Lan8720_RestartAutoNegotiation(uint32_t phy_address)
{
    uint32_t bmcr = 0U;

    if (!EthernetMdio_Read(phy_address, LAN8720_REG_BMCR, &bmcr))
    {
        return false;
    }

    bmcr |= LAN8720_BMCR_AUTO_NEGOTIATION_ENABLE;
    bmcr |= LAN8720_BMCR_RESTART_AUTO_NEGOTIATION;

    return EthernetMdio_Write(phy_address, LAN8720_REG_BMCR, bmcr);
}

/**
 * @brief  获取 PHY 当前链路、自动协商、速率和双工状态。
 *
 * @param[in]  phy_address PHY 地址，范围为 0~31。
 * @param[out] status      PHY 状态输出。
 *
 * @retval true   PHY 状态读取成功。
 * @retval false  参数无效或 MDIO 读取失败。
 */
bool Lan8720_GetStatus(uint32_t phy_address, Lan8720Status *status)
{
    uint32_t bmsr = 0U;
    uint32_t phy_status = 0U;
    uint32_t hcdspeed = 0U;

    if (status == NULL)
    {
        return false;
    }

    status->link_up = false;
    status->auto_negotiation_complete = false;
    status->speed = LAN8720_SPEED_UNKNOWN;
    status->duplex = LAN8720_DUPLEX_UNKNOWN;

    // BMSR Link Status 为 latch-low。
    // 第一次读取清除历史 latch，第二次读取用于获取当前状态。
    if (!EthernetMdio_Read(phy_address, LAN8720_REG_BMSR, &bmsr))
    {
        return false;
    }

    if (!EthernetMdio_Read(phy_address, LAN8720_REG_BMSR, &bmsr))
    {
        return false;
    }

    status->link_up = (bmsr & LAN8720_BMSR_LINK_STATUS) != 0U;
    status->auto_negotiation_complete = (bmsr & LAN8720_BMSR_AUTO_NEGOTIATION_COMPLETE) != 0U;

    if (!EthernetMdio_Read(phy_address, LAN8720_REG_PHY_SPECIAL_CONTROL, &phy_status))
    {
        return false;
    }

    if ((phy_status & LAN8720_PHY_STATUS_AUTODONE) == 0U)
    {
        status->auto_negotiation_complete = false;
        return true;
    }

    status->auto_negotiation_complete = true;

    if (!status->link_up)
    {
        return true;
    }

    hcdspeed = (phy_status >> LAN8720_PHY_STATUS_HCDSPEED_SHIFT) & LAN8720_PHY_STATUS_HCDSPEED_MASK;

    switch (hcdspeed)
    {
        case LAN8720_HCDSPEED_10M_HALF:
            status->speed = LAN8720_SPEED_10M;
            status->duplex = LAN8720_DUPLEX_HALF;
            break;

        case LAN8720_HCDSPEED_10M_FULL:
            status->speed = LAN8720_SPEED_10M;
            status->duplex = LAN8720_DUPLEX_FULL;
            break;

        case LAN8720_HCDSPEED_100M_HALF:
            status->speed = LAN8720_SPEED_100M;
            status->duplex = LAN8720_DUPLEX_HALF;
            break;

        case LAN8720_HCDSPEED_100M_FULL:
            status->speed = LAN8720_SPEED_100M;
            status->duplex = LAN8720_DUPLEX_FULL;
            break;

        default:
            return true;
    }

    return true;
}