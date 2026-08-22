#ifndef ETHERNET_MDIO_H
#define ETHERNET_MDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool EthernetMdio_Read(uint32_t phy_address, uint32_t register_address, uint32_t *value);
bool EthernetMdio_Write(uint32_t phy_address, uint32_t register_address, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNET_MDIO_H */
