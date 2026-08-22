# Project Status

- 更新时间：2026-08-22
- 当前阶段：M2 — STM32H743 MAC / DMA + Driver Package 化
- 当前状态：`Ethernet/` Driver Package 第一轮重构已完成 Debug 构建与上板回归；PHY / MAC 正常启动，CMSIS-RTOS2 异步 RX 再次完成 1000 / 1000 帧验证。

## 1. 已完成

### 工程与硬件基线

- [x] STM32H743VIT6 + LAN8720AI + RMII；
- [x] STM32 HAL + FreeRTOS / CMSIS-RTOS2；
- [x] CubeMX 6.18.1 / STM32CubeH7 V1.13.0 / HAL ETH 1.11.6；
- [x] PHY Reset、MDIO、PHY ID、Address、Strap；
- [x] Auto-negotiation、Link、100M Full Duplex；
- [x] USART1 基础调试输出。

### DMA / Memory

- [x] SRAM3 `0x30040000 ~ 0x30047FFF` 作为 `RAM_ETH`；
- [x] RX Descriptor `0x30040000`；
- [x] TX Descriptor `0x30040080`；
- [x] RX Pool `0x30042000 / 4 × 1536 B`；
- [x] TX Pool `0x30044000 / 4 × 1536 B`；
- [x] MPU Non-cacheable / Descriptor overlay；
- [x] linker section / ASSERT / map 历史验证；
- [x] copy-based RX Buffer recycle；
- [x] polling TX success-path ownership。

### MAC / DMA 数据路径

- [x] PHY Speed / Duplex → MAC 配置；
- [x] `HAL_ETH_Start_IT()`；
- [x] polling TX；
- [x] polling RX；
- [x] ETH IRQ；
- [x] `HAL_ETH_RxCpltCallback()`；
- [x] CMSIS-RTOS2 Thread Flag notification；
- [x] RX Task 中 drain `EthernetDriver_Receive()` 直到 `ETHERNET_RX_NONE`；
- [x] 异步 RX 连续 1000 / 1000 帧上板验证。

### Driver Package 化

当前结构：

```text
Ethernet/
├── Inc/
├── Src/
├── PHY/LAN8720/
├── Port/Inc/
└── RTOS/CMSIS_RTOS2/
```

已完成并回归验证：

- [x] 通用 Driver 从 `Drivers/Ethernet/**` 收敛到根目录 `Ethernet/**`；
- [x] 新增 `ethernet_port.h`，通用 Driver 不再直接 include Demo `eth.h`；
- [x] `EthernetPort_GetHandle()` 解除对全局变量名 `heth` 的直接依赖；
- [x] 当前 STM32H743 板级 Port 实现放在 `BSP/stm32h743vit6_iot/ethernet_port.c`；
- [x] 新增 CMSIS-RTOS2 Adapter；
- [x] RTOS Adapter 不创建 Task，只提供 `EthernetRtos_RxTask()`；
- [x] RX Frame 通过任务上下文同步 Handler 向上交付；
- [x] `0x88B5` 测试逻辑继续留在 Demo `freertos.c`，不进入 Driver Package；
- [x] 根 README 已改为 Driver 集成指南；
- [x] 修正 `ethernet_port.h` 直接 include `stm32h7xx_hal_eth.h` 导致的 HAL 递归包含问题，改为 `stm32h7xx_hal.h`；
- [x] Package 化代码 Debug Build Verified；
- [x] Package 化代码 On-board Verified：PHY / MAC 正常启动；
- [x] Package 化代码 On-board Verified：Async RX 1000 / 1000 PASS。

仍待补充：Release fresh build、当前 Package 提交下的 map / ELF 再检查、CubeMX Generate Code 后边界回归。

## 2. 已确认验证结果

### PHY On-board Verified

```text
PHY ID1     = 0x0007
PHY ID2     = 0xC0F1
PHY Address = 0
MODE        = 111
Link        = Up / Down 可检测
Speed       = 100M
Duplex      = Full
```

### DMA Build / Map Verified

历史已验证布局：

```text
RAM_ETH      = 0x30040000 / 32 KiB
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
RX Pool      = 0x30042000 / 0x1800
TX Pool      = 0x30044000 / 0x1800
```

Package 化没有修改 linker 或 DMA section 定义，但当前重构后的 map / ELF 再检查仍建议补做。

### Raw Frame On-board Verified

Polling 基线测试固件：`e50bf6a4ce9c3763e6b863b5982522b4e60ac197`。

- 裸 TX：PC `tcpdump` 抓到 60 B / EtherType `0x88B5`；
- polling RX：单帧 PASS；
- polling RX：连续 1000 / 1000 PASS，约 5 ms / Frame。

### Async RX On-board Verified

异步 RX 原始测试固件：`6b2f1f4bd153e6e4d119be6679a8dea55e7d4ccd`。

Package 化后再次得到：

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

该结果确认 Package 化后的路径仍能持续工作：

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

连续 1000 帧仅验证异步机制和基础 ownership，不代表高负载压力测试。

## 3. 当前 Driver Package 接口

### Port

```text
EthernetPort_GetHandle()
EthernetPort_PrepareDmaMemory()
EthernetPort_PhyResetAssert()
EthernetPort_PhyResetRelease()
```

### Driver / MDIO / PHY

```text
EthernetDriver_Init()
EthernetDriver_SetRxEventHandler()
EthernetDriver_ConfigureLink()
EthernetDriver_Start()
EthernetDriver_Transmit()
EthernetDriver_Receive()

EthernetMdio_Read()
EthernetMdio_Write()

Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

### CMSIS-RTOS2 Adapter

```text
EthernetRtos_SetRxFrameHandler()
EthernetRtos_IsReady()
EthernetRtos_RxTask()
```

RTOS Adapter 不创建 Task；Task 的 priority、stack 和 allocation 由应用 / CubeMX 管理。

## 4. 当前未完成

### Package 化补充验证

- [ ] fresh Release build；
- [ ] 当前重构后的 map / ELF 地址再检查；
- [ ] CubeMX Generate Code 后检查 USER CODE / Port / RTOS Adapter 边界；
- [ ] 实测 CubeMX `As external` / `As weak` Task code generation。

### M2 后续

- [ ] 异步 TX completion ownership；
- [ ] RX/TX error / drop 统计；
- [ ] DMA error / timeout recovery；
- [ ] Link Down / Up 完整 MAC lifecycle；
- [ ] 长时间 / 高负载稳定性；
- [ ] D-Cache 开启后的专项验证。

### 仓库结构 / 文档后续

- [ ] 把完整 STM32H743 Demo 收敛到 `examples/`；
- [ ] 整理 `docs/` 用户技术文档与项目控制文档的目录；
- [ ] 清理旧路径和重复迁移文档。

### M3 ～ M6

LwIP / Ping / UDP / TCP / Stress 尚未进入。

## 5. 当前尚未冻结

- CubeMX Task 默认推荐 `As external` 还是 `As weak`；
- ETH IRQ 最终优先级；
- RX Task 最终 priority / stack；
- 异步 TX completion ownership；
- DMA error recovery；
- Link change 完整 lifecycle；
- Cache 开启后的最终策略；
- LwIP Runtime 参数和 API。

## 6. 下一工作单元

Package 化第一轮功能回归已经通过。下一工作单元优先继续仓库产品化整理，而不是立即进入异步 TX：

```text
参考 Demo → examples/
技术文档 / 项目控制文档重新分组
CubeMX Task external/weak 方式实测并冻结
```
