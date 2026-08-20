# Project Status

- 更新时间：2026-08-20
- 当前阶段：M0 — 项目基线 / 开发准备
- 当前状态：尚未进入正式 PHY/MAC 编码

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
- [x] 建立多对话协作所需项目文档骨架。

## 2. 当前未完成

### M0

- [ ] 确定代码仓库实际基础结构；
- [ ] 确定 CubeMX / STM32CubeH7 版本；
- [ ] 确定编译工具链；
- [ ] 确定当前工程是否从 CubeMX 空工程开始；
- [ ] 建立最小 FreeRTOS 基础工程；
- [ ] 形成第一版实际目录；
- [ ] 建立最基本串口/调试输出能力。

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

## 3. 当前已确认硬件

```text
MCU : STM32H743VIT6
PHY : LAN8720AI
MAC/PHY Interface : RMII
RTOS : FreeRTOS
Network Stack : LwIP
```

主要引脚见 `02_HARDWARE_BASELINE.md`。

## 4. 当前尚未冻结的设计

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

## 5. 推荐下一工作单元

下一次新对话应先读取项目文档，再根据用户意图确定职责。

如果没有指定任务，当前合理候选为：

1. **工程基础与目录搭建**
   - CubeMX 基础工程；
   - FreeRTOS；
   - HAL；
   - Driver/BSP/Middleware 目录；
   - 生成代码与自定义代码边界。

2. **M1 PHY 设计准备**
   - LAN8720A strap；
   - Reset；
   - MDIO 接口边界；
   - PHY Driver API；
   - M1 测试方案。

不要在同一次工作单元里同时完成 M0、M1、M2。

## 6. 更新规则

完成每个工作单元后必须更新：

- 已完成项；
- 当前未完成项；
- 新确认硬件事实；
- 新 Accepted / Proposed 决策；
- 下一步建议。

`STATUS.md` 只描述当前事实，不记录完整讨论历史。
