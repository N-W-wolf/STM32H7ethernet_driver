# Latest Handoff

- 来源工作单元：M2 DMA 内存基础 + 文档约定整理
- 日期：2026-08-21
- 当前阶段：M2 MAC / DMA

## 1. 本次完成内容

### Ethernet DMA 内存基础

已完成：

- STM32H743 Ethernet DMA 可访问 SRAM 核对；
- SRAM3 独立为 `RAM_ETH`；
- RX / TX Descriptor 固定地址；
- Descriptor linker ASSERT；
- Cortex-M7 MPU 配置；
- SRAM3 时钟准备；
- Descriptor Build / map 地址验证。

当前地址：

```text
RAM_D2       = 0x30000000 / 256 KiB
RAM_ETH      = 0x30040000 / 32 KiB
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
RX section   = 96 B
TX section   = 96 B
```

MPU：

```text
Region 1
0x30040000 / 32 KiB
Normal, Non-cacheable, Non-bufferable, Shareable, XN

Region 2
0x30040000 / 256 B
Device, Non-cacheable, Bufferable, Non-shareable, XN
```

当前 CPU I-Cache / D-Cache 均为 Disabled。

### 板级 DMA SRAM 准备

新增接口：

```text
BoardEthernet_PrepareDmaMemory()
```

位置：

```text
BSP/stm32h743vit6_iot/board_ethernet.c/.h
```

当前实现使能：

```c
__HAL_RCC_D2SRAM3_CLK_ENABLE();
```

调用位置：

```text
Core/Src/main.c
USER CODE BEGIN SysInit
```

调用发生在 `MX_ETH_Init()` 之前。

### 文档与板级迁移约定

已确定：

- README 和技术参考文档采用产品 / 使用者视角；
- 公开技术文档不展示内部 M0/M1/M2 等开发阶段；
- `00_PROJECT.md`、`05_TEST_PLAN.md`、`06_DECISIONS.md`、`07_STATUS.md`、`08_HANDOFF.md` 继续承担项目控制和规划；
- `01_ARCHITECTURE.md` 成为唯一架构技术文档；
- 删除与其重复的《STM32H7 Ethernet 通用驱动开发指导与规划》；
- `03_MEMORY_DMA.md` 改为记录当前有效 DMA / MPU / linker 方案；
- 新增 `docs/BOARD_PORTING.md`；
- 板级 linker 采用显式配置，不使用 regex / 字符串 patch 脚本自动修改；
- 自动化优先用于 map / ELF / alignment / 越界验证。

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

## 3. 当前文件所有权

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

- [x] STM32H743 Ethernet DMA 不使用 DTCM；
- [x] SRAM3 可作为 Ethernet DMA 内存；
- [x] Descriptor / Buffer 需要显式 linker 管理；
- [x] MPU 两层覆盖属性检查；
- [x] SRAM3 clock 在 Ethernet 初始化前使能；
- [x] BSP / linker / `.ioc` 的职责边界明确。

### Build / Map Verified

- [x] `RAM_D2 = 256 KiB`；
- [x] `RAM_ETH = 32 KiB`；
- [x] `DMARxDscrTab = 0x30040000`；
- [x] `DMATxDscrTab = 0x30040080`；
- [x] RX / TX Descriptor section = 96 B；
- [x] linker 地址 / 大小 / 非空 ASSERT 通过。

### On-board Verified

本工作单元没有完成 Ethernet DMA Frame 数据路径上板验证。

PHY 既有上板结果仍有效：

```text
PHY ID1     = 0x0007
PHY ID2     = 0xC0F1
PHY Address = 0
MODE        = 111
Link        = Up / Down
Speed       = 100M
Duplex      = Full
```

### Measured

没有新增示波器 / 逻辑分析仪测量结果。

## 5. 当前尚未解决

M2 关键未完成项：

- RX Buffer Pool；
- TX Buffer Pool；
- HAL RX Allocate / Link callback ownership；
- TX completion ownership；
- Buffer linker section / 地址；
- MAC Speed / Duplex 与 PHY 状态同步；
- 裸 Ethernet Frame TX；
- 裸 Ethernet Frame RX；
- ETH IRQ；
- FreeRTOS 异步收发；
- DMA 错误统计；
- DMA Frame 数据路径上板验证；
- 长时间 / 高负载稳定性。

RX/TX Buffer 数量和最终地址当前不得自行假定。

## 6. 新的 Accepted 决策

`06_DECISIONS.md` 新增 / 更新：

- D007：原 DMA / Cache 倾向方案标记为 Superseded；
- D015：文档受众与阶段信息边界；
- D016：STM32H743 Ethernet DMA SRAM3 / Descriptor / MPU 基础方案；
- D017：板级 linker 与自动化策略；
- D018：CubeMX Memory Management Tool 边界。

## 7. 下一工作单元开始时优先读取

1. `00_PROJECT.md`；
2. `01_ARCHITECTURE.md`；
3. `02_HARDWARE_BASELINE.md`；
4. `03_MEMORY_DMA.md`；
5. `05_TEST_PLAN.md`；
6. `06_DECISIONS.md`；
7. `07_STATUS.md`；
8. 本文件；
9. `Core/Src/eth.c` / `Core/Inc/eth.h`；
10. 当前 HAL 1.11.6 Ethernet 源码；
11. `STM32H743xx_FLASH.ld`；
12. `stm32H7ethernet_demo.ioc`。

## 8. 下一工作单元推荐边界

继续 M2：RX/TX Buffer Pool 与 HAL ownership。

优先：

1. 精确核对 `HAL_ETH_Start()`、`HAL_ETH_ReadData()`、`ETH_UpdateDescriptor()`、RX Allocate / Link callback；
2. 确定 RX Descriptor 数量与静态 RX Buffer 数量关系；
3. 确定 TX Buffer ownership；
4. 定义 Buffer section；
5. 放入 `RAM_ETH` 并 Build / map 验证；
6. 再建立最小裸 Ethernet Frame TX / RX。

范围外：

- LwIP；
- Ping；
- UDP / TCP；
- 机器人 HostLink / 业务协议。
