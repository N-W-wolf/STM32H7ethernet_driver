# Project Status

- 更新时间：2026-08-21
- 当前阶段：M2 — STM32H743 MAC / DMA
- 当前状态：Polling MAC/DMA Frame 数据路径已建立并完成裸 TX、裸 RX 与连续 1000 帧 RX 上板验证；ETH IRQ + FreeRTOS 异步收发尚未实现。

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

### MAC / DMA 内存与 ownership

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
- [x] RX Buffer Pool：`0x30042000` / 4 × 1536 B；
- [x] TX Buffer Pool：`0x30044000` / 4 × 1536 B；
- [x] Buffer section linker ASSERT；
- [x] Build / map 验证 Descriptor 与 Buffer 实际地址；
- [x] HAL RX Allocate / Link callback ownership；
- [x] polling TX success-path ownership；
- [x] CubeMX MMT 管理边界已明确。

### MAC / DMA polling 数据路径

- [x] PHY Speed / Duplex → MAC 配置；
- [x] `HAL_ETH_Start()`；
- [x] `EthernetDriver_Transmit()`；
- [x] `EthernetDriver_Receive()`；
- [x] 裸 Ethernet Frame TX；
- [x] 裸 Ethernet Frame RX；
- [x] 连续 1000 帧 RX Buffer recycle。

### 文档约定

- [x] README 保持产品 / 库视角；
- [x] 面向使用者的技术文档不展示内部开发阶段；
- [x] 项目控制文档保留里程碑 / 状态 / Handoff；
- [x] 建立 `docs/BOARD_PORTING.md`；
- [x] `03_MEMORY_DMA.md` 记录当前有效 DMA / MPU / linker / Buffer ownership 设计；
- [x] 确定板级 linker 采用显式配置 + map/ELF 验证，不使用 regex patch 脚本；
- [x] `01_ARCHITECTURE.md` 作为唯一架构技术文档。

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
RX Desc      = 96 B
TX Desc      = 96 B
RX Pool      = 0x30042000 / 0x1800
TX Pool      = 0x30044000 / 0x1800
```

Descriptor / Buffer linker ASSERT 全部通过。

### Raw Frame On-board Verified

测试固件基线：`e50bf6a4ce9c3763e6b863b5982522b4e60ac197`。

TX：

```text
Source MAC  = 00:80:E1:00:00:00
Destination = FF:FF:FF:FF:FF:FF
EtherType   = 0x88B5
Length      = 60 B
Payload     = STM32H7 raw Ethernet TX
```

PC `tcpdump` 实际抓包成功。

RX：

```text
EtherType = 0x88B5
Length    = 60 B
Payload   = PC -> STM32H7 raw Ethernet RX
```

结果：

```text
单帧 RX         PASS
连续 RX         1000 / 1000 PASS
PC 发送间隔     约 5 ms / Frame
```

连续 1000 帧仅证明基础 Buffer recycle，没有完成高负载或长时间压力测试。

## 3. 当前代码接口

### Board Ethernet BSP

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

### Ethernet Driver

```text
EthernetDriver_Init()
EthernetDriver_ConfigureLink()
EthernetDriver_Start()
EthernetDriver_Transmit()
EthernetDriver_Receive()
```

当前 `Transmit()` / `Receive()` 为 polling Frame API。启动路径在 PHY 首次 Auto-negotiation 成功后同步 MAC Speed / Duplex 并启动 MAC/DMA。

已完成的 `0x88B5` 裸 Frame 测试代码不再保留在正常启动路径；测试证据记录在 `05_TEST_PLAN.md` 和本文件。

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

- [ ] ETH IRQ；
- [ ] FreeRTOS 异步 RX；
- [ ] FreeRTOS 异步 TX completion；
- [ ] RX/TX 错误统计；
- [ ] DMA error / timeout recovery；
- [ ] Link Down / Up 完整 MAC lifecycle；
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
RX Buffer Pool    : 0x30042000 / 4 × 1536 B
TX Buffer Pool    : 0x30044000 / 4 × 1536 B
```

## 6. 当前尚未冻结的设计

以下内容不能在新对话中自行假定：

- Cache 开启后的最终策略；
- ETH IRQ 优先级；
- Ethernet RX Task 的最终优先级 / 栈大小；
- RX notification 最终机制；
- 异步 TX completion ownership；
- DMA error recovery；
- MAC Link change 时的完整启停策略；
- LwIP Heap / Pool 参数；
- `tcpip_thread` 参数；
- Socket / Netconn / Raw API 最终选择；
- 最终 Link poll 周期和承载任务。

当前 RX/TX Buffer 数量、大小、section、板级地址和 copy-based polling ownership 已由 D019 Accepted，不再列为未冻结项。

## 7. 下一工作单元建议

继续 M2，只推进：

```text
ETH IRQ
→ HAL callback
→ FreeRTOS Task Notification
→ RX Task
→ HAL_ETH_ReadData() / EthernetDriver_Receive()
```

边界：

- 先只异步化 RX；
- 不同时实现异步 TX；
- 不进入 LwIP；
- 不实现 Ping / UDP / TCP；
- 不在本工作单元顺手完成完整 Link lifecycle。

完成标准：ETH IRQ 能稳定唤醒 RX Task，ISR 保持短小，RX Frame 在任务上下文读取，连续收包下没有明显 ownership 泄漏或丢失。

## 8. 更新规则

完成每个工作单元后更新：

- 已完成项；
- 当前未完成项；
- 新硬件事实；
- 新 Accepted / Proposed 决策；
- 测试等级和结果；
- 下一工作单元建议。

`07_STATUS.md` 只描述当前事实，不承担完整讨论历史。
