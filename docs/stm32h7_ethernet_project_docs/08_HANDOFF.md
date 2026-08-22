# Latest Handoff

- 来源工作单元：M2 异步 RX + Driver Package 产品化整理 + CubeMX Task 边界收尾
- 日期：2026-08-22
- 当前阶段：M2 MAC / DMA
- 当前远程定位：根 `Ethernet/` 为 Driver Package，完整 STM32H743 Demo 位于 `examples/`

## 1. 本工作单元完成状态

本轮已经完成并验证：

```text
Driver Package
→ Port / RTOS Adapter 分层
→ 完整 Reference Example 移入 examples/
→ CubeMX EthernetRtos_RxTask + As weak
→ Package 强定义 Task Entry
→ Build / map / On-board 回归
```

当前 `As weak` 方案已由 D023 Accepted。

## 2. 当前 RX Runtime 已验证路径

```text
ETH IRQ
→ HAL_ETH_IRQHandler()
→ HAL_ETH_RxCpltCallback()
→ Driver RX event
→ EthernetRtos_OnRxEvent()
→ CMSIS-RTOS2 Thread Flag
→ EthernetRtos_RxTask()
→ EthernetDriver_Receive()
→ Demo / Application Frame Handler
→ RX Buffer recycle
```

async RX 1000 / 1000 已通过当前 Reference Example 回归。

该测试验证基础异步机制和 ownership，不代表高负载 Stress。

## 3. 当前仓库结构

```text
Ethernet/                                  ← 可复制 Driver Package
README.md                                  ← Driver Integration Guide
examples/STM32H743_LAN8720_FreeRTOS/       ← 完整 Reference Example
docs/ETHERNET_RUNTIME_FLOW.md              ← Runtime / callback 原理说明
docs/stm32h7_ethernet_project_docs/        ← 项目与专题文档
```

Reference Example 包含：

```text
BSP/
Core/
Drivers/
Middlewares/
cmake/
CMakeLists.txt
CMakePresets.json
STM32H743xx_FLASH.ld
stm32H7ethernet_demo.ioc
startup_stm32h743xx.s
build.sh
flash.sh
README.md
```

Example CMake 通过 `../../Ethernet` 引用根 Package；`cmake/stm32cubemx/CMakeLists.txt` 未被项目手工接管。

## 4. CubeMX Task 当前冻结方式

Reference Example：

```text
Task Name  : EthernetRxTask
Entry      : EthernetRtos_RxTask
Generation : As weak
```

CubeMX 负责：

```text
Task object
priority
stack
allocation
osThreadNew()
weak Task stub
```

Package 负责：

```text
EthernetRtos_RxTask() strong implementation
RX task handle
Driver RX event registration
Thread Flag wait
drain EthernetDriver_Receive()
Frame Handler dispatch
```

因此不存在 `StartEthernetRxTask()` wrapper。

普通非 CubeMX 用户仍可自己用 RTOS API 创建 Task，入口指向 `EthernetRtos_RxTask()`。

## 5. 两层运行时 Handler

当前真正的运行时注册只有两层：

```text
EthernetDriver_SetRxEventHandler()
→ Driver → RTOS Adapter
→ 由 EthernetRtos_RxTask() 内部自动完成

EthernetRtos_SetRxFrameHandler()
→ RTOS Adapter → Application / ethernetif
→ 上层决定完整 Frame 最终交给谁
```

HAL 的 `HAL_ETH_RxCpltCallback()`、`HAL_ETH_RxLinkCallback()`、`HAL_ETH_RxAllocateCallback()` 是 HAL 固定 callback；CubeMX `__weak EthernetRtos_RxTask()` 是链接阶段替换机制。它们不要和运行时 Handler 注册混为一类。

详细说明见：

```text
docs/ETHERNET_RUNTIME_FLOW.md
```

## 6. 当前 DMA / ownership 不变事实

```text
RAM_ETH      = 0x30040000 / 32 KiB
RX Desc      = 0x30040000
TX Desc      = 0x30040080
RX Pool      = 0x30042000 / 0x1800 / 4 × 1536 B
TX Pool      = 0x30044000 / 0x1800 / 4 × 1536 B
```

RX：

```text
DMA Buffer
→ HAL_ETH_RxLinkCallback()
→ copy Driver CPU Frame
→ 立即 release RX Buffer
→ EthernetDriver_Receive() 再 copy 给 RTOS Adapter
→ Frame Handler
```

当前仍是 copy-first，未进入 Zero Copy。

## 7. 当前测试等级

已确认：

- Static Review：PASS；
- Debug Build：PASS；
- Release Build：PASS；
- map / ELF DMA layout：PASS；
- On-board PHY / MAC startup：PASS；
- On-board async RX 1000 / 1000：PASS；
- CubeMX `As weak` Generate Code / strong implementation linkage：PASS。

未完成：Measured 高负载性能、长时间 Stress、D-Cache-on。

## 8. 当前仍未完成

- Async TX completion ownership；
- DMA error / timeout recovery；
- RX/TX error/drop 统计；
- 完整 Link Down / Up MAC lifecycle；
- Task stack high-water mark；
- D-Cache-on；
- LwIP / Ping / UDP / TCP；
- 高负载 / 长时间 Stress。

## 9. 下一工作单元建议

下一工作单元优先只做：

> **Async TX completion ownership**

需要先基于当前 HAL 1.11.6 再确认：

```text
HAL_ETH_Transmit_IT()
HAL ETH TX complete interrupt
HAL_ETH_ReleaseTxPacket()
HAL_ETH_TxFreeCallback()
```

再设计：

```text
Caller Frame
→ TX DMA Buffer
→ async submit
→ DMA ownership
→ TX completion event
→ task-side release
→ Buffer recycle
```

不要同时进入 LwIP 或完整 Link lifecycle。