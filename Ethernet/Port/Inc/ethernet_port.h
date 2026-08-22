#ifndef ETHERNET_PORT_H
#define ETHERNET_PORT_H

#include "stm32h7xx_hal_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

ETH_HandleTypeDef *EthernetPort_GetHandle(void);
void EthernetPort_PrepareDmaMemory(void);
void EthernetPort_PhyResetAssert(void);
void EthernetPort_PhyResetRelease(void);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNET_PORT_H */
