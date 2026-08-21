# Latest Handoff

- 来源工作单元：M2 RX/TX Buffer Pool + Polling Raw Frame 数据路径
- 日期：2026-08-21
- 当前阶段：M2 MAC / DMA

## 1. 本次完成内容

### DMA Payload Buffer Pool

当前通用 Driver 使用：

```text
ETH_RX_DESC_CNT = 4
ETH_TX_DESC_CNT = 4
RX Buffer       = 4 × 1536 B
TX Buffer       = 4 × 1536 B
Alignment       = 32 B
```

当前 STM32H743VIT6 板级 linker：

```text
RAM_ETH      = 0x30040000 / 32 KiB
RX Desc      = 0x30040000 / 96 B
TX Desc      = 0x30040080 / 96 B
RX Pool      = 0x30042000 / 0x1800
TX Pool      = 0x30044000 / 0x1800
```

linker 使用 `.eth_dma_rx` / `.eth_dma_tx` output section 和精确地址 / 大小 ASSERT。

### RX ownership

已实现强符号：

```text
HAL_ETH_RxAllocateCallback()
HAL_ETH_RxLinkCallback()
```

当前路径：

```text
RX DMA Buffer
→ HAL_ETH_ReadData()
→ HAL_ETH_RxLinkCallback()
→ memcpy 到 CPU 侧 g_rx_frame
→ 立即释放 RX DMA Buffer
→ HAL ETH_UpdateDescriptor() 重建 Descriptor
→ EthernetDriver_Receive() 再复制给调用者
```

上层不持有 DMA RX Buffer。

### TX ownership

当前使用 polling：

```text
Caller Frame
→ acquire static TX DMA Buffer
→ memcpy
→ HAL_ETH_Transmit(timeout)
→ HAL_OK 后 release TX Buffer
```

HAL 发送错误时当前不立即复用 Buffer，完整 error recovery 尚未实现。

### MAC / DMA 启动

首次 PHY Auto-negotiation 成功后：

```text
Lan8720Status
→ Speed / Duplex 映射
→ EthernetDriver_ConfigureLink()
→ EthernetDriver_Start()
```

当前 Bootstrap 只负责首次同步和启动；长期 Link polling 尚未驱动完整 Stop / Reconfigure / Start 生命周期。

### Bring-up 测试代码整理

完成裸 Frame 验证后，`Core/Src/freertos.c` 不再在每次正常启动时发送 `0x88B5` 测试帧或等待 1000 帧 PC 测试流量。

正常启动只保留：

- PHY Reset / ready；
- Auto-negotiation；
- Link / Speed / Duplex 输出；
- MAC Speed / Duplex 同步；
- MAC/DMA Start；
- PHY Link polling。

测试证据保留在项目控制文档中。

## 2. 当前代码接口

### Board Ethernet BSP

```text
BoardEthernet_PhyResetAssert()
BoardEthernet_PhyResetRelease()
BoardEthernet_PrepareDmaMemory()
```

### MDIO Wrapper

```text
EthernetMdio_Read()
EthernetMdio_Write()
```

### LAN8720 PHY Driver

```text
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

`EthernetDriver_Transmit()` / `Receive()` 当前为 polling Frame API。

## 3. 文件所有权

CubeMX / ST 管理：

```text
stm32H7ethernet_demo.ioc
Core/**
Drivers/CMSIS/**
Drivers/STM32H7xx_HAL_Driver/**
cmake/stm32cubemx/CMakeLists.txt
CubeMX 生成的 Third_Party middleware
```

`Core/**` 的长期手工代码只允许写入 USER CODE 区域。

项目维护：

```text
BSP/**
Drivers/Ethernet/**
Middlewares/Network/**
App/**
docs/**
顶层 CMakeLists.txt
项目脚本
STM32H743xx_FLASH.ld 中的 Ethernet DMA 配置
```

当前项目不使用 CubeMX MMT 自动生成 Ethernet DMA linker section。

## 4. 已执行测试与结果

### Static Review

- [x] HAL 1.11.6 RX Allocate / Link / ReadData 生命周期核对；
- [x] RX Descriptor 与静态 RX Buffer ownership 核对；
- [x] polling TX success-path ownership 核对；
- [x] SRAM3 / MPU / linker / section 对齐核对；
- [x] PHY → MAC Speed / Duplex 同步顺序核对。

### Build / Map Verified

- [x] `DMARxDscrTab = 0x30040000`；
- [x] `DMATxDscrTab = 0x30040080`；
- [x] `.eth_dma_rx = 0x30042000 / 0x1800`；
- [x] `.eth_dma_tx = 0x30044000 / 0x1800`；
- [x] Buffer / Descriptor linker ASSERT 通过。

### On-board Verified

测试固件基线：`e50bf6a4ce9c3763e6b863b5982522b4e60ac197`。

TX：

```text
00:80:E1:00:00:00 → FF:FF:FF:FF:FF:FF
EtherType 0x88B5
60 B
Payload: STM32H7 raw Ethernet TX
```

PC `tcpdump` 实际抓包成功。

RX：

```text
PC → 00:80:E1:00:00:00
EtherType 0x88B5
60 B
Payload: PC -> STM32H7 raw Ethernet RX
```

结果：

```text
Single RX      PASS
Continuous RX  1000 / 1000 PASS
PC interval    ≈ 5 ms / Frame
```

该测试验证基础 RX Buffer recycle，不属于高负载压力测试。

### Measured

没有新增示波器 / 逻辑分析仪测量结果。

## 5. Accepted 决策

本工作单元新增：

- D019：第一版 Payload Buffer 与 copy-based ownership。

D019 冻结：

- RX/TX 各 4 个 1536 B 静态 DMA Buffer；
- 32 B alignment；
- 通用 Driver input section；
- 当前板 RX/TX Pool 地址；
- RX callback copy + immediate recycle；
- TX polling success-path recycle。

D019 不冻结异步 TX、错误恢复、Link lifecycle、Cacheable Buffer 或 Zero Copy。

## 6. 当前尚未解决

M2 关键未完成项：

- ETH IRQ；
- FreeRTOS 异步 RX；
- FreeRTOS 异步 TX completion；
- RX/TX 错误 / drop 统计；
- DMA error / timeout recovery；
- Link Down / Up 完整 MAC lifecycle；
- 长时间 / 高负载稳定性；
- D-Cache 开启后的专项验证。

注意：当前正常启动后，如果首次 Auto-negotiation 未成功而稍后才 Link Up，长期 PHY polling 只会打印状态，不会自动启动 MAC/DMA。该行为属于尚未实现的完整 Link lifecycle，不要误判为已解决。

## 7. 下一工作单元开始时优先读取

1. `00_PROJECT.md`；
2. `01_ARCHITECTURE.md`；
3. `03_MEMORY_DMA.md`；
4. `04_RTOS_NETWORK.md`；
5. `05_TEST_PLAN.md`；
6. `06_DECISIONS.md`；
7. `07_STATUS.md`；
8. 本文件；
9. `Drivers/Ethernet/Inc/ethernet_driver.h`；
10. `Drivers/Ethernet/Src/ethernet_driver.c`；
11. `Core/Src/freertos.c`；
12. `Core/Src/stm32h7xx_it.c` / `Core/Inc/stm32h7xx_it.h`；
13. 当前 HAL 1.11.6 `HAL_ETH_IRQHandler()` / callbacks；
14. `FreeRTOSConfig.h`；
15. `.ioc` 中 ETH NVIC 配置。

## 8. 下一工作单元推荐边界

继续 M2：ETH IRQ + FreeRTOS 异步 RX。

目标最小路径：

```text
ETH IRQ
→ HAL_ETH_IRQHandler()
→ RX complete callback
→ Task Notification FromISR
→ Ethernet RX Task
→ HAL_ETH_ReadData() / EthernetDriver_Receive()
```

本工作单元先只异步化 RX。

范围外：

- 异步 TX completion；
- 完整 Link lifecycle；
- LwIP；
- Ping；
- UDP / TCP；
- 机器人 HostLink / 业务协议。

完成标准：ISR 保持短小；IRQ 可以可靠唤醒 RX Task；Frame 在任务上下文读取；连续收包下 RX Buffer ownership 不泄漏；中断优先级符合 FreeRTOS FromISR API 约束。
