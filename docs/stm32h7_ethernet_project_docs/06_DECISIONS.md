# Design Decisions

本文件只记录会跨模块影响后续工作的设计决定。

状态含义：

- `Accepted`：后续对话默认必须遵守；
- `Proposed`：当前倾向方案，尚未冻结；
- `Superseded`：已被后续决定替代；
- `Rejected`：讨论过但不采用。

如果要改变 `Accepted` 决策，应新增一条决策说明原因，并将旧决策标记为 `Superseded`，不要直接删除历史。

---

## D001 第一验证平台

- 状态：Accepted
- 日期：2026-08-20

决定：

第一验证平台使用：

```text
STM32H743VIT6
+
LAN8720AI
+
RMII
```

驱动整体目标仍是面向 STM32H7 的可复用组件。

---

## D002 软件运行环境

- 状态：Accepted
- 日期：2026-08-20

采用：

```text
STM32 HAL
FreeRTOS
LwIP
```

Ethernet RX/TX 按 FreeRTOS 异步场景设计。

---

## D003 基础软件分层

- 状态：Accepted
- 日期：2026-08-20

保持：

```text
Application
    ↓
LwIP
    ↓
ethernetif
    ↓
Ethernet Driver
    ├── STM32H7 MAC / DMA
    └── PHY abstraction
             ↓
          PHY Driver
    ↓
BSP / Board Port
```

不允许把 IP/UDP/TCP 或机器人业务塞入 Ethernet Driver。

---

## D004 底层语言边界

- 状态：Accepted
- 日期：2026-08-20

以下部分优先使用 C：

- BSP；
- PHY；
- STM32H7 MAC / DMA；
- `ethernetif`；
- LwIP 接口代码。

未来上层应用可以使用 C 或 C++。

---

## D005 第一版网络能力范围

- 状态：Accepted
- 日期：2026-08-20

基础验证范围：

```text
Static IPv4
ICMP Ping
UDP Echo
TCP Echo
```

暂不加入：

```text
DHCP
DNS
mDNS
TLS
HTTP
```

---

## D006 第一版不主动追求 Zero Copy

- 状态：Accepted
- 日期：2026-08-20

优先完成可验证、稳定的 Ethernet 数据路径。

允许在 `ethernetif` 与 DMA Buffer / LwIP pbuf 之间复制。

只有在正确性、压力测试和性能测量完成后，才评估 Zero Copy。

---

## D007 DMA / Cache 第一版方案

- 状态：Superseded
- 日期：2026-08-20
- 替代：D016

原倾向：

```text
专用 .eth_dma 区域
+
明确 MPU 属性
+
优先选择易验证的一致性方案
```

该决定已由 D016 的 STM32H743 当前板实际 SRAM3 / MPU / linker 方案替代。

---

## D008 PHY Link 检测

- 状态：Accepted
- 日期：2026-08-20

采用周期轮询 PHY Link。

暂不依赖 LAN8720 nINT。

Bring-up 中使用 200 ms polling 进行上板验证，但该周期以及最终承载 Link 管理的任务不作为长期固定参数。

进入 LwIP / `ethernetif` 后，应结合实际网络任务模型重新确定轮询位置和周期。

---

## D009 LwIP 应用 API

- 状态：Proposed
- 日期：2026-08-20

用于 Echo / Bring-up 的应用 API 暂未冻结。

候选：

- Socket API；
- Netconn API；
- Raw API。

在 LwIP Runtime 设计确定时再冻结。

---

## D010 多对话项目状态管理

- 状态：Accepted
- 日期：2026-08-20

聊天历史不作为项目唯一状态源。

所有跨对话的重要信息必须进入：

- 代码；
- `DECISIONS.md`；
- `STATUS.md`；
- `HANDOFF.md`；
- 对应专题文档。

每个新对话只承担一个经过当前对话确认的工作范围。

---

## D011 FreeRTOS 与 HAL 时间基线

- 状态：Accepted
- 日期：2026-08-20

工程使用 FreeRTOS，并由 CubeMX 以 CMSIS-RTOS v2 接口生成基础任务框架。

时间基线采用：

```text
TIM6    → HAL 1 ms Tick / HAL timeout
SysTick → FreeRTOS Kernel Tick
```

Ethernet ISR 如需调用 FreeRTOS FromISR API，必须基于实际 `FreeRTOSConfig.h` 和 NVIC 配置核对中断优先级约束。

本决定不冻结 Ethernet Task、`tcpip_thread` 或 ETH IRQ 的最终优先级。

---

## D012 基础调试输出

- 状态：Accepted
- 日期：2026-08-20

当前验证板基础调试输出使用：

```text
USART1
PA9  = TX
PA10 = RX
115200 / 8N1
```

`printf` 通过 BSP 中的强符号 `_write()` 重定向到 `HAL_UART_Transmit()`。

该路径仅用于低频启动、Bring-up 和诊断信息；禁止在 Ethernet IRQ、高频 RX/TX 快路径中直接 `printf`。

UART 阻塞发送必须带 timeout。

---

## D013 CubeMX 生成代码与手工代码边界

- 状态：Accepted
- 日期：2026-08-20

以下内容按 CubeMX / ST 生成或维护：

```text
stm32H7ethernet_demo.ioc
Core/**
Drivers/CMSIS/**
Drivers/STM32H7xx_HAL_Driver/**
cmake/stm32cubemx/CMakeLists.txt
CubeMX 生成的 Third_Party middleware
```

对 `Core/**` 的长期手工修改只放在 CubeMX `USER CODE BEGIN/END` 区域内，除非后续明确决定接管某个文件。

以下目录用于项目长期手工维护：

```text
BSP/**
Drivers/Ethernet/**
Middlewares/Network/**
App/**
docs/**
顶层 CMakeLists.txt 与项目脚本
```

Ethernet DMA linker 配置的特殊所有权见 D017。

不为了占位提前创建空源文件；实际需要时再建立文件和接口。

---

## D014 PHY Driver 与 RTOS 边界

- 状态：Accepted
- 日期：2026-08-20

LAN8720 PHY Driver 保持与 FreeRTOS 解耦。

PHY Driver 提供非阻塞的状态与控制接口：

```text
Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

Reset 后的等待、轮询周期和 timeout 由调用层负责。

PHY ready 通过 PHY ID polling + timeout 确认。

对 MDIO 返回的 `0xFFFF` 不作为有效 PHY 状态解释，避免将 PHY 无响应错误判断为有效 Link 状态。

---

## D015 文档受众与阶段信息边界

- 状态：Accepted
- 日期：2026-08-21

项目文档分为两类。

面向使用者 / 技术阅读者：

```text
README.md
01_ARCHITECTURE.md
02_HARDWARE_BASELINE.md
03_MEMORY_DMA.md
04_RTOS_NETWORK.md
docs/BOARD_PORTING.md
```

这些文档：

- 不展示 M0/M1/M2 等内部开发阶段；
- 不使用“当前阶段”“下一阶段”“工作单元”等项目推进语义；
- 只描述介绍、支持状态、硬件、架构、环境、使用方式、限制、技术设计和迁移；
- 未实现能力直接标记“未实现”；
- README 不作为开发日志。

项目控制 / 规划文档：

```text
00_PROJECT.md
05_TEST_PLAN.md
06_DECISIONS.md
07_STATUS.md
08_HANDOFF.md
```

这些文档允许记录里程碑、当前阶段、工作单元、Accepted / Proposed 和测试计划。

`01_ARCHITECTURE.md` 作为唯一架构技术文档，不再保留内容重复的《STM32H7 Ethernet 通用驱动开发指导与规划》。

---

## D016 STM32H743 Ethernet DMA 内存与 MPU

- 状态：Accepted
- 日期：2026-08-21

当前 STM32H743VIT6 验证板将 SRAM3 专用于 Ethernet DMA：

```text
RAM_ETH
0x30040000 ~ 0x30047FFF
32 KiB
```

普通 D2 RAM 缩为：

```text
RAM_D2
0x30000000 ~ 0x3003FFFF
256 KiB
```

Descriptor：

```text
RX = 0x30040000
TX = 0x30040080
ETH_RX_DESC_CNT = 4
ETH_TX_DESC_CNT = 4
```

当前 HAL `ETH_DMADescTypeDef` 为 24 B，因此每组 4 个 Descriptor 实际 96 B，各预留 128 B slot。

MPU：

```text
Region 1
0x30040000 / 32 KiB
Normal, Non-cacheable, Non-bufferable, Shareable, XN

Region 2
0x30040000 / 256 B
Device, Non-cacheable, Bufferable, Non-shareable, XN
```

Region 2 以更高编号覆盖 Descriptor 区。

板级代码在 `MX_ETH_Init()` 前显式使能 D2 SRAM3 时钟。

当前 CPU I-Cache / D-Cache 均为 Disabled。

已完成 Build / map 验证：

```text
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
```

本决定只冻结 DMA 专用 SRAM、Descriptor 地址、MPU 和 linker 基础边界；RX/TX Buffer 数量、最终 Buffer section / 地址和 ownership 在该决定形成时仍未冻结，后续由 D019 补充。

---

## D017 板级 linker 与自动化策略

- 状态：Accepted
- 日期：2026-08-21

当前根目录 `STM32H743xx_FLASH.ld` 已包含项目自定义 Ethernet DMA 内存布局，视为当前验证板的项目维护配置。

CubeMX Generate Code 后必须检查 linker diff，避免自定义 `RAM_ETH`、Descriptor section 和 `ASSERT` 被覆盖。

当前不使用脚本通过 regex / 字符串替换自动修改 `.ld`。

自动化优先用于验证：

- `.map` / ELF 地址；
- Descriptor / Buffer alignment；
- DMA region 越界；
- section 非空；
- CI 检查。

只有出现实际多板维护需求时，再评估 board-specific linker 选择或结构化配置 + template 生成。

---

## D018 Ethernet Memory Management Tool 边界

- 状态：Accepted
- 日期：2026-08-21

当前项目不使用 CubeMX Memory Management Tool 自动管理 Ethernet DMA linker section。

`.ioc` 负责保存 ETH Descriptor 地址和 Cortex-M7 MPU Region；项目 linker 负责物理 DMA SRAM 与 section；BSP 负责当前板 DMA SRAM 的时钟准备。

理由：当前 CubeMX 6.18.1 实际试验中，MMT 对 Descriptor / RX Pool region 的生成表达与静态 Buffer Pool 设计不匹配，并出现 linker 配置源不一致风险。

---

## D019 第一版 Payload Buffer 与 ownership

- 状态：Accepted
- 日期：2026-08-21

第一版 Ethernet Frame 数据路径采用静态、copy-based DMA Buffer ownership。

当前通用 Driver 配置：

```text
RX Buffer Count = ETH_RX_DESC_CNT = 4
TX Buffer Count = ETH_TX_DESC_CNT = 4
Buffer Size     = 1536 B
Alignment       = 32 B
```

通用 Driver 只定义：

```text
.eth_dma_buffer.rx
.eth_dma_buffer.tx
```

物理地址由板级 linker 决定。当前 STM32H743VIT6 验证板为：

```text
RX Pool = 0x30042000 / 0x1800 / 4 × 1536 B
TX Pool = 0x30044000 / 0x1800 / 4 × 1536 B
```

RX ownership：

```text
DMA RX Buffer
→ HAL_ETH_RxLinkCallback()
→ memcpy 到 CPU 侧单帧暂存区
→ 立即归还 RX Pool
→ HAL 重建 Descriptor 时重新分配
```

TX polling ownership：

```text
Caller Frame
→ memcpy 到静态 TX DMA Buffer
→ HAL_ETH_Transmit(timeout)
→ HAL_OK 后归还 Buffer
```

HAL TX 错误路径不假定 DMA 已完全放弃 Buffer，因此当前不立即复用该 Buffer；完整 error recovery 后续单独设计。

该设计已完成：

- linker / map 地址与大小验证；
- 裸 Frame TX 上板验证；
- 裸 Frame RX 单帧验证；
- 连续 1000 帧 RX Buffer recycle 上板验证。

本决定不冻结：

- `HAL_ETH_Transmit_IT()` 异步 TX completion ownership；
- DMA error recovery；
- 完整 Link change 生命周期；
- Cacheable Buffer 方案；
- Zero Copy。

Zero Copy 仍遵循 D006：在正确性、压力测试和性能测量完成前不主动引入。
