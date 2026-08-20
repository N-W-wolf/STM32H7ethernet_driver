# Hardware Baseline

- 状态：Active
- 用途：记录当前第一验证板的硬件事实
- 原则：只写已经由当前有效原理图、Datasheet 或实测确认的信息；推断必须显式标记。

## 1. 第一验证平台

| 项目 | 当前值 | 可信度 |
|---|---|---|
| MCU | STM32H743VIT6 | 已由当前有效原理图确认 |
| PHY | LAN8720AI-CP-TR 系列 | 已由当前有效原理图确认 |
| MAC-PHY 接口 | RMII | 已由原理图与信号连接确认 |
| PHY 供电 | 3.3 V 系统 | 已由原理图确认 |
| PHY 本地时钟源 | 25 MHz 晶振 | 已由原理图确认 |
| 网络速率能力 | 10/100 Mbit/s | LAN8720A Datasheet |
| RTOS | FreeRTOS | 项目设计决定 |
| TCP/IP 栈 | LwIP | 项目设计决定 |

STM32H743 本身提供 Ethernet MAC 和专用 DMA，外接 PHY 通过 RMII 与 MAC 通信。

## 2. RMII / SMI 引脚

根据当前有效 STM32H743VIT6 原理图：

| STM32H743 引脚 | 网络信号 | 方向（相对 MCU） |
|---|---|---|
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

PHY 的 `nINT/REFCLKO` 与 MCU `PA1_RMII_REF_CLK` 相连。

## 3. PHY 时钟拓扑

当前原理图显示：

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

LAN8720A Datasheet 明确支持使用 25 MHz 晶振并向 MAC 输出 50 MHz RMII `REF_CLK`。

因此当前高可信判断是：

- PHY 使用 25 MHz 晶振；
- LAN8720AI 负责产生 RMII 50 MHz REF_CLK；
- STM32H743 从 PA1 接收该时钟。

M1 已完成 PHY Reset、MDIO、Auto-negotiation 和 Link 上板验证，这从功能上支持当前 REF_CLK 路径能够工作的判断。

但当前未使用示波器或逻辑分析仪独立测量 25 MHz 晶振及 PA1 / RMII_REF_CLK，因此不把“PA1 实测约 50 MHz”写成 Measured 事实。

## 4. PHY Reset

当前原理图：

```text
STM32H743 PC0
    ↓
PC0_ETH_RESET
    ↓
LAN8720AI nRST
```

当前软件由 CubeMX 初始化阶段先将 `ETH_RESET` 拉低，再由 BSP 接口释放 PHY Reset。

M1 已上板确认：

- PC0 可以控制 LAN8720AI nRST；
- Reset 释放后可以通过 MDIO 读取正确 PHY ID；
- 软件使用 PHY ID polling + timeout 判断 PHY 已进入可管理状态，而不是依赖固定释放后延时判断 ready。

## 5. MDIO / MDC

当前原理图：

```text
STM32 PA2  ↔ LAN8720 MDIO
STM32 PC1  → LAN8720 MDC
```

当前第一版通过 STM32 Ethernet MAC/HAL PHY Management 接口访问 LAN8720 Clause 22 寄存器，不使用 GPIO bit-bang MDIO。

当前工程 HAL 为 STM32H7 HAL 1.11.6；M1 已完成 MDIO Read / Write 上板验证。

## 6. PHY Strap

M1 上板读取 LAN8720 Special Modes Register（Reg 18）得到：

```text
Reg18       = 0x60E0
MODE[2:0]   = 111
PHYAD[4:0]  = 00000
```

因此当前第一验证板已确认：

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

结合当前有效原理图与 LAN8720A Datasheet：

- `nINTSEL` 外部 strap 为 Low，符合 REF_CLK Out Mode；
- `REGOFF` 外部 strap 为 Low，符合内部 1.2 V regulator 启用模式。

其中 PHY Address、MODE 和 PHY ID 已通过 MDIO 上板实测确认；`nINTSEL` / `REGOFF` 当前由原理图连接和 Datasheet 支持，尚未通过独立电气测量确认。

## 7. 物理网络接口

当前原理图包含：

- LAN8720AI；
- TXP/TXN、RXP/RXN；
- 网络变压器；
- RJ45；
- Link / Speed LED 相关连接。

第一版软件驱动无需管理变压器和 RJ45。

软件关注边界：

```text
STM32H743 MAC
    ↕ RMII
LAN8720AI PHY
    ↕ MDI
Transformer
    ↕
RJ45
```

## 8. M0 调试串口

当前工程使用 USART1 作为基础调试输出：

```text
PA9  = USART1_TX
PA10 = USART1_RX
115200 baud
8 data bits
No parity
1 stop bit
No hardware flow control
```

该配置已完成上板验证，Linux 端可通过对应 USB 虚拟串口接收日志。

### PCB 丝印注意事项

M0 实测发现：当前板卡 UART 相关 PCB 丝印与有效原理图中的 TX/RX 标识相反。

已验证的软件信号定义仍为：

```text
PA9  = USART1_TX
PA10 = USART1_RX
```

因此调试接线时应以 MCU 实际信号和当前有效原理图为依据，不依赖该处 PCB 丝印。该结论仅针对当前验证板，不外推到其他板卡。

## 9. 内存 / DMA

当前尚未冻结 Ethernet DMA 内存布局。

已知项目约束：

- Ethernet DMA 使用的 Descriptor / Buffer 不得放入 DMA 无法访问的内存；
- STM32H7 D-Cache 一致性必须显式处理；
- 第一版倾向划分专用 `.eth_dma` 区域；
- 第一版倾向优先采用简单、易验证的一致性方案。

最终 SRAM 区域、MPU 属性、Descriptor 地址和 Buffer 地址统一在 `03_MEMORY_DMA.md` 中确定。

## 10. 当前硬件验证清单

M0 已确认：

- [x] USART1 PA9 TX / PA10 RX 配置可工作；
- [x] 基础串口输出链路可工作；
- [x] 记录当前 PCB UART 丝印与原理图标识冲突。

M1 已确认：

- [x] PC0 可正确控制 LAN8720AI nRST；
- [x] MDIO 可以读 PHY ID；
- [x] MDIO Write 可用；
- [x] PHY Address 与 strap 一致；
- [x] MODE strap 与读取寄存器一致；
- [x] 插拔网线可以反映到 PHY Link 状态；
- [x] 100 Mbit/s / Full Duplex 状态可以正确读取。

仍待独立或补充验证：

- [ ] 25 MHz PHY 晶振独立测量；
- [ ] PA1 / RMII_REF_CLK 独立测量；
- [ ] Link / Speed LED 行为专项验证；
- [ ] 10 Mbit/s 实际链路；
- [ ] Half Duplex 实际链路。

## 11. 资料基线

当前主要资料：

- 《STM32H7 Ethernet 通用驱动开发指导与规划》
- STM32H743 Datasheet
- LAN8720A/LAN8720Ai Datasheet
- 当前有效 STM32H743VIT6 开发板原理图
- 当前仓库 STM32H7 HAL 1.11.6 Ethernet 源码

如后续加入 STM32H743 Reference Manual 或官方 Ethernet/LwIP 示例，应在这里补充版本信息。
