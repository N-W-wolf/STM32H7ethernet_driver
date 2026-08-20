# Project Status

- 更新时间：2026-08-20
- 当前阶段：M1 — PHY Bring-up / 核心功能已完成
- 当前状态：PHY 基础 Bring-up 已上板验证，可进入 M2 MAC / DMA；M1 稳定性补充测试仍可后续追加

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
- [x] 建立最小 FreeRTOS 基础工程；
- [x] 建立 BootstrapTask，并完成基础运行上板验证；
- [x] 配置 SYS Timebase = TIM6，SysTick 留给 FreeRTOS；
- [x] 配置 USART1：PA9 TX / PA10 RX，115200 8N1；
- [x] 建立 `printf -> _write() -> HAL_UART_Transmit() -> USART1` 调试输出链路；
- [x] 明确 CubeMX 生成代码与手工代码边界；
- [x] 建立 BSP 调试输出文件 `BSP/stm32h743vit6_iot/board_debug.c`；
- [x] 建立 PHY Reset BSP `BSP/stm32h743vit6_iot/board_ethernet.c/.h`；
- [x] 建立 Ethernet MDIO wrapper；
- [x] 建立 LAN8720 PHY Driver；
- [x] PHY Reset 上板验证；
- [x] MDIO Read / Write 上板验证；
- [x] PHY ID 上板验证；
- [x] PHY Address = 0；
- [x] MODE[2:0] = 111；
- [x] Auto-negotiation 上板验证；
- [x] Link Up / Link Down 上板验证；
- [x] 100 Mbit/s Full Duplex 上板验证；
- [x] 单次网线拔出 / 插回状态恢复验证。

## 2. M1 验证结果

当前第一验证板上板结果：

```text
PHY ID1     = 0x0007
PHY ID2     = 0xC0F1
Reg18       = 0x60E0
PHY Address = 0
MODE        = 111
Link        = Up / Down 可检测
Speed       = 100M
Duplex      = Full
```

当前软件路径：

```text
PC0 PHY Reset
    ↓
PHY ID polling + timeout
    ↓
MDIO Read / Write
    ↓
Restart Auto-negotiation
    ↓
Link / Auto-negotiation polling
    ↓
Speed / Duplex
    ↓
周期 Link polling
```

已实测网线拔出后可检测 Link Down，重新插入后可恢复 Link Up。

## 3. 当前未完成

### M1 补充验证

- [ ] 10 Mbit/s 实际链路；
- [ ] Half Duplex 实际链路；
- [ ] 连续多次插拔网线；
- [ ] 多次 STM32 重启；
- [ ] 多次 PHY Reset；
- [ ] 25 MHz PHY 晶振独立测量；
- [ ] PA1 / RMII_REF_CLK 独立测量；
- [ ] Link / Speed LED 行为专项验证。

### M2 ～ M6

尚未开始。

## 4. 当前已确认工程基线

```text
MCU              : STM32H743VIT6
PHY              : LAN8720AI
PHY Address      : 0
PHY Boot MODE    : 111 / All capable + Auto-negotiation
MAC/PHY Interface: RMII
RTOS             : FreeRTOS / CMSIS-RTOS v2
Network Stack    : LwIP（尚未接入）
CubeMX           : 6.18.1
STM32CubeH7      : V1.13.0
HAL component    : H7 HAL 1.11.6
Build            : CMake + GNU Arm Embedded Toolchain
HAL Timebase     : TIM6
Debug UART       : USART1, 115200 8N1
```

主要硬件引脚见 `02_HARDWARE_BASELINE.md`。

## 5. 当前尚未冻结的设计

以下内容不能在新对话中自行假定：

- `nINTSEL` 独立电气测量结果；
- Ethernet DMA Descriptor / Buffer 的 SRAM 地址；
- MPU Region；
- D-Cache 最终策略；
- RX/TX Descriptor 数量；
- RX/TX Buffer 数量；
- LwIP Heap / Pool 参数；
- `tcpip_thread` / Ethernet RX Task 的最终优先级；
- ETH IRQ 优先级；
- 最终 Link poll 周期和承载任务；
- Socket / Netconn / Raw API 最终选择。

## 6. 下一工作单元

下一工作单元进入 **M2：STM32H7 MAC / DMA**。

边界包括：

- 核对 STM32H743 Ethernet DMA 可访问内存；
- 确定 Descriptor / Buffer 放置；
- 明确 linker section；
- 确认 MPU / D-Cache 一致性策略；
- 核对当前 HAL 1.11.6 ETH Descriptor / Buffer API；
- 建立最小 TX / RX Frame 路径；
- 处理 ETH IRQ 与 FreeRTOS 边界；
- 通过 map 文件和上板结果验证实际地址与 DMA 可访问性。

M2 暂不进入 LwIP、Ping、UDP、TCP。

## 7. 更新规则

完成每个工作单元后必须更新：

- 已完成项；
- 当前未完成项；
- 新确认硬件事实；
- 新 Accepted / Proposed 决策；
- 下一步建议。

`07_STATUS.md` 只描述当前事实，不记录完整讨论历史。
