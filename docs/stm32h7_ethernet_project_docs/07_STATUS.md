# Project Status

- 更新时间：2026-08-22
- 当前阶段：M2 — MAC / DMA Runtime
- 当前状态：Driver Package 产品化整理、Reference Example 目录迁移以及 CubeMX `As weak` RX Task 集成均已完成 Build / Map / On-board 回归；D023 已 Accepted。下一工作单元可进入 Async TX completion ownership。

## 1. 已确认完成

### 硬件 / PHY

- [x] STM32H743VIT6 + LAN8720AI + RMII；
- [x] PHY Reset / MDIO / ID / Address / Strap；
- [x] Auto-negotiation、Link、100M Full Duplex；
- [x] USART1 调试输出。

### DMA / Memory

- [x] RAM_ETH = SRAM3 `0x30040000 / 32 KiB`；
- [x] RX Desc `0x30040000`；
- [x] TX Desc `0x30040080`；
- [x] RX Pool `0x30042000 / 0x1800 / 4×1536 B`；
- [x] TX Pool `0x30044000 / 0x1800 / 4×1536 B`；
- [x] MPU Non-cacheable + Descriptor Device overlay；
- [x] linker ASSERT / map 验证；
- [x] copy-based RX recycle；
- [x] polling TX success ownership。

### MAC / DMA Runtime

- [x] PHY Speed / Duplex → MAC；
- [x] `HAL_ETH_Start_IT()`；
- [x] Raw TX / RX；
- [x] polling RX 1000 / 1000；
- [x] ETH IRQ；
- [x] Driver generic RX event；
- [x] CMSIS-RTOS2 Thread Flag；
- [x] `EthernetRtos_RxTask()` drain；
- [x] async RX 1000 / 1000。

### Driver Package / Reference Example

```text
Ethernet/                                  ← Driver Package
examples/STM32H743_LAN8720_FreeRTOS/       ← Reference Example
README.md                                  ← Integration Guide
docs/ETHERNET_RUNTIME_FLOW.md              ← Runtime 原理说明
```

- [x] Driver Core 不再直接依赖 Demo `eth.h` / `heth`；
- [x] Port API 建立；
- [x] CMSIS-RTOS2 Adapter 建立；
- [x] Adapter 不创建 Task；
- [x] Frame Handler 保持任务上下文；
- [x] `0x88B5` 测试逻辑留在 Example；
- [x] 完整 Demo 移入 `examples/`；
- [x] Example CMake 通过 `../../Ethernet` 引用 Package；
- [x] Debug fresh Build；
- [x] Release fresh Build；
- [x] map / ELF 地址回归；
- [x] On-board async RX 1000 / 1000。

### CubeMX RX Task 边界

当前已采用并验证：

```text
Task Entry : EthernetRtos_RxTask
Generation : As weak
```

- [x] CubeMX 6.18.1 Generate Code；
- [x] generated `freertos.c` 产生 weak Task Entry；
- [x] CubeMX 继续管理 Task attributes / `osThreadNew()`；
- [x] Package 同名强定义链接无冲突；
- [x] Build / On-board async RX 回归通过；
- [x] D023 Accepted。

## 2. 当前接口

Port：

```text
EthernetPort_GetHandle()
EthernetPort_PrepareDmaMemory()
EthernetPort_PhyResetAssert()
EthernetPort_PhyResetRelease()
```

Driver / PHY：

```text
EthernetDriver_Init()
EthernetDriver_SetRxEventHandler()
EthernetDriver_ConfigureLink()
EthernetDriver_Start()
EthernetDriver_Transmit()
EthernetDriver_Receive()
EthernetMdio_Read()/Write()
Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

CMSIS-RTOS2：

```text
EthernetRtos_SetRxFrameHandler()
EthernetRtos_IsReady()
EthernetRtos_RxTask()
```

其中 `EthernetDriver_SetRxEventHandler()` 是 Driver → RTOS Adapter 内部绑定；普通用户通常只需要通过 `EthernetRtos_SetRxFrameHandler()` 决定完整 Frame 最终交给谁。

## 3. Runtime 逻辑文档

新增：

```text
docs/ETHERNET_RUNTIME_FLOW.md
```

该文档专门解释：

- CubeMX weak Task Entry 与 Package 强定义；
- HAL 固定 callback；
- 两层运行时 Handler 注册；
- IRQ → Thread Flag → RX Task；
- `HAL_ETH_ReadData()`、RxLink / RxAllocate callback；
- RX DMA Buffer ownership 与两次 copy；
- Frame Handler 生命周期；
- 当前 polling TX 与未来 async TX 的边界。

## 4. 当前已确认 On-board 结果

当前 Reference Example 与 `As weak` Task Entry 结构下，用户已确认 Build / On-board 回归通过，async RX 1000 / 1000 正常。

该测试继续只表示基础异步机制与 ownership 可持续工作，不代表高负载 Stress Test。

## 5. M2 尚未完成

- [ ] Async TX completion ownership；
- [ ] RX/TX error / drop 统计；
- [ ] DMA fatal / RBU / timeout recovery；
- [ ] Link Down / Up 完整 MAC lifecycle；
- [ ] Task stack high-water mark；
- [ ] D-Cache 场景；
- [ ] 长时间 / 高负载。

## 6. 下一工作单元建议

推荐进入 **Async TX completion ownership**：

```text
HAL_ETH_Transmit_IT()
→ TX complete IRQ
→ task-side release / HAL_ETH_ReleaseTxPacket()
→ HAL_ETH_TxFreeCallback()
→ TX DMA Buffer recycle
```

本工作单元只处理异步 TX ownership 和完成路径；不要同时进入 LwIP、完整 Link lifecycle 或大规模 error recovery。