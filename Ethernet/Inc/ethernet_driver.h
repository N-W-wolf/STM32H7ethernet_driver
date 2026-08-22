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
    ETHERNET_RX_NONE = 0,
    ETHERNET_RX_FRAME,
    ETHERNET_RX_ERROR
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
    ETHERNET_DUPLEX_HALF = 0,
    ETHERNET_DUPLEX_FULL
} EthernetDuplexMode;

/**
 * @brief RX complete 事件处理函数。
 *
 * @details
 * 该回调由 Ethernet HAL RX complete callback 在 ISR 上下文触发。
 * 实现必须保持短小，不得阻塞、打印日志或处理协议业务。
 */
typedef void (*EthernetDriverRxEventHandler)(void *context);

void EthernetDriver_Init(void);
void EthernetDriver_SetRxEventHandler(EthernetDriverRxEventHandler handler, void *context);
bool EthernetDriver_ConfigureLink(EthernetLinkSpeed speed, EthernetDuplexMode duplex);
bool EthernetDriver_Start(void);
bool EthernetDriver_Transmit(const uint8_t *frame, uint16_t length, uint32_t timeout_ms);
EthernetRxResult EthernetDriver_Receive(uint8_t *frame, uint16_t capacity, uint16_t *length);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNET_DRIVER_H */
