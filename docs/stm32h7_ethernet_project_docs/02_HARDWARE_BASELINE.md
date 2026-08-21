# Hardware Baseline

本文记录当前验证板的 Ethernet 相关硬件事实。只写由有效原理图、器件 Datasheet、Reference Manual、当前源码或实测支持的信息；推断和未独立测量项必须明确标注。

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
| RTOS | FreeRTOS / CMSIS-RTOS v2 | 当前工程 |
| TCP/IP 栈 | LwIP | 项目软件环境，当前未接入 |

STM32H743 内部提供 Ethernet MAC 和 DMA，外接 LAN8720AI 负责物理层收发。

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

PHY 的 `nINT/REFCLKO` 与 MCU `PA1 / RMII_REF_CLK` 相连。

## 3. PHY 时钟拓扑

原理图和 LAN8720A Datasheet 支持以下连接：

```text
25 MHz Crystal
      ↓
LAN8720AI XTAL1 / XTAL2
      ↓
LAN8720AI internal clock generation
      ↓
nINT / REFCLKO
      ↓
STM32H743 PA1 / RMII_REF_CLK
```

已确认：

- PHY 使用 25 MHz 晶振；
- LAN8720AI 配置支持 REF_CLK Out Mode；
- STM32H743 从 PA1 接收 RMII REF_CLK；
- PHY Reset、MDIO、Auto-negotiation 和 Link 已可正常工作，从功能上支持该时钟路径有效。

未独立测量：

- 25 MHz PHY 晶振实际频率；
- PA1 / RMII_REF_CLK 实际频率。

因此不把“PA1 实测 50 MHz”描述为 Measured 结果。

## 4. PHY Reset

连接：

```text
STM32H743 PC0
    ↓
PC0_ETH_RESET
    ↓
LAN8720AI nRST
```

当前 BSP 接口：

```c
void BoardEthernet_PhyResetAssert(void);
void BoardEthernet_PhyResetRelease(void);
```

已上板确认：

- PC0 可以控制 LAN8720AI nRST；
- Reset 释放后可以通过 MDIO 读取正确 PHY ID；
- 软件通过 PHY ID polling + timeout 判断 PHY 已进入可管理状态，不依赖固定长延时判断 ready。

## 5. MDIO / MDC

连接：

```text
STM32 PA2  ↔ LAN8720 MDIO
STM32 PC1  → LAN8720 MDC
```

当前工程通过 STM32 Ethernet MAC/HAL PHY Management 接口访问 LAN8720 Clause 22 寄存器，不使用 GPIO bit-bang MDIO。

当前 HAL Ethernet 组件版本为 STM32H7 HAL 1.11.6。

MDIO Read / Write 已完成上板验证。

## 6. PHY ID / Address / Strap

LAN8720 Special Modes Register（Reg 18）上板读取结果：

```text
Reg18       = 0x60E0
MODE[2:0]   = 111
PHYAD[4:0]  = 00000
```

已确认：

```text
PHY Address : 0
MODE[2:0]   : 111
Boot mode   : All capable, Auto-negotiation enabled
```

PHY ID 实测：

```text
PHY ID1 = 0x0007
PHY ID2 = 0xC0F1
```

与 LAN8720 系列器件标识一致。

结合原理图和 Datasheet：

- `nINTSEL` 外部 strap 为 Low，符合 REF_CLK Out Mode；
- `REGOFF` 外部 strap 为 Low，符合内部 1.2 V regulator 启用模式。

PHY Address、MODE 和 PHY ID 已通过 MDIO 实测；`nINTSEL` / `REGOFF` 由原理图和 Datasheet 支持，尚未独立做电气测量。

## 7. Link / Speed / Duplex

已上板验证：

```text
Link Up / Down 可检测
100 Mbit/s 可读取
Full Duplex 可读取
单次网线拔出 / 插回可恢复 Link 状态
```

尚未完成专项验证：

- 10 Mbit/s 实际链路；
- Half Duplex 实际链路；
- 连续多次快速插拔；
- Link / Speed LED 行为。

## 8. 物理网络接口

当前原理图包含：

- LAN8720AI；
- TXP/TXN、RXP/RXN；
- 网络变压器；
- RJ45；
- Link / Speed LED 相关连接。

软件边界：

```text
STM32H743 MAC
    ↕ RMII
LAN8720AI PHY
    ↕ MDI
Transformer
    ↕
RJ45
```

网络变压器和 RJ45 不需要由软件驱动管理。

## 9. 调试串口

```text
USART1
PA9  = USART1_TX
PA10 = USART1_RX
115200 baud
8 data bits
No parity
1 stop bit
No hardware flow control
```

该配置已上板验证。

当前验证板 UART 相关 PCB 丝印与有效原理图中的 TX/RX 标识存在反向情况。接线应以 MCU 实际信号和有效原理图为依据。

## 10. Ethernet DMA 内存

当前验证板使用 STM32H743 SRAM3 作为 Ethernet DMA 专用内存：

```text
SRAM3 / RAM_ETH
0x30040000 ~ 0x30047FFF
32 KiB
```

普通 D2 RAM 在 linker 中限制为：

```text
RAM_D2
0x30000000 ~ 0x3003FFFF
256 KiB
```

已配置 Descriptor：

```text
RX Descriptor  0x30040000
TX Descriptor  0x30040080
```

当前 4 个 Descriptor 的实际 section 大小均为 96 B，并已通过 linker / map 验证。

MPU：

```text
Region 1: 0x30040000 / 32 KiB
          Normal, Non-cacheable, Non-bufferable, Shareable, XN

Region 2: 0x30040000 / 256 B
          Device, Non-cacheable, Bufferable, Non-shareable, XN
```

Region 2 编号更高，覆盖 SRAM3 前 256 B 的 Descriptor 区域。

板级初始化在 `MX_ETH_Init()` 前调用 `BoardEthernet_PrepareDmaMemory()`，显式使能 D2 SRAM3 时钟。

当前 CPU I-Cache / D-Cache 均未启用。RX/TX 数据 Buffer Pool 尚未实现，因此 Buffer 实际地址和 ownership 仍未形成可验证数据路径。

详细设计见 `03_MEMORY_DMA.md`。

## 11. 验证状态汇总

### On-board Verified

- PHY Reset；
- MDIO Read / Write；
- PHY ID；
- PHY Address = 0；
- MODE = 111；
- Auto-negotiation；
- Link Up / Down；
- 100 Mbit/s；
- Full Duplex；
- 单次网线拔出 / 插回恢复；
- USART1 调试输出。

### Build / Map Verified

- SRAM3 从普通 `RAM_D2` 中独立为 `RAM_ETH`；
- RX Descriptor = `0x30040000`；
- TX Descriptor = `0x30040080`；
- Descriptor linker size / non-empty 断言通过。

### 尚未独立测量或功能验证

- 25 MHz PHY 晶振频率；
- PA1 / RMII_REF_CLK 频率；
- Ethernet DMA 裸 Frame RX/TX；
- RX/TX Buffer 数据路径；
- Cache 开启后的数据路径。

## 12. 资料依据

主要依据：

- 当前有效 STM32H743VIT6 开发板原理图；
- STM32H743 Datasheet / Reference Manual；
- LAN8720A/LAN8720Ai Datasheet；
- 当前仓库 STM32H7 HAL 1.11.6 Ethernet 源码；
- 当前仓库 `.ioc`、BSP、linker 和构建结果。
