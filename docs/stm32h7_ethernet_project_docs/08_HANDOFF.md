# Latest Handoff

- 来源工作单元：M2 异步 RX 收尾 + Driver Package 第二阶段产品化整理
- 日期：2026-08-22
- 当前阶段：M2 MAC / DMA
- 当前远程目标：根 `Ethernet/` 为产品，完整 STM32H743 Demo 收敛到 `examples/`

## 1. 已有 On-board 基线

第一轮 Package 化后已经实测：

```text
[ETH] EthernetRxTask started
[ETH] BootstrapTask started
[ETH] PHY ready
[ETH] Auto-negotiation started
[ETH] Link up
[ETH] Speed=100M
[ETH] Duplex=Full
[ETH] MAC/DMA started
[ETH] Async RX test 1000/1000 PASS, total=1000
```

已确认路径：

```text
ETH IRQ
→ HAL_ETH_IRQHandler()
→ Driver RX event
→ CMSIS-RTOS2 Thread Flag
→ EthernetRtos_RxTask()
→ EthernetDriver_Receive()
→ Demo Frame Handler
→ RX Buffer recycle
```

验证等级：On-board Verified。约 5 ms / Frame，仅验证异步机制与基础 ownership，不代表 Stress。

## 2. 第二阶段仓库结构

本工作单元把仓库定位落实为：

```text
Ethernet/                                  ← 可复制 Driver Package
README.md                                  ← Driver Integration Guide
examples/STM32H743_LAN8720_FreeRTOS/       ← 完整 Reference Example
docs/stm32h7_ethernet_project_docs/        ← 项目与专题文档
```

Reference Example 现在包含：

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

Example 顶层 CMake 通过：

```cmake
set(ETHERNET_PACKAGE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Ethernet")
```

引用仓库根 Package。CubeMX 生成的 `cmake/stm32cubemx/CMakeLists.txt` 只是整体移动，没有手工修改；其对 Example 内 `Core/Drivers/Middlewares` 的相对路径保持不变。

## 3. README / 文档整理

根 README 已改为完整用户接入指南，重点覆盖：

```text
Package 架构
→ CubeMX ETH / MPU / NVIC / RTOS
→ DMA SRAM 选择
→ linker MEMORY / Descriptor / RX/TX Pool / ASSERT
→ map / ELF 验证
→ Port 实现
→ 构建系统接入
→ 初始化 / PHY / Frame API
→ CMSIS-RTOS2 async RX
→ 新板迁移检查表
```

原 `docs/BOARD_PORTING.md` 与根 README 重复且仍包含旧路径/API，因此移除，迁移入口统一为根 README。

`docs/stm32h7_ethernet_project_docs/00_PROJECT.md`～`08_HANDOFF.md` 路径暂时不移动，保持项目跨对话状态读取稳定。

## 4. CubeMX Task 方式

D021 不变：RTOS Adapter 不创建 Task。

当前倾向：

```text
Task Entry : EthernetRtos_RxTask
Generation : As weak
```

已确认的外部证据：CubeMX 6.18.1 生成的 `As weak` Task Entry 使用 `__weak`，同时 CubeMX 仍生成 Task attributes 和 `osThreadNew()`；这与 Package 提供强定义 Entry 的边界匹配。

因此新增 D023 Proposed。**本 Reference Example 尚未在 CubeMX UI 中切换后 Generate Code / Build / On-board**，所以不要把 D023 写成 Accepted，也不要手工修改生成 `freertos.c` 冒充 CubeMX 结果。

## 5. 当前验证等级

第二阶段目录移动本身：

```text
Static Review only
```

尚未执行：

```text
fresh Debug build
fresh Release build
new-path map / ELF check
new-path On-board boot
new-path async RX 1000 / 1000
current Example CubeMX Generate Code regression
```

第一轮 Package 化的既有 Debug Build / On-board 结果仍有效，但不能自动等价为第二阶段移动后的提交已验证。

## 6. 当前 DMA 不变事实

```text
RAM_ETH      = 0x30040000 / 32 KiB
RX Desc      = 0x30040000
TX Desc      = 0x30040080
RX Pool      = 0x30042000 / 0x1800 / 4 × 1536 B
TX Pool      = 0x30044000 / 0x1800 / 4 × 1536 B
```

本次只改变仓库路径和 Example build reference，不改变物理 DMA layout 或 copy-based ownership。

## 7. 下一次开始时先做

```bash
cd examples/STM32H743_LAN8720_FreeRTOS
./build.sh Debug --fresh
./build.sh Release --fresh

grep -E "RxDescripSection|TxDescripSection|eth_dma_rx|eth_dma_tx|DMARxDscrTab|DMATxDscrTab" \
  build/Debug/stm32H7ethernet_demo.map
```

然后烧录并重新发送 1000 个 `0x88B5` Frame，确认 async RX 1000 / 1000。

完成路径回归后，再用 CubeMX 6.18.1 把 Ethernet RX Task Entry 调整为 `EthernetRtos_RxTask` + `As weak`，Generate Code 后检查 `.ioc` 与 `Core/Src/freertos.c` diff，再执行同样 Build / On-board 回归。

## 8. 仍未完成

- Async TX completion ownership；
- DMA error / timeout recovery；
- RX/TX error/drop 统计；
- 完整 Link lifecycle；
- D-Cache-on；
- LwIP / Ping / UDP / TCP；
- 高负载 / 长时间 Stress。
