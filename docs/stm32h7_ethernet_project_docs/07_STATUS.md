# Project Status

- 更新时间：2026-08-22
- 当前阶段：M2 — MAC / DMA + Driver Package 产品化整理
- 当前状态：第一轮 `Ethernet/` Package 化已完成 Debug Build 和 Async RX 1000 / 1000 上板回归；第二阶段已把完整 STM32H743 Reference Example 移入 `examples/`，该路径重构当前仅 Static Review，等待本地 fresh Build / map / On-board 回归。

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
- [x] linker ASSERT / map 历史验证；
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

### Driver Package 第一轮

```text
Ethernet/
├── Inc/
├── Src/
├── PHY/LAN8720/
├── Port/Inc/
└── RTOS/CMSIS_RTOS2/
```

- [x] Driver Core 不再直接依赖 Demo `eth.h` / `heth`；
- [x] Port API 建立；
- [x] CMSIS-RTOS2 Adapter 建立；
- [x] Adapter 不创建 Task；
- [x] Frame Handler 保持任务上下文；
- [x] `0x88B5` 测试逻辑留在 Demo；
- [x] Debug Build Verified；
- [x] On-board Verified：Package 化后 Async RX 1000 / 1000 PASS。

## 2. 第二阶段仓库结构整理

当前提交已静态完成：

```text
Ethernet/                                  ← Driver Package
examples/STM32H743_LAN8720_FreeRTOS/       ← Reference Example
README.md                                  ← Integration Guide
docs/stm32h7_ethernet_project_docs/        ← 项目/专题文档
```

具体改动：

- [x] `Core/`、ST `Drivers/`、`Middlewares/`、BSP、`.ioc`、linker、CMake、startup、build/flash 脚本整体移入 Example；
- [x] Example `CMakeLists.txt` 通过 `../../Ethernet` 引用根 Driver Package；
- [x] `cmake/stm32cubemx/CMakeLists.txt` 原样移动，没有手工修改；
- [x] 根 README 扩充为独立 Driver Integration Guide；
- [x] 新增 Example README；
- [x] `docs/BOARD_PORTING.md` 内容职责并入根 README并删除；
- [x] `.gitignore` 支持 nested example build 目录；
- [x] D022 记录 Product / Example 目录边界；
- [x] D023 Proposed：CubeMX RX Task 倾向 `As weak`。

验证等级：**Static Review only**。尚未把这一结构移动提交写成 Build / On-board Verified。

## 3. 已确认 On-board 结果

第一轮 Package 化后的实际输出：

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

该结果证明第一轮 Package 结构下 IRQ → Driver event → Thread Flag → RX Task → Receive → Buffer recycle 可持续工作 1000 帧；不是高负载 Stress Test。

## 4. 当前接口

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

## 5. 当前待验证

针对第二阶段目录移动：

- [ ] `cd examples/STM32H743_LAN8720_FreeRTOS && ./build.sh Debug --fresh`；
- [ ] `./build.sh Release --fresh`；
- [ ] 当前路径下重新检查 map / ELF；
- [ ] 烧录后 PHY / MAC startup；
- [ ] async RX 1000 / 1000；
- [ ] CubeMX Generate Code 后检查路径与 USER CODE；
- [ ] 在当前 Example 上实际切换 `EthernetRxTask` 为 `EthernetRtos_RxTask + As weak`，Generate Code / Build / On-board 后决定 D023 是否 Accepted。

## 6. M2 尚未完成

- [ ] Async TX completion ownership；
- [ ] RX/TX error / drop 统计；
- [ ] DMA fatal / RBU / timeout recovery；
- [ ] Link Down / Up 完整 MAC lifecycle；
- [ ] Task stack high-water mark；
- [ ] D-Cache 场景；
- [ ] 长时间 / 高负载。

## 7. 下一工作单元建议

先完成本次结构移动的本地 Build / map / On-board 回归和 CubeMX `As weak` 实测，不进入 Async TX。结构回归通过后再决定是收尾 M2 Runtime 还是进入异步 TX completion。
