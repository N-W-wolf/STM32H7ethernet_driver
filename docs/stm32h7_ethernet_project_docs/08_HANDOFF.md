# Latest Handoff

- 来源工作单元：M2 ETH IRQ + CMSIS-RTOS2 异步 RX验证，以及第一步 Driver Package 化
- 日期：2026-08-22
- 当前阶段：M2 MAC / DMA
- 当前远程基线：本文件所在提交

## 1. 异步 RX 已完成上板验证

异步 RX 在原始结构测试固件 `6b2f1f4bd153e6e4d119be6679a8dea55e7d4ccd` 上已完成 1000 / 1000；Package 化后再次得到同样结果：

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

Package 化后的已确认路径：

```text
ETH IRQ
→ HAL_ETH_IRQHandler()
→ HAL_ETH_RxCpltCallback()
→ Ethernet Driver RX event
→ CMSIS-RTOS2 Thread Flag
→ EthernetRtos_RxTask()
→ EthernetDriver_Receive()
→ Demo RX Frame Handler
→ RX Buffer recycle
```

该结果是 On-board Verified，但只验证约 5 ms / Frame 的异步机制和 Buffer recycle，不属于高负载压力测试。

## 2. Driver Package 第一轮重构

仓库产品定位：

```text
Ethernet/   = 可复制的 Driver Package
当前根目录 STM32CubeMX 工程 = STM32H743 + LAN8720 参考 Demo
```

当前 Package：

```text
Ethernet/
├── Inc/
│   ├── ethernet_driver.h
│   └── ethernet_mdio.h
├── Src/
│   ├── ethernet_driver.c
│   └── ethernet_mdio.c
├── PHY/
│   └── LAN8720/
│       ├── Inc/lan8720.h
│       └── Src/lan8720.c
├── Port/
│   └── Inc/ethernet_port.h
└── RTOS/
    └── CMSIS_RTOS2/
        ├── Inc/ethernet_rtos.h
        └── Src/ethernet_rtos.c
```

### Port 边界

通用 Driver 不再直接 include Demo `eth.h`，也不直接依赖全局变量名 `heth`。

Port API：

```text
EthernetPort_GetHandle()
EthernetPort_PrepareDmaMemory()
EthernetPort_PhyResetAssert()
EthernetPort_PhyResetRelease()
```

当前 STM32H743 参考实现：

```text
BSP/stm32h743vit6_iot/ethernet_port.c
```

只有该文件知道 CubeMX `eth.h`、`heth`、当前 PHY Reset GPIO 和 SRAM3 clock。

本轮回归时发现 `ethernet_port.h` 直接 include `stm32h7xx_hal_eth.h` 会触发 HAL 递归包含顺序问题，表现为大量 `HAL_StatusTypeDef` / `HAL_LockTypeDef` 未定义。已改为 include `stm32h7xx_hal.h`；修复后 Debug 构建和上板测试正常。

### Driver RX Event 边界

Driver 提供：

```text
EthernetDriver_SetRxEventHandler()
```

`HAL_ETH_RxCpltCallback()` 由 Driver Core 提供强定义，只把 RX complete 事件转交给轻量 ISR handler，不在 ISR 中读取 Frame。

### CMSIS-RTOS2 Adapter

Adapter API：

```text
EthernetRtos_SetRxFrameHandler()
EthernetRtos_IsReady()
EthernetRtos_RxTask()
```

边界：

- Adapter 不调用 `osThreadNew()`；
- Task priority / stack / static-dynamic allocation 由应用或 CubeMX 管理；
- Task 启动后通过 `osThreadGetId()` 获取自身 handle；
- Driver RX event 在 ISR 中调用 `osThreadFlagsSet()`；
- Task 被唤醒后持续 drain `EthernetDriver_Receive()` 直到 `ETHERNET_RX_NONE`；
- 完整 Frame 在任务上下文通过同步 Handler 向上交付；
- Frame pointer 只在 Handler 调用期间有效。

当前 CubeMX Demo 暂时保留生成的 `StartEthernetRxTask()` wrapper：

```text
StartEthernetRxTask()
→ 注册 Demo 0x88B5 Frame Handler
→ EthernetRtos_RxTask()
```

因此测试语义仍留在 Demo，Driver Package 不认识 EtherType `0x88B5`。

## 3. README 与用户接入定位

根 `README.md` 已重写为 Driver Integration Guide，主要顺序：

```text
Driver 架构
→ 接入流程
→ CubeMX ETH / MPU / NVIC / FreeRTOS 配置
→ DMA SRAM 选择原则
→ 完整 linker MEMORY / Descriptor / RX/TX Pool / ASSERT 示例
→ Port 实现
→ 初始化顺序
→ Driver / PHY API
→ CMSIS-RTOS2 异步 RX
→ 构建系统说明
→ 当前验证状态
```

根 README 不再把本仓库 CMake / build.sh / flash.sh 当作 Driver 的固定使用方式。

## 4. 当前测试等级

### 已确认

- PHY Bring-up：On-board Verified；
- polling Raw TX：On-board Verified；
- polling Raw RX 1000 / 1000：On-board Verified；
- ETH IRQ + CMSIS-RTOS2 async RX 1000 / 1000：On-board Verified；
- Driver Package 第一轮重构：Debug Build Verified；
- Driver Package 第一轮重构：PHY / MAC startup On-board Verified；
- Driver Package 第一轮重构：Async RX 1000 / 1000 On-board Verified。

### 仍待补充

```text
fresh Release build
当前 Package 提交下的 map / ELF 再检查
CubeMX Generate Code 后边界回归
```

历史 DMA 布局已经 Build / Map Verified，Package 化没有修改 linker 或 DMA section，但在新 Package 提交下仍建议再次检查 map / ELF，不能仅凭“代码未改 linker”替代验证。

## 5. 当前仍未完成

- `As external` 与 `As weak` 的 CubeMX 6.18.1 实际生成比较；
- 整个参考 Demo 移入 `examples/`；
- 用户技术文档与项目控制文档目录重整；
- 清理 `docs/BOARD_PORTING.md` 等已被新 README 覆盖的重复迁移内容；
- 异步 TX completion ownership；
- DMA error / timeout recovery；
- RX/TX error / drop 统计；
- Link Down / Up 完整 MAC lifecycle；
- D-Cache 开启后的专项验证；
- LwIP / Ping / UDP / TCP。

## 6. 下一工作单元

Package 化第一轮功能回归已通过，不需要再回退到旧 `Drivers/Ethernet` 结构。

下一工作单元优先继续仓库产品化整理：

```text
1. 完整参考 Demo → examples/
2. 技术文档 / 项目控制文档重新分组
3. CubeMX task As external / As weak 方式实测并冻结
4. 补 Release / map 回归
```

暂不同时进入异步 TX。

## 7. 关键不变事实

当前 STM32H743 参考板 DMA 布局仍为：

```text
RAM_ETH      = 0x30040000 / 32 KiB
RX Desc      = 0x30040000
TX Desc      = 0x30040080
RX Pool      = 0x30042000 / 0x1800 / 4 × 1536 B
TX Pool      = 0x30044000 / 0x1800 / 4 × 1536 B
```

本次 Package 化没有改变物理 DMA 内存方案和 copy-based ownership。
