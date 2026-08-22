# Hardware Baseline

本文记录当前 STM32H743VIT6 + LAN8720AI 参考板的 Ethernet 硬件事实。它描述参考 Demo，不把当前板的 GPIO、SRAM 地址或时钟拓扑当作所有 STM32H7 用户的固定配置。

## 1. 验证平台

| 项目 | 当前值 | 依据 |
| --- | --- | --- |
| MCU | STM32H743VIT6 | 当前有效原理图 |
| 封装 | LQFP100 | MCU 型号 / 原理图 |
| PHY | LAN8720AI | 当前有效原理图 |
| MAC-PHY 接口 | RMII | 原理图与引脚连接 |
| PHY 供电 | 3.3 V 系统 | 原理图 |
| PHY 本地时钟源 | 25 MHz 晶振 | 原理图 |
| 网络速率能力 | 10/100 Mbit/s | LAN8720A Datasheet |
| RTOS | FreeRTOS / CMSIS-RTOS2 | 当前 Demo |
| TCP/IP 栈 | LwIP | 项目目标，尚未接入 |

STM32H743 内部提供 Ethernet MAC / DMA，LAN8720AI 负责物理层收发。

## 2. RMII / SMI 引脚

| STM32H743 引脚 | 网络信号 | 方向（相对 MCU） |
| --- | --- | --- |
| PA1 | RMII_REF_CLK | 输入 |
| PA2 | ETH_MDIO | 双向 |
| PC1 | ETH_MDC | 输出 |
| PA7 | RMII_CRS_DV | 输入 |
| PC4 | RMII_RXD0 | 输入 |
| PC5 | RMII_RXD1 | 输入 |
| PB11 | RMII_TX_EN | 输出 |
| PB12 | RMII_TXD0 | 输出 |
| PB13 | RMII_TXD1 | 输出 |
| PC0 | ETH_RESET / PHY nRST | 输出 |

PHY `nINT/REFCLKO` 与 MCU `PA1 / RMII_REF_CLK` 相连。

## 3. PHY 时钟拓扑

```text
25 MHz Crystal
      ↓
LAN8720AI XTAL1 / XTAL2
      ↓
LAN8720 internal clock generation
      ↓
nINT / REFCLKO
      ↓
STM32H743 PA1 / RMII_REF_CLK
```

【已确认事实】

- PHY 使用 25 MHz 晶振；
- LAN8720AI 当前 strap 支持 REF_CLK Out Mode；
- STM32 从 PA1 接收 RMII REF_CLK；
- PHY Reset、MDIO、Auto-negotiation、Link 以及裸 Frame RX/TX 均已工作。

【待确认 / 未独立测量】

- 25 MHz 晶振实际频率；
- PA1 / RMII_REF_CLK 实际频率。

因此目前没有“REF_CLK 已 Measured 50 MHz”的结论。

## 4. PHY Reset 与 Port

硬件连接：

```text
STM32H743 PC0
→ PC0_ETH_RESET
→ LAN8720AI nRST
```

Driver Package 的板级接口：

```c
void EthernetPort_PhyResetAssert(void);
void EthernetPort_PhyResetRelease(void);
```

当前参考实现位于：

```text
BSP/stm32h743vit6_iot/ethernet_port.c
```

已上板确认 PC0 可控制 PHY nRST，Reset 释放后可通过 MDIO 读到正确 PHY ID。

## 5. MDIO / MDC

```text
STM32 PA2  ↔ LAN8720 MDIO
STM32 PC1  → LAN8720 MDC
```

当前通过 STM32 Ethernet MAC / HAL PHY Management 接口访问 Clause 22 寄存器，不使用 GPIO bit-bang。

当前 HAL Ethernet 组件版本：STM32H7 HAL 1.11.6。

MDIO Read / Write 已上板验证。

## 6. PHY ID / Address / Strap

Reg 18 实测：

```text
Reg18       = 0x60E0
MODE[2:0]   = 111
PHYAD[4:0]  = 00000
```

因此：

```text
PHY Address = 0
MODE        = 111
Boot mode   = All capable + Auto-negotiation
```

PHY ID 实测：

```text
PHY ID1 = 0x0007
PHY ID2 = 0xC0F1
```

`nINTSEL` / `REGOFF` strap 判断由原理图和 LAN8720 Datasheet 支持，尚未单独做电气测量。

## 7. Link / Speed / Duplex

已上板验证：

```text
Link Up / Down
100 Mbit/s
Full Duplex
单次网线拔出 / 插回 Link 状态恢复
```

100M Full 协商结果已用于配置 STM32 MAC，并完成裸 Frame TX / RX。

尚未专项验证：

- 10 Mbit/s；
- Half Duplex；
- 连续快速插拔；
- Link / Speed LED。

## 8. 物理网络接口

```text
STM32H743 MAC
    ↕ RMII
LAN8720AI PHY
    ↕ MDI
Transformer
    ↕
RJ45
```

网络变压器和 RJ45 不需要软件管理。

## 9. 调试串口

```text
USART1
PA9  = TX
PA10 = RX
115200 / 8N1
```

已上板验证。

当前 PCB UART 丝印与有效原理图 TX/RX 标识存在反向情况，接线以 MCU 实际信号和有效原理图为准。

## 10. Ethernet DMA 内存

当前参考板使用 SRAM3：

```text
RAM_ETH
0x30040000 ~ 0x30047FFF
32 KiB
```

普通 D2 RAM 在 linker 中限制为：

```text
RAM_D2
0x30000000 ~ 0x3003FFFF
256 KiB
```

布局：

```text
RX Descriptor  0x30040000
TX Descriptor  0x30040080
RX Buffer Pool 0x30042000 / 4 × 1536 B / 0x1800
TX Buffer Pool 0x30044000 / 4 × 1536 B / 0x1800
```

MPU：

```text
Region 1: 0x30040000 / 32 KiB
          Normal, Non-cacheable, Non-bufferable, Shareable, XN

Region 2: 0x30040000 / 256 B
          Device, Non-cacheable, Bufferable, Non-shareable, XN
```

`EthernetPort_PrepareDmaMemory()` 在 `MX_ETH_Init()` 前显式使能 D2 SRAM3 时钟。

当前 I-Cache / D-Cache 均 Disabled。

物理布局、linker、MPU、ownership 细节见 `03_MEMORY_DMA.md`。

## 11. 数据路径验证

### On-board Verified

- PHY Reset；
- MDIO Read / Write；
- PHY ID / Address / Strap；
- Auto-negotiation；
- Link Up / Down；
- 100M Full；
- 单次网线拔插；
- USART1；
- MAC Speed / Duplex 同步；
- 裸 Ethernet TX；
- polling RX 单帧与连续 1000 / 1000；
- ETH IRQ + CMSIS-RTOS2 async RX 连续 1000 / 1000（重构前固件 `6b2f1f4...`）。

### Build / Map Verified

- `DMARxDscrTab = 0x30040000`；
- `DMATxDscrTab = 0x30040080`；
- RX Pool `0x30042000 / 0x1800`；
- TX Pool `0x30044000 / 0x1800`；
- linker ASSERT 通过。

### 尚未验证

- Package 化后的再次 build / map / on-board regression；
- PHY / RMII 时钟仪器测量；
- 10M / Half Duplex；
- 高负载 / 长时间 DMA；
- D-Cache 开启后的数据路径。

## 12. 资料依据

- 当前有效 STM32H743VIT6 原理图；
- STM32H743 Datasheet / Reference Manual；
- LAN8720A/LAN8720Ai Datasheet；
- 当前仓库 HAL 1.11.6 Ethernet 源码；
- 当前 `.ioc`、linker、Port 和 map 结果；
- 上板 Raw Frame / async RX 测试结果。
