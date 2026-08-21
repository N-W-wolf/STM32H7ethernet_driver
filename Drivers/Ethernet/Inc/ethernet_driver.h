#ifndef ETHERNET_DRIVER_H
#define ETHERNET_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ETHERNET_FRAME_BUFFER_SIZE 1536U

/**
 * @brief Ethernet 接收结果。
 */
typedef enum
{
    ETHERNET_RX_NONE = 0,   // 没有包
    ETHERNET_RX_FRAME,      // 收到完整包
    ETHERNET_RX_ERROR       // 内部异常
} EthernetRxResult;

/**
 * @brief Ethernet 链路速率。
 */
typedef enum
{
    ETHERNET_LINK_SPEED_10M = 0,
    ETHERNET_LINK_SPEED_100M
} EthernetLinkSpeed;

/**
 * @brief Ethernet 双工模式。
 */
typedef enum
{
    ETHERNET_DUPLEX_HALF = 0,   // 半双工
    ETHERNET_DUPLEX_FULL        // 全双工
} EthernetDuplexMode;

void EthernetDriver_Init(void);
bool EthernetDriver_ConfigureLink(EthernetLinkSpeed speed, EthernetDuplexMode duplex);
bool EthernetDriver_Start(void);
bool EthernetDriver_Transmit(const uint8_t *frame, uint16_t length, uint32_t timeout_ms);
EthernetRxResult EthernetDriver_Receive(uint8_t *frame, uint16_t capacity, uint16_t *length);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNET_DRIVER_H */