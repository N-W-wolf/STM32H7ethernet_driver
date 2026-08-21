# STM32H7 Ethernet 通用驱动项目

- 状态：Active / M2 进行中
- 第一验证平台：STM32H743VIT6
- 第一验证 PHY：LAN8720AI
- 接口：RMII
- 软件环境：STM32 HAL + FreeRTOS + LwIP

## 1. 项目目标

开发一套面向 STM32H7 的可复用 Ethernet 基础组件。

第一验证平台使用 STM32H743VIT6 + LAN8720AI，目标覆盖：

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

机器人 HostLink、CommandFrame、StateFrame、控制业务和安全状态机不属于 Ethernet 基础组件。

## 2. 稳定软件分层

```text
Application
    ↓
LwIP
    ↓
ethernetif
    ↓
Ethernet Driver
    ├── STM32H7 MAC / DMA
    └── PHY abstraction
             ↓
          PHY Driver
    ↓
BSP / Board Port
    ↓
STM32 HAL / Hardware
```

约束：

- Application 不直接操作 `HAL_ETH_xxx()`；
- Ethernet Driver 不处理 UDP/TCP/IP 或机器人业务；
- PHY Driver 不依赖 LwIP；
- BSP 只承担板级差异；
- DMA / MPU / linker 必须显式设计；
- 不为尚未出现的需求建立复杂抽象。

详细架构以 `01_ARCHITECTURE.md` 为准。

## 3. 开发里程碑

### M0：项目基线

核心内容已完成：

- 工程目录与文档；
- CubeMX / CMake 基础工程；
- FreeRTOS / CMSIS-RTOS v2；
- TIM6 HAL Tick；
- BootstrapTask；
- USART1 调试输出；
- 基础上板运行。

Debug / Release fresh build 的正式重复记录仍可补充。

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

已完成：

- STM32H743 Ethernet DMA 可达内存核对；
- SRAM3 独立为 `RAM_ETH`；
- RX/TX Descriptor 固定地址；
- Descriptor linker ASSERT；
- MPU Normal Non-cacheable + Device overlay；
- SRAM3 时钟准备；
- RX/TX 静态 DMA Buffer Pool；
- Buffer linker section / 地址验证；
- HAL RX Allocate / Link callback ownership；
- polling TX Buffer ownership；
- PHY Speed / Duplex 与 MAC 同步；
- `HAL_ETH_Start()` 基础生命周期；
- 裸 Ethernet Frame TX 上板验证；
- 裸 Ethernet Frame RX 单帧及连续 1000 帧上板验证。

未完成：

- ETH IRQ；
- FreeRTOS 异步 RX/TX；
- TX 异步 completion ownership；
- DMA / MAC 错误统计与恢复；
- Link Down / Up 时完整 MAC Stop / Reconfigure / Start 生命周期；
- 长时间和高负载压力测试；
- D-Cache 开启后的专项验证。

### M3：LwIP + IP

未开始：

- `ethernetif`；
- LwIP；
- Static IPv4；
- ARP；
- ICMP Ping。

### M4：UDP

未开始。

### M5：TCP

未开始。

### M6：通用化与验收

未开始。

## 4. 多对话协作方式

聊天历史不作为项目当前状态源。

项目真实状态由远程仓库当前 `main` 分支中的以下内容共同构成：

- 代码；
- `.ioc`；
- CMake；
- linker；
- README；
- `DECISIONS.md`；
- `STATUS.md`；
- `HANDOFF.md`；
- 对应专题文档。

每个新对话只承担一个明确工作单元。

默认读取顺序：

1. `00_PROJECT.md`；
2. `01_ARCHITECTURE.md`；
3. `02_HARDWARE_BASELINE.md`；
4. `06_DECISIONS.md`；
5. `07_STATUS.md`；
6. `08_HANDOFF.md`；
7. 当前任务实际需要的源码 / `.ioc` / linker / HAL / Datasheet / Reference Manual。

## 5. 资料优先级

### 硬件连接

```text
实际 PCB / 当前有效原理图
>
器件 Datasheet
>
Hardware Baseline
>
聊天历史与推断
```

### PHY 行为

```text
LAN8720A/LAN8720Ai Datasheet
>
实测
>
官方参考实现
>
网络资料
```

### STM32 外设行为

```text
对应 Reference Manual / Datasheet
>
当前仓库 HAL 源码
>
STM32CubeH7 官方示例
>
其他资料
```

### 项目架构

```text
Accepted DECISIONS.md
>
01_ARCHITECTURE.md
>
专题技术文档
>
当前讨论中的临时方案
```

发生冲突时必须显式记录，不允许把推断写成已确认事实。

## 6. 文档分类与写作规则

项目文档分为两类。

### 6.1 面向使用者 / 技术阅读者

包括：

```text
README.md
01_ARCHITECTURE.md
02_HARDWARE_BASELINE.md
03_MEMORY_DMA.md
04_RTOS_NETWORK.md
docs/BOARD_PORTING.md
```

规则：

- 不展示 M0/M1/M2 等内部开发阶段；
- 不使用“当前阶段”“下一阶段”“工作单元”等项目推进语义；
- 只描述项目介绍、支持状态、硬件、架构、环境依赖、使用方法、限制、技术设计和迁移方法；
- 未实现的能力直接标记“未实现”；
- 区分 Static Review、Build Verified、On-board Verified 和 Measured；
- 不把规划写成已经实现的功能；
- README 永远保持产品 / 库视角，不作为开发日志。

### 6.2 项目控制 / 规划文档

包括：

```text
00_PROJECT.md
05_TEST_PLAN.md
06_DECISIONS.md
07_STATUS.md
08_HANDOFF.md
```

这些文档可以包含：

- M0/M1/M2 等里程碑；
- 当前阶段；
- 下一工作单元；
- Accepted / Proposed / Superseded；
- 未完成项；
- 测试计划；
- 交接信息。

## 7. 文档职责

- `README.md`：项目介绍、支持状态、环境、构建、烧录、入口文档；
- `01_ARCHITECTURE.md`：稳定软件分层和职责边界；
- `02_HARDWARE_BASELINE.md`：当前验证板硬件事实与验证状态；
- `03_MEMORY_DMA.md`：DMA / MPU / Cache / linker 当前有效设计；
- `04_RTOS_NETWORK.md`：FreeRTOS / LwIP 运行边界与支持状态；
- `docs/BOARD_PORTING.md`：板级迁移指南；
- `05_TEST_PLAN.md`：里程碑测试与验收；
- `06_DECISIONS.md`：跨模块设计决定；
- `07_STATUS.md`：当前事实和进度；
- `08_HANDOFF.md`：最近工作单元交接。

原 `docs/STM32H7 Ethernet 通用驱动开发指导与规划.md` 与旧 `01_ARCHITECTURE.md` 内容重复，不再保留双份架构文档；`01_ARCHITECTURE.md` 是唯一架构技术文档。

## 8. 板级配置与自动化原则

板级硬件配置保持显式、可审查：

- `.ioc` 保存 CubeMX 外设和 MPU 配置；
- BSP 保存板级 Reset / 时钟准备等逻辑；
- linker 明确表达 DMA SRAM 和 section；
- `.map` / ELF 用于验证实际地址。

不使用正则或字符串 patch 脚本自动修改 linker。

自动化优先用于：

- 构建；
- map / ELF 验证；
- alignment / 地址范围检查；
- CI 阻止错误内存布局。

只有出现真实多板维护需求时，才评估结构化 Board Config + linker template 生成。

## 9. GitHub 写入规则

默认 GitHub 只读。

只有用户在当前请求中明确授权，才能修改远程文件、提交或更新分支。授权只覆盖当前明确工作范围，不自动延续到之后的工作单元。
