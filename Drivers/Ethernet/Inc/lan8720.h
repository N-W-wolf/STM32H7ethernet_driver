#ifndef LAN8720_H
#define LAN8720_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief LAN8720 链路速率。
 */
typedef enum
{
    LAN8720_SPEED_UNKNOWN = 0,
    LAN8720_SPEED_10M,
    LAN8720_SPEED_100M
} Lan8720Speed;


/**
 * @brief LAN8720 链路双工模式。
 */
typedef enum
{
    LAN8720_DUPLEX_UNKNOWN = 0,
    LAN8720_DUPLEX_HALF,
    LAN8720_DUPLEX_FULL
} Lan8720Duplex;

/**
 * @brief LAN8720 当前链路状态。
 *
 * 包含 PHY 链路状态、自动协商状态以及协商得到的速率和双工模式。
 * 当链路未建立或自动协商尚未完成时，speed 和 duplex 可能为 UNKNOWN。
 */
typedef struct
{
    bool link_up;
    bool auto_negotiation_complete;
    Lan8720Speed speed;
    Lan8720Duplex duplex;
} Lan8720Status;

bool Lan8720_IsReady(uint32_t phy_address);
bool Lan8720_RestartAutoNegotiation(uint32_t phy_address);
bool Lan8720_GetStatus(uint32_t phy_address, Lan8720Status *status);

#ifdef __cplusplus
}
#endif

#endif /* LAN8720_H */