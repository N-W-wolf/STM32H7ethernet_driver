# Project Status

- 更新时间：2026-08-20
- 当前阶段：M0 — 项目基线 / 已完成
- 当前状态：M0 已完成，可进入 M1 PHY Bring-up

## 1. 已完成

- [x] 明确项目目标：STM32H7 通用 Ethernet 基础组件；
- [x] 形成《STM32H7 Ethernet 通用驱动开发指导与规划》；
- [x] 确认 FreeRTOS + LwIP 场景；
- [x] 确认第一验证 MCU：STM32H743VIT6；
- [x] 确认第一验证 PHY：LAN8720AI；
- [x] 确认 RMII 硬件连接；
- [x] 加入 STM32H743 Datasheet；
- [x] 加入 LAN8720A/LAN8720Ai Datasheet；
- [x] 加入当前有效 STM32H743VIT6 原理图；
- [x] 建立多对话协作所需项目文档骨架；
- [x] 确定代码仓库实际基础结构；
- [x] 确定 CubeMX 6.18.1 / STM32CubeH7 V1.13.0；
- [x] 确定 GNU Arm Embedded + CMake 构建工具链；
- [x] 明确当前工程以 CubeMX 基础工程为起点；
- [x] 建立最小 FreeRTOS 基础工程；
- [x] 建立 BootstrapTask，并完成 LED 心跳上板验证；
- [x] 配置 SYS Timebase = TIM6，SysTick 留给 FreeRTOS；
- [x] 配置 USART1：PA9 TX / PA10 RX，115200 8N1；
- [x] 建立 `printf -> _write() -> HAL_UART_Transmit() -> USART1` 调试输出链路；
- [x] Ubuntu 端通过 `/dev/ttyACM0` 成功接收串口日志；
- [x] 明确 CubeMX 生成代码与手工代码边界；
- [x] 建立 BSP 调试输出文件 `BSP/stm32h743vit6_iot/board_debug.c`；
- [x] 项目状态文档已纳入版本控制。

## 2. M0 验证结果

当前已实际验证：

```text
上电
  ↓
HAL 初始化
  ↓
USART1 初始化
  ↓
FreeRTOS Scheduler 启动
  ↓
BootstrapTask 周期运行
  ├── LED1 心跳正常
  └── printf 串口输出正常
```

调试串口当前使用：

```text
USART1
PA9  = TX
PA10 = RX
115200 / 8N1
```

PC/Linux 端已通过 `/dev/ttyACM0` 成功接收日志。

当前板卡实测发现 UART 相关 PCB 丝印与有效原理图标识相反；软件和后续接线判断应以实际 MCU 信号与有效原理图为准，不依赖丝印。

## 3. 当前未完成

### M1

- [ ] LAN8720AI Reset；
- [ ] MDIO Read；
- [ ] MDIO Write；
- [ ] PHY ID；
- [ ] Strap 验证；
- [ ] Auto-negotiation；
- [ ] Link；
- [ ] Speed；
- [ ] Duplex。

### M2 ～ M6

尚未开始。

## 4. 当前已确认工程基线

```text
MCU             : STM32H743VIT6
PHY             : LAN8720AI
MAC/PHY Interface: RMII
RTOS            : FreeRTOS / CMSIS-RTOS v2
Network Stack   : LwIP（尚未接入）
CubeMX          : 6.18.1
STM32CubeH7     : V1.13.0
HAL component   : 当前仓库源码为 H7 HAL 1.11.6
Build           : CMake + GNU Arm Embedded Toolchain
HAL Timebase    : TIM6
Debug UART      : USART1, 115200 8N1
```

主要硬件引脚见 `02_HARDWARE_BASELINE.md`。

## 5. 当前尚未冻结的设计

以下内容不能在新对话中自行假定：

- PHY 最终 Address；
- MODE[2:0] strap 结果；
- nINTSEL 最终状态；
- Ethernet DMA Descriptor / Buffer 的 SRAM 地址；
- MPU Region；
- D-Cache 最终策略；
- RX/TX Descriptor 数量；
- RX/TX Buffer 数量；
- LwIP Heap / Pool 参数；
- `tcpip_thread` / Ethernet RX Task 的最终优先级；
- ETH IRQ 优先级；
- Link poll 周期；
- Socket / Netconn / Raw API 最终选择。

## 6. 下一工作单元

M0 已完成。

下一工作单元应进入 **M1 PHY Bring-up**，边界包括：

- 核对 LAN8720AI strap；
- 设计并实现 PHY Reset；
- 确认当前 HAL ETH Management API；
- 建立最小 MDIO Read / Write；
- 读取 PHY ID；
- 逐步验证 Auto-negotiation、Link、Speed、Duplex。

M1 中仍不进入 Ethernet DMA、LwIP、UDP/TCP。

## 7. 更新规则

完成每个工作单元后必须更新：

- 已完成项；
- 当前未完成项；
- 新确认硬件事实；
- 新 Accepted / Proposed 决策；
- 下一步建议。

`STATUS.md` 只描述当前事实，不记录完整讨论历史。
