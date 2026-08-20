#ifndef BOARD_ETHERNET_H
#define BOARD_ETHERNET_H

#ifdef __cplusplus
extern "C" {
#endif


void BoardEthernet_PhyResetAssert(void);
void BoardEthernet_PhyResetRelease(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_ETHERNET_H */