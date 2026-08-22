# Hardware Baseline

本文记录当前 STM32H743VIT6 + LAN8720AI Reference Example 的 Ethernet 硬件事实。它是已验证示例，不把 GPIO、SRAM 地址或时钟拓扑当作所有 STM32H7 的固定配置。

## 1. 平台

| 项目 | 当前值 |
| --- | --- |
| MCU | STM32H743VIT6 / LQFP100 |
| PHY | LAN8720AI |
| MAC-PHY | RMII |
| PHY 供电 | 3.3 V |
| PHY 本地时钟 | 25 MHz 晶振 |
| 网络能力 | 10/100 Mbit/s |
| RTOS | FreeRTOS / CMSIS-RTOS2 |

Reference Example：

```text
examples/STM32H743_LAN8720_FreeRTOS/
```

## 2. RMII / SMI 引脚

| MCU 引脚 | 信号 |
| --- | --- |
| PA1 | RMII_REF_CLK |
| PA2 | ETH_MDIO |
| PC1 | ETH_MDC |
| PA7 | RMII_CRS_DV |
| PC4 | RMII_RXD0 |
| PC5 | RMII_RXD1 |
| PB11 | RMII_TX_EN |
| PB12 | RMII_TXD0 |
| PB13 | RMII_TXD1 |
| PC0 | ETH_RESET / PHY nRST |

PHY `nINT/REFCLKO` 接 STM32 PA1 / RMII_REF_CLK。

## 3. PHY 时钟

```text
25 MHz Crystal
→ LAN8720AI XTAL1/XTAL2
→ PHY internal clock generation
→ nINT/REFCLKO
→ STM32 PA1 / RMII_REF_CLK
```

已由原理图、Datasheet 和实际 Ethernet 功能支持该路径有效。25 MHz 晶振和 RMII REF_CLK 尚未用仪器独立测量，因此不标记为 Measured。

## 4. PHY Reset / MDIO

PC0 控制 LAN8720 nRST。Reset 释放后通过 PHY ID polling + timeout 判断可管理状态。

```text
PA2  ↔ MDIO
PC1  → MDC
```

使用 STM32 Ethernet MAC/HAL Clause 22 Management Interface，不使用 GPIO bit-bang。HAL ETH 版本 1.11.6；MDIO Read/Write 已上板验证。

## 5. PHY Address / Strap / ID

上板结果：

```text
Reg18       = 0x60E0
MODE[2:0]   = 111
PHY Address = 0
PHY ID1     = 0x0007
PHY ID2     = 0xC0F1
```

原理图 + Datasheet 支持：`nINTSEL` 为 Low，对应 REF_CLK Out Mode；`REGOFF` 为 Low，对应内部 regulator 启用。PHY Address/MODE/ID 已 MDIO 实测，strap 电平未独立电气测量。

## 6. Link / Speed / Duplex

On-board Verified：

```text
Link Up / Down
100 Mbit/s
Full Duplex
单次网线拔出 / 插回 Link 状态恢复
```

PHY 协商结果已用于配置 STM32 MAC，并完成 Raw TX/RX 和 async RX 验证。

尚未专项验证：10 Mbit/s、Half Duplex、快速连续插拔、Link/Speed LED。

## 7. 调试串口

```text
USART1
PA9 TX
PA10 RX
115200 / 8N1
```

已上板验证。UART 用于低频 Bring-up，不进入 ETH ISR / 高频数据路径。

## 8. Ethernet DMA 内存

当前 STM32H743 Example：

```text
RAM_ETH / SRAM3 = 0x30040000 ~ 0x30047FFF / 32 KiB
RAM_D2          = 0x30000000 ~ 0x3003FFFF / 256 KiB

RX Descriptor   = 0x30040000
TX Descriptor   = 0x30040080
RX Pool         = 0x30042000 / 0x1800 / 4×1536 B
TX Pool         = 0x30044000 / 0x1800 / 4×1536 B
```

MPU：SRAM3 Normal Non-cacheable；前 256 B Device overlay。当前 I/D Cache Disabled。

当前板 Port：

```text
examples/STM32H743_LAN8720_FreeRTOS/BSP/stm32h743vit6_iot/ethernet_port.c
```

其 `EthernetPort_PrepareDmaMemory()` 在 `MX_ETH_Init()` 前使能 D2 SRAM3 clock。

板级 linker：

```text
examples/STM32H743_LAN8720_FreeRTOS/STM32H743xx_FLASH.ld
```

## 9. 验证状态

On-board Verified：PHY Reset/MDIO/ID/Address/Auto-negotiation/Link/100M Full Duplex、USART1、MAC link sync、Raw TX、Raw RX、polling RX 1000/1000、ETH IRQ + CMSIS-RTOS2 async RX 1000/1000。

Build/Map Verified 历史布局：RX/TX Descriptor 与 RX/TX Pool 地址/大小如上。

第二阶段仅移动 Reference Example 路径，不改变硬件或 DMA 方案；新路径提交仍需重新 Build/map/On-board 才能单独标记 Verified。

## 10. 资料依据

- 当前有效 STM32H743VIT6 原理图；
- STM32H743 Datasheet / Reference Manual；
- LAN8720A/LAN8720AI Datasheet；
- 仓库 HAL 1.11.6；
- Example `.ioc`、linker、Port；
- Raw Frame / async RX 上板结果。
