# Project Status

- 更新时间：2026-08-21
- 当前阶段：M2 — STM32H743 MAC / DMA
- 当前状态：Ethernet DMA 内存、Descriptor 地址、MPU 和 SRAM3 时钟基础已完成；RX/TX Buffer ownership 与裸 Frame 数据路径尚未实现。

## 1. 已完成

### 工程基线

- [x] 明确项目目标：STM32H7 通用 Ethernet 基础组件；
- [x] 确认 STM32 HAL + FreeRTOS + LwIP 场景；
- [x] 确认第一验证 MCU：STM32H743VIT6；
- [x] 确认第一验证 PHY：LAN8720AI；
- [x] 确认 RMII 硬件连接；
- [x] 确认 CubeMX 6.18.1 / STM32CubeH7 V1.13.0 / HAL 1.11.6；
- [x] 建立 CMake + GNU Arm Embedded 构建工程；
- [x] 建立 FreeRTOS / CMSIS-RTOS v2 基础运行环境；
- [x] TIM6 作为 HAL 1 ms Tick，SysTick 用于 FreeRTOS；
- [x] 建立 USART1 调试输出；
- [x] 明确 CubeMX / 项目手工维护边界。

### PHY

- [x] PHY Reset；
- [x] MDIO Read / Write；
- [x] PHY ID；
- [x] PHY Address = 0；
- [x] MODE = 111；
- [x] Auto-negotiation；
- [x] Link Up / Down；
- [x] 100 Mbit/s；
- [x] Full Duplex；
- [x] 单次网线拔出 / 插回恢复。

### MAC / DMA 内存基础

- [x] 核对 STM32H743 Ethernet DMA 可访问 SRAM；
- [x] 不使用 DTCM 作为 Ethernet DMA 内存；
- [x] SRAM3 `0x30040000 ~ 0x30047FFF` 独立为 `RAM_ETH`；
- [x] `RAM_D2` 缩为 `0x30000000 ~ 0x3003FFFF`；
- [x] RX Descriptor 固定 `0x30040000`；
- [x] TX Descriptor 固定 `0x30040080`；
- [x] Descriptor section 32-byte alignment；
- [x] Descriptor 地址 / 大小 / 非空 linker ASSERT；
- [x] MPU Region 1：SRAM3 Normal Non-cacheable；
- [x] MPU Region 2：前 256 B Device Non-cacheable overlay；
- [x] `BoardEthernet_PrepareDmaMemory()` 显式使能 D2 SRAM3 时钟；
- [x] DMA SRAM 在 `MX_ETH_Init()` 前准备；
- [x] Build / map 验证 Descriptor 实际地址；
- [x] CubeMX MMT 管理边界已明确。

### 文档约定

- [x] README 改为产品 / 库视角；
- [x] 面向使用者的技术文档不展示内部开发阶段；
- [x] 项目控制文档保留里程碑 / 状态 / Handoff；
- [x] 建立 `docs/BOARD_PORTING.md`；
- [x] `03_MEMORY_DMA.md` 记录当前有效 DMA / MPU / linker 设计；
- [x] 确定板级 linker 采用显式配置 + map/ELF 验证，不使用 regex patch 脚本；
- [x] 移除重复的《STM32H7 Ethernet 通用驱动开发指导与规划》，统一由 `01_ARCHITECTURE.md` 承担架构技术文档职责。

## 2. 已确认验证结果

### PHY On-board Verified

```text
PHY ID1     = 0x0007
PHY ID2     = 0xC0F1
Reg18       = 0x60E0
PHY Address = 0
MODE        = 111
Link        = Up / Down 可检测
Speed       = 100M
Duplex      = Full
```

单次网线拔出后可检测 Link Down，重新插入后可恢复 Link Up。

### DMA Build / Map Verified

```text
RAM_D2       = 0x30000000 / 256 KiB
RAM_ETH      = 0x30040000 / 32 KiB
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
RX section   = 96 B
TX section   = 96 B
```

Descriptor linker ASSERT 全部通过。

该结果只证明链接布局正确，不等同于 Ethernet DMA Frame 数据路径已经上板验证。

## 3. 当前实现边界

### MPU

```text
Region 1
0x30040000 / 32 KiB
Normal, Non-cacheable, Non-bufferable, Shareable, XN

Region 2
0x30040000 / 256 B
Device, Non-cacheable, Bufferable, Non-shareable, XN
```

当前 CPU I-Cache / D-Cache 均为 Disabled。

### Board Port

```text
BoardEthernet_PhyResetAssert()
BoardEthernet_PhyResetRelease()
BoardEthernet_PrepareDmaMemory()
```

### PHY / MDIO

```text
EthernetMdio_Read()
EthernetMdio_Write()
Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

## 4. 当前未完成

### M1 补充测试

- [ ] 10 Mbit/s 实际链路；
- [ ] Half Duplex 实际链路；
- [ ] 连续多次插拔网线；
- [ ] 多次 STM32 重启；
- [ ] 多次 PHY Reset；
- [ ] 25 MHz PHY 晶振独立测量；
- [ ] PA1 / RMII_REF_CLK 独立测量；
- [ ] Link / Speed LED 专项验证。

### M2

- [ ] RX Buffer Pool；
- [ ] TX Buffer Pool；
- [ ] HAL RX Allocate / Link callback ownership；
- [ ] TX Buffer ownership；
- [ ] Buffer linker section / 实际地址；
- [ ] MAC Speed / Duplex 与 PHY 状态同步；
- [ ] 裸 Ethernet Frame TX；
- [ ] 裸 Ethernet Frame RX；
- [ ] ETH IRQ；
- [ ] FreeRTOS 异步 RX/TX；
- [ ] RX/TX 错误统计；
- [ ] DMA 数据路径上板验证；
- [ ] 长时间 / 高负载稳定性；
- [ ] Cache 开启后的专项验证。

### M3 ～ M6

尚未进入。

## 5. 当前工程基线

```text
MCU               : STM32H743VIT6
PHY               : LAN8720AI
PHY Address       : 0
PHY Boot MODE     : 111 / All capable + Auto-negotiation
MAC/PHY Interface : RMII
RTOS              : FreeRTOS / CMSIS-RTOS v2
Network Stack     : LwIP（尚未接入）
CubeMX            : 6.18.1
STM32CubeH7       : V1.13.0
HAL component     : H7 HAL 1.11.6
Build             : CMake + GNU Arm Embedded Toolchain
HAL Timebase      : TIM6
Debug UART        : USART1, 115200 8N1
CPU D-Cache       : Disabled
Ethernet DMA RAM  : SRAM3 / 0x30040000 / 32 KiB
```

## 6. 当前尚未冻结的设计

以下内容不能在新对话中自行假定：

- RX Buffer 数量；
- TX Buffer 数量；
- RX/TX Buffer 最终地址；
- RX/TX ownership；
- Cache 开启后的最终策略；
- ETH IRQ 优先级；
- Ethernet RX Task / notification 机制；
- MAC Link change 时的完整启停策略；
- LwIP Heap / Pool 参数；
- `tcpip_thread` 参数；
- Socket / Netconn / Raw API 最终选择；
- 最终 Link poll 周期和承载任务。

## 7. 下一工作单元建议

继续 M2，优先完成 RX/TX Buffer Pool 与 HAL 1.11.6 ownership 设计。

建议先确定：

1. HAL `HAL_ETH_Start()` / `HAL_ETH_ReadData()` / RX callbacks 的真实生命周期；
2. RX Descriptor 与 RX Buffer 数量关系；
3. TX Buffer 分配和 completion ownership；
4. Buffer linker section 与 SRAM3 地址；
5. map 验证；
6. 再进入裸 Frame TX / RX。

本工作单元不扩展到 LwIP、Ping、UDP 或 TCP。

## 8. 更新规则

完成每个工作单元后更新：

- 已完成项；
- 当前未完成项；
- 新硬件事实；
- 新 Accepted / Proposed 决策；
- 测试等级和结果；
- 下一工作单元建议。

`07_STATUS.md` 只描述当前事实，不承担完整讨论历史。
