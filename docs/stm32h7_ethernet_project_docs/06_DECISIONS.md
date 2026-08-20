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

第一版 Bring-up 使用：

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

第一版采用：

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

第一版验证：

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

- 状态：Proposed
- 日期：2026-08-20

当前倾向：

```text
专用 .eth_dma 区域
+
明确 MPU 属性
+
优先选择易验证的一致性方案
```

是否最终全部使用 Non-cacheable，需要在 M2 根据 STM32H743 内存映射、HAL 数据结构和性能测试确认。

在确认前，任何模块不得假定最终 Cache 策略已经冻结。

---

## D008 PHY Link 检测

- 状态：Accepted
- 日期：2026-08-20

第一版采用周期轮询 PHY Link。

暂不依赖 LAN8720 nINT。

M1 Bring-up 中使用 200 ms polling 进行上板验证，但该周期以及最终承载 Link 管理的任务不作为长期固定参数。

进入 LwIP / `ethernetif` 后，应结合实际网络任务模型重新确定轮询位置和周期。

---

## D009 LwIP 应用 API

- 状态：Proposed
- 日期：2026-08-20

用于 Echo/Bring-up 的应用 API 暂未冻结。

候选：

- Socket API：调试直观；
- Netconn API；
- Raw API：开销低，但线程模型要求更严格。

在 M3 前确定。

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

## D011 M0 FreeRTOS 与 HAL 时间基线

- 状态：Accepted
- 日期：2026-08-20

M0 工程使用 FreeRTOS，并由 CubeMX 以 CMSIS-RTOS v2 接口生成基础任务框架。

时间基线采用：

```text
TIM6    → HAL 1 ms Tick / HAL timeout
SysTick → FreeRTOS Kernel Tick
```

后续 Ethernet ISR 如需调用 FreeRTOS FromISR API，必须基于实际 `FreeRTOSConfig.h` 和 NVIC 配置重新核对中断优先级约束。

本决定不冻结未来 Ethernet Task、`tcpip_thread` 或 ETH IRQ 的优先级。

---

## D012 M0 基础调试输出

- 状态：Accepted
- 日期：2026-08-20

第一验证板的 M0 基础调试输出使用：

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
docs/stm32h7_ethernet_project_docs/**
顶层 CMakeLists.txt 与项目脚本
```

不为了占位而提前创建 M1/M2/M3 的空源文件；实际进入对应模块时再建立文件和接口。

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

PHY ready 不依赖固定延时判断，而通过 PHY ID polling + timeout 确认。

对 MDIO 返回的 `0xFFFF` 不作为有效 PHY 状态解释，避免将 PHY 无响应错误判断为有效 Link 状态。
