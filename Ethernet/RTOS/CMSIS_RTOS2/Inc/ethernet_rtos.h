#ifndef ETHERNET_RTOS_H
#define ETHERNET_RTOS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*EthernetRtosRxFrameHandler)(const uint8_t *frame, uint16_t length, void *context);

void EthernetRtos_SetRxFrameHandler(EthernetRtosRxFrameHandler handler, void *context);
bool EthernetRtos_IsReady(void);
void EthernetRtos_RxTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNET_RTOS_H */
