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

仍需在 M1 Bring-up 时完成两项确认：

1. 核对 `nINTSEL` strap 的实际电阻/LED连接；
2. 上板测量 PA1 / REFCLKO 是否稳定输出约 50 MHz。

在这两项完成前，不把 nINTSEL 的最终逻辑电平写死为已实测事实。

## 4. PHY Reset

当前原理图：

```text
STM32H743 PC0
    ↓
PC0_ETH_RESET
    ↓
LAN8720AI nRST
```

PHY reset 的有效电平和最小时序必须按 LAN8720A Datasheet 实现。

具体 Reset 低电平持续时间、释放后等待时间在 PHY 驱动任务中确定并记录到 `DECISIONS.md`。

## 5. MDIO / MDC

当前原理图：

```text
STM32 PA2  ↔ LAN8720 MDIO
STM32 PC1  → LAN8720 MDC
```

PHY Driver 不应直接操作 STM32 GPIO bit-bang MDIO；第一版计划通过 STM32 Ethernet MAC/HAL 提供的 PHY Management 接口访问 PHY Register。

实际 HAL 接口以当前工程所使用的 STM32CubeH7 HAL 版本源码为准。

## 6. PHY Strap：待确认项

以下内容需要在 M1 中结合原理图细节、LAN8720A Datasheet 和寄存器实测确认：

- PHYAD0 → 最终 PHY Address；
- MODE[2:0] → 默认速率 / 双工 / Auto-negotiation 模式；
- nINTSEL → nINT / REFCLKO 选择；
- REGOFF → 内部 1.2 V regulator 配置；
- LED strap 对上述配置的影响。

不要仅根据 LAN8720A 内部默认上下拉直接假定最终 strap 值，因为这些脚同时连接外部电阻、LED 或 MCU 输入。

M1 完成后，应把最终结果补充为：

```text
PHY Address:
MODE[2:0]:
nINTSEL:
REGOFF:
Boot default mode:
```

并给出寄存器读取结果作为验证。

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

## 8. 内存 / DMA

当前尚未冻结 Ethernet DMA 内存布局。

已知项目约束：

- Ethernet DMA 使用的 Descriptor / Buffer 不得放入 DMA 无法访问的内存；
- STM32H7 D-Cache 一致性必须显式处理；
- 第一版倾向划分专用 `.eth_dma` 区域；
- 第一版倾向优先采用简单、易验证的一致性方案。

最终 SRAM 区域、MPU 属性、Descriptor 地址和 Buffer 地址统一在 `03_MEMORY_DMA.md` 中确定。

## 9. 当前硬件验证清单

M1 前后应实际确认：

- [ ] PC0 可正确控制 LAN8720AI nRST；
- [ ] 25 MHz PHY 晶振正常；
- [ ] PA1 可观察到正确 RMII REF_CLK；
- [ ] MDIO 可以读 PHY ID；
- [ ] PHY Address 与 strap 一致；
- [ ] MODE strap 与读取寄存器一致；
- [ ] Link LED 行为正常；
- [ ] 插拔网线可以反映到 PHY Link 状态；
- [ ] Speed / Duplex 可以正确读取。

## 10. 资料基线

当前主要资料：

- 《STM32H7 Ethernet 通用驱动开发指导与规划》
- STM32H743 Datasheet
- LAN8720A/LAN8720Ai Datasheet
- 当前有效 STM32H743VIT6 开发板原理图

如后续加入 STM32H743 Reference Manual、当前 STM32CubeH7 HAL 源码或官方 Ethernet/LwIP 示例，应在这里补充版本信息。
