# Test Plan

- 状态：Active
- 说明：本文件定义各里程碑验收边界。所有结果区分 Static Review、Build Verified、On-board Verified 和 Measured。

## M0：项目基线

通过条件：

- [ ] Debug / Release fresh build 可重复记录；
- [x] FreeRTOS 最小任务正常；
- [x] USART1 调试输出正常；
- [x] CubeMX 生成代码与手工代码边界明确；
- [x] 基础上板运行。

## M1：PHY Bring-up

### 已通过

- [x] PHY Reset；
- [x] MDIO Read；
- [x] MDIO Write；
- [x] PHY ID；
- [x] PHY Address；
- [x] Strap；
- [x] Auto-negotiation；
- [x] Link Up / Down；
- [x] 100 Mbit/s；
- [x] Full Duplex；
- [x] 单次网线拔出 / 插回恢复。

上板结果：

```text
PHY ID1     = 0x0007
PHY ID2     = 0xC0F1
Reg18       = 0x60E0
PHY Address = 0
MODE        = 111
Speed       = 100M
Duplex      = Full
```

### 补充覆盖

- [ ] 10 Mbit/s 实际链路；
- [ ] Half Duplex；
- [ ] 连续多次插拔；
- [ ] 多次 MCU Reset；
- [ ] 多次 PHY Reset；
- [ ] 25 MHz PHY 晶振 Measured；
- [ ] PA1 / RMII_REF_CLK Measured。

## M2：MAC / DMA

### 1. Memory / Descriptor / Buffer

- [x] Ethernet DMA 可达 SRAM选择 — Static Review；
- [x] SRAM3 独立 `RAM_ETH` — Build Verified；
- [x] RX Descriptor `0x30040000` — Build / Map Verified；
- [x] TX Descriptor `0x30040080` — Build / Map Verified；
- [x] Descriptor linker ASSERT — Build Verified；
- [x] MPU 配置 — Static Review + Build Verified；
- [x] SRAM3 clock 在 `MX_ETH_Init()` 前准备 — Static Review + Build Verified；
- [x] RX Pool `0x30042000 / 4 × 1536 B` — Build / Map Verified；
- [x] TX Pool `0x30044000 / 4 × 1536 B` — Build / Map Verified；
- [x] RX ownership / recycle — Static Review + On-board Verified；
- [x] TX polling success-path ownership — Static Review + On-board Verified；
- [ ] TX error / timeout 完整 recovery；
- [ ] D-Cache 开启后的数据路径。

历史 map：

```text
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
.eth_dma_rx  = 0x30042000 / 0x1800
.eth_dma_tx  = 0x30044000 / 0x1800
```

### 2. Polling Raw Frame

- [x] MAC Speed / Duplex 与 PHY 同步；
- [x] Raw TX；
- [x] Raw RX 单帧；
- [x] RX Buffer 连续 recycle。

Polling 测试固件：

```text
e50bf6a4ce9c3763e6b863b5982522b4e60ac197
```

TX：

```text
Source      = 00:80:E1:00:00:00
Destination = FF:FF:FF:FF:FF:FF
EtherType   = 0x88B5
Length      = 60 B
```

PC `tcpdump` 实际抓包成功。

RX：

```text
PC → STM32
EtherType = 0x88B5
Length    = 60 B
```

结果：

```text
Single RX      PASS
Continuous RX  1000 / 1000 PASS
PC interval    ≈ 5 ms / Frame
```

该结果只验证基础 Descriptor / Buffer recycle，不是吞吐压力测试。

### 3. ETH IRQ + FreeRTOS Async RX

- [x] ETH_IRQn 配置；
- [x] IRQ priority 满足当前 FreeRTOS FromISR 约束；
- [x] `ETH_IRQHandler()` → `HAL_ETH_IRQHandler()`；
- [x] `HAL_ETH_Start_IT()`；
- [x] RX complete callback；
- [x] CMSIS-RTOS2 Thread Flag；
- [x] RX Task 在任务上下文读取 Frame；
- [x] 一次唤醒持续 drain 到 `ETHERNET_RX_NONE`；
- [x] 连续 1000 / 1000 async RX。

异步 RX 测试固件：

```text
6b2f1f4bd153e6e4d119be6679a8dea55e7d4ccd
```

上板实际结果：

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

验证等级：On-board Verified。

该测试证明当前 IRQ → Thread Flag → RX Task → Receive → Buffer recycle 可持续工作 1000 帧，不代表高负载或长时间稳定性。

### 4. Driver Package 化回归

当前 Package 重构把通用代码收敛到：

```text
Ethernet/
```

并新增 Port + CMSIS-RTOS2 Adapter。

重构后必须重新执行：

- [ ] `./build.sh Debug --fresh`；
- [ ] `./build.sh Release --fresh`；
- [ ] map / ELF Descriptor 地址；
- [ ] map / ELF RX/TX Pool 地址与大小；
- [ ] PHY / MAC 启动回归；
- [ ] Raw / Async RX 1000 / 1000；
- [ ] 确认无新的 HardFault / DMA Error；
- [ ] CubeMX Generate Code 后检查 USER CODE / Port 边界。

在这些测试完成前，Package 化提交只能标记 Static Review。

### 5. M2 尚未完成

- [ ] Async TX completion；
- [ ] RX/TX error / drop 统计；
- [ ] DMA fatal / RBU / timeout recovery；
- [ ] Link Down / Up 完整 MAC lifecycle；
- [ ] 长时间 DMA；
- [ ] 高负载无内存破坏；
- [ ] Task stack high-water mark；
- [ ] D-Cache 场景。

M2 完整退出前至少需要 Async RX、Async TX 基础 ownership、关键错误可观测和异步 Raw Frame 数据路径稳定。

## M3：LwIP + Ping

- [ ] `ethernetif`；
- [ ] Static IPv4；
- [ ] ARP；
- [ ] Ping；
- [ ] 不同 Ping payload；
- [ ] 持续 Ping；
- [ ] Link / MCU / PC 重启恢复。

退出条件：可长期稳定 Ping，无 HardFault、DMA Error、内存泄漏或明显 pbuf 异常。

## M4：UDP

- [ ] UDP Echo；
- [ ] 小包 / 接近 MTU 大包 / 随机长度；
- [ ] 高频 RX / TX；
- [ ] 双向通信；
- [ ] Link 恢复；
- [ ] 长时间运行；
- [ ] Drop / Error 统计。

## M5：TCP

- [ ] Connect；
- [ ] Send / Receive；
- [ ] Disconnect / Reconnect；
- [ ] Client crash；
- [ ] Cable disconnect；
- [ ] MCU reset；
- [ ] 长时间连接。

## M6：压力与通用化

- [ ] 数小时持续 UDP；
- [ ] 大量小包；
- [ ] 大包；
- [ ] 双向高负载；
- [ ] 快速 Link Up / Down；
- [ ] FreeRTOS stack high-water mark；
- [ ] LwIP memory pool；
- [ ] CPU load；
- [ ] RX/TX drop；
- [ ] DMA error；
- [ ] 更换 STM32H7 板时以复制 `Ethernet/` + 修改 CubeMX / linker / Port 为主。

## 测试记录要求

每个测试最终记录：

- 前置条件；
- 固件 commit；
- Cube / HAL 版本；
- PC 工具；
- 操作步骤；
- 预期 / 实际结果；
- 错误计数；
- 是否通过；
- 验证等级；
- 必要的仪器测量。
