# STM32H7 Ethernet 通用驱动项目

- 状态：Active / M2 进行中
- 仓库定位：STM32H7 Ethernet Driver Package + 参考 Demo
- 第一验证平台：STM32H743VIT6
- 第一验证 PHY：LAN8720AI
- 接口：RMII
- 软件环境：STM32 HAL + FreeRTOS + LwIP

## 1. 项目目标

开发一套面向 STM32H7 的可复用 Ethernet 基础组件。

仓库的核心产品是：

```text
Ethernet/
```

目标用户应能够把该目录复制到自己的 STM32H7 工程，再根据目标 MCU / PCB 完成 CubeMX、linker、MPU 和 Port 配置。当前 STM32H743VIT6 + LAN8720AI 工程是参考 Demo，不定义 Driver 的固定构建工具链或板级地址。

第一版目标覆盖：

- PHY Reset、MDIO/MDC、Link / Speed / Duplex；
- STM32H7 Ethernet MAC / DMA；
- Descriptor / Buffer ownership；
- MPU / Cache / linker；
- FreeRTOS 异步 RX/TX；
- LwIP `ethernetif`；
- Static IPv4；
- ICMP Ping；
- UDP Echo；
- TCP Echo；
- 错误统计、链路恢复和压力测试；
- 板级差异、PHY 差异与通用驱动逻辑分离。

机器人 HostLink、CommandFrame、StateFrame、控制业务和安全状态机不属于 Ethernet Driver。

## 2. 稳定软件分层

```text
Application / LwIP
        ↓
RTOS Adapter / ethernetif
        ↓
Ethernet Driver
    ├── STM32H7 MAC / DMA
    └── MDIO
        ↓
PHY Driver
        ↓
Ethernet Port
        ↓
STM32 HAL / Board
```

约束：

- Application 不直接操作 `HAL_ETH_xxx()`；
- Ethernet Driver 不处理 UDP/TCP/IP 或机器人业务；
- PHY Driver 不依赖 LwIP / FreeRTOS；
- Driver Core 不直接依赖某个 Demo 的 `eth.h` / `heth`；
- Port 只承担 HAL ETH Handle、PHY Reset、DMA SRAM 准备等板级绑定；
- DMA / MPU / linker 必须显式设计；
- RTOS Adapter 不隐藏 Task 创建；
- 不为尚未出现的需求建立复杂抽象。

详细架构以 `01_ARCHITECTURE.md` 为准。

## 3. 开发里程碑

### M0：项目基线

核心内容已完成：

- CubeMX / CMake 参考工程；
- FreeRTOS / CMSIS-RTOS v2；
- TIM6 HAL Tick；
- BootstrapTask；
- USART1 调试输出；
- 基础上板运行。

### M1：PHY Bring-up

核心内容已完成并上板验证：

- PHY Reset；
- MDIO Read / Write；
- PHY ID / Address / Strap；
- Auto-negotiation；
- Link Up / Down；
- 100 Mbit/s；
- Full Duplex；
- 单次网线拔插恢复。

10 Mbit/s、Half Duplex、连续插拔和时钟独立测量仍属于补充测试。

### M2：MAC / DMA

已完成并验证的基础能力：

- STM32H743 Ethernet DMA 可达内存核对；
- SRAM3 独立为 `RAM_ETH`；
- RX/TX Descriptor 固定地址与 linker ASSERT；
- MPU Normal Non-cacheable + Device overlay；
- RX/TX 静态 DMA Buffer Pool；
- HAL RX Allocate / Link callback ownership；
- polling TX；
- polling RX；
- 裸 Ethernet Frame TX / RX；
- polling RX 连续 1000 / 1000；
- ETH IRQ；
- `HAL_ETH_Start_IT()`；
- CMSIS-RTOS2 Thread Flag；
- RX Task drain；
- 异步 RX 连续 1000 / 1000。

当前正在进行 Driver Package 化，已形成 `Ethernet/`、Port 和 CMSIS-RTOS2 Adapter 的静态实现，但该重构提交仍需 fresh build / map / 再次上板验证。

M2 仍未完成：

- 异步 TX completion ownership；
- DMA / MAC 错误统计与恢复；
- Link Down / Up 时完整 MAC Stop / Reconfigure / Start 生命周期；
- 长时间和高负载压力测试；
- D-Cache 开启后的专项验证。

### M3 ～ M6

LwIP / Ping / UDP / TCP / Stress 尚未进入。

## 4. 多对话协作方式

聊天历史不作为项目当前状态源。

项目真实状态由远程仓库当前 `main` 中的代码、`.ioc`、linker、README、Decisions、Status、Handoff 和专题文档共同构成。

默认读取顺序：

1. `00_PROJECT.md`；
2. `01_ARCHITECTURE.md`；
3. `02_HARDWARE_BASELINE.md`；
4. `06_DECISIONS.md`；
5. `07_STATUS.md`；
6. `08_HANDOFF.md`；
7. 当前任务需要的源码 / `.ioc` / linker / HAL / Datasheet / Reference Manual。

## 5. 资料优先级

硬件连接：

```text
实际 PCB / 当前有效原理图
>
器件 Datasheet
>
Hardware Baseline
>
聊天历史与推断
```

PHY 行为：

```text
LAN8720A/LAN8720Ai Datasheet
>
实测
>
官方参考实现
>
网络资料
```

STM32 外设行为：

```text
对应 Reference Manual / Datasheet
>
当前仓库 HAL 源码
>
STM32CubeH7 官方示例
>
其他资料
```

项目架构：

```text
Accepted DECISIONS.md
>
01_ARCHITECTURE.md
>
专题技术文档
>
当前讨论中的临时方案
```

## 6. 文档分类

面向 Driver 使用者 / 技术阅读者：

```text
README.md              ← 首要 Integration Guide
01_ARCHITECTURE.md     ← 深入架构
02_HARDWARE_BASELINE.md← 当前参考板硬件事实
03_MEMORY_DMA.md       ← DMA / MPU / linker 深入设计
04_RTOS_NETWORK.md     ← RTOS / LwIP 运行边界
```

`README.md` 必须能够独立覆盖一块新 STM32H7 板最关键的接入流程，不能要求用户先跳到隐藏的板级迁移子文档才能完成 CubeMX / linker / MPU 配置。

项目控制 / 规划文档：

```text
00_PROJECT.md
05_TEST_PLAN.md
06_DECISIONS.md
07_STATUS.md
08_HANDOFF.md
```

`docs/BOARD_PORTING.md` 当前仍保留作为历史/补充迁移资料；新的完整接入流程以根 `README.md` 为准，待 Package 化回归验证完成后再决定是否删除该重复文档并重整 `docs/` 目录。

## 7. CubeMX 与手工代码边界

当前参考 Demo 中，CubeMX / ST 管理：

```text
stm32H7ethernet_demo.ioc
Core/**
Drivers/CMSIS/**
Drivers/STM32H7xx_HAL_Driver/**
cmake/stm32cubemx/CMakeLists.txt
CubeMX Third_Party middleware
```

`Core/**` 手工逻辑只写入 USER CODE 区域。

长期通用 Driver 源码位于：

```text
Ethernet/**
```

目标板实现通过 `ethernet_port.c` 与 Driver 绑定。当前 STM32H743 参考实现暂时位于 `BSP/stm32h743vit6_iot/`。

当前不使用 CubeMX Memory Management Tool 自动管理 Ethernet DMA linker section。linker 保持显式、可审查，自动化优先用于 map / ELF 验证。

## 8. GitHub 写入规则

默认 GitHub 只读。

只有用户在当前请求中明确授权，才能修改远程文件、提交或更新分支。授权只覆盖当前明确工作范围，不自动延续到之后的工作单元。
