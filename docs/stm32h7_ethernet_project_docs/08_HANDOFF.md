# Latest Handoff

- 来源工作单元：M1 PHY Bring-up
- 日期：2026-08-20
- 当前阶段：M1 核心功能完成，下一工作单元为 M2 MAC / DMA

## 1. 本次完成内容

M1 已完成 LAN8720AI PHY 基础 Bring-up：

- PHY Reset；
- HAL ETH MDIO/MDC Management 访问；
- MDIO Read / Write；
- PHY ID；
- PHY Address；
- Strap MODE；
- Auto-negotiation；
- Link；
- Speed；
- Duplex；
- Link Up / Down 动态检测。

实测：

```text
PHY ID1     = 0x0007
PHY ID2     = 0xC0F1
Reg18       = 0x60E0
PHY Address = 0
MODE        = 111
Link        = Up / Down
Speed       = 100M
Duplex      = Full
```

网线拔出后可检测 Link Down；重新插入后可恢复 Link Up。

## 2. 本次修改 / 新增代码

M1 相关手工维护文件：

```text
BSP/stm32h743vit6_iot/board_ethernet.c
BSP/stm32h743vit6_iot/board_ethernet.h
Drivers/Ethernet/Inc/ethernet_mdio.h
Drivers/Ethernet/Src/ethernet_mdio.c
Drivers/Ethernet/Inc/lan8720.h
Drivers/Ethernet/Src/lan8720.c
Core/Src/freertos.c                 # 仅 USER CODE 区域加入 Bring-up / Link polling
CMakeLists.txt                      # 引入 BSP / Ethernet 驱动源文件
```

CubeMX 生成的 ETH 基础初始化仍由 `.ioc` 与 `Core/Src/eth.c` 等生成文件管理。

## 3. 当前接口与职责边界

### Board Ethernet BSP

```text
BoardEthernet_PhyResetAssert()
BoardEthernet_PhyResetRelease()
```

位置：

```text
BSP/stm32h743vit6_iot/board_ethernet.c/.h
```

职责仅限当前板级 PHY Reset GPIO 控制。

### MDIO Wrapper

```text
EthernetMdio_Read()
EthernetMdio_Write()
```

位置：

```text
Drivers/Ethernet/Inc/ethernet_mdio.h
Drivers/Ethernet/Src/ethernet_mdio.c
```

职责：封装当前 HAL 1.11.6 的 PHY Management Read / Write，并做基本地址检查。

### LAN8720 PHY Driver

```text
Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

位置：

```text
Drivers/Ethernet/Inc/lan8720.h
Drivers/Ethernet/Src/lan8720.c
```

当前 PHY Driver：

- 只通过 `ethernet_mdio` 访问 PHY；
- 不直接依赖 FreeRTOS；
- 不依赖 LwIP；
- `Lan8720_GetStatus()` 返回 Link、Auto-negotiation、Speed、Duplex；
- BMSR Link Status 按 latch-low 规则连续读取两次；
- PHY Link Down 后不再要求读取 Reg31 才能返回 Link 状态；
- 对 `0xFFFF` 无响应值进行过滤，避免误判 Link 状态。

### BootstrapTask

位置：

```text
Core/Src/freertos.c
StartBootstrapTask()
```

当前仅作为 Bring-up 验证载体，负责：

```text
等待上电稳定
→ 释放 PHY Reset
→ PHY ID polling + timeout
→ Restart Auto-negotiation
→ 等待 Link / AN 完成
→ 打印 Speed / Duplex
→ 周期轮询 Link 状态
```

当前 Link polling 周期 200 ms 只作为 M1 验证参数，不代表未来网络任务最终周期或架构。

## 4. 已执行测试与结果

### Static Review

- [x] BSP / MDIO / PHY 分层符合当前项目架构；
- [x] PHY Driver 不依赖 FreeRTOS / LwIP；
- [x] 当前 HAL ETH PHY Management API 已按仓库 HAL 1.11.6 核对；
- [x] BMSR latch-low 行为按 Datasheet 处理；
- [x] Reg31 HCDSPEED 映射按 LAN8720 Datasheet 处理。

### Build Verified

- [x] 当前 M1 固件已完成编译、烧录并上板运行。

未单独记录 Debug / Release 两套 `--fresh` 可重复构建，因此 `05_TEST_PLAN.md` 中 M0 的“工程可重复编译”仍保留待补。

### On-board Verified

- [x] PHY Reset；
- [x] MDIO Read；
- [x] MDIO Write；
- [x] PHY ID；
- [x] PHY Address = 0；
- [x] MODE = 111；
- [x] Auto-negotiation；
- [x] Link Up；
- [x] Link Down；
- [x] 100 Mbit/s；
- [x] Full Duplex；
- [x] 单次网线拔出 / 插回恢复。

### Measured

当前没有独立示波器 / 逻辑分析仪测量结果。

尤其不要把以下内容写成 Measured：

```text
25 MHz PHY crystal
PA1 / RMII_REF_CLK ≈ 50 MHz
```

这两项当前由原理图、Datasheet 和功能验证支持，但尚未独立测量。

## 5. M1 尚未完成的补充测试

以下不阻塞进入 M2，但仍可在后续回归测试中补充：

- [ ] 10 Mbit/s 实际链路；
- [ ] Half Duplex 实际链路；
- [ ] 连续多次插拔网线；
- [ ] 多次 STM32 重启；
- [ ] 多次 PHY Reset；
- [ ] 25 MHz 晶振独立测量；
- [ ] PA1 / RMII_REF_CLK 独立测量；
- [ ] Link / Speed LED 行为专项验证。

## 6. 本次设计决定

`06_DECISIONS.md` 已更新：

- D008：PHY Link 检测由 Proposed 转为 Accepted，第一版采用周期轮询；
- D014：PHY Driver 与 RTOS 边界，Accepted。

D008 不冻结最终 200 ms 周期，也不冻结最终 Link 管理任务位置。

## 7. 下一工作单元开始时必须读取

1. `00_PROJECT.md`
2. `01_ARCHITECTURE.md`
3. `02_HARDWARE_BASELINE.md`
4. `03_MEMORY_DMA.md`
5. `05_TEST_PLAN.md`
6. `06_DECISIONS.md`
7. `07_STATUS.md`
8. 本文件
9. 当前 `Core/Src/eth.c` / `Core/Inc/eth.h`
10. 当前 HAL 1.11.6 Ethernet 源码
11. `stm32H7ethernet_demo.ioc`
12. STM32H743 Datasheet / Reference Manual 中 Ethernet DMA、SRAM、Cache / MPU 相关章节

## 8. 下一工作单元推荐边界

下一工作单元进入 **M2：STM32H743 MAC / DMA**。

优先按以下顺序推进：

1. 核对 STM32H743 Ethernet DMA Master 可访问内存；
2. 读取当前 CubeMX 生成的 Descriptor / Buffer 配置；
3. 核对当前 HAL 1.11.6 ETH Descriptor / Buffer API；
4. 冻结第一版 Descriptor / Buffer SRAM 放置方案；
5. 明确 linker section、实际地址与 map 验证方法；
6. 确定 MPU / D-Cache 一致性方案；
7. 建立最小裸 Ethernet Frame TX；
8. 建立最小裸 Ethernet Frame RX；
9. 再处理 ETH IRQ 与 FreeRTOS 异步推进。

M2 不进入：

- LwIP；
- Ping；
- UDP / TCP；
- 机器人 HostLink / 业务协议。
