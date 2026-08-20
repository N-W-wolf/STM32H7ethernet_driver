# Latest Handoff

- 来源工作单元：项目规划与多对话协作设计
- 日期：2026-08-20
- 当前阶段：M0

## 1. 本次完成

已建立项目持续上下文的基本结构：

- 项目总览；
- 硬件基线；
- 设计决策；
- 当前状态；
- 交接机制；
- 项目级 Prompt。

确认第一验证硬件：

```text
STM32H743VIT6
+
LAN8720AI
+
RMII
```

现有《STM32H7 Ethernet 通用驱动开发指导与规划》继续作为技术总纲。

## 2. 本次未修改代码

当前尚未建立或修改正式 Ethernet Driver 源代码。

因此没有接口可以被后续模块视为已实现。

## 3. 下一工作单元开始时必须读取

1. `00_PROJECT.md`
2. `01_ARCHITECTURE.md`
3. `02_HARDWARE_BASELINE.md`
4. `06_DECISIONS.md`
5. `07_STATUS.md`
6. 本文件
7. 当前任务涉及的 Datasheet / Reference Manual / 原理图

## 4. 当前需要特别注意

不要将以下内容当成已经确认：

- PHY Address；
- LAN8720 strap 最终值；
- DMA 内存布局；
- MPU / Cache 最终策略；
- LwIP API；
- FreeRTOS 任务优先级。

当前原理图表明 LAN8720AI 使用 25 MHz 晶振，并将 `nINT/REFCLKO` 接到 STM32H743 `PA1_RMII_REF_CLK`。该时钟拓扑需要在 M1 中通过 strap 核对和板级测量最终确认。

## 5. 下一次对话如何确定职责

如果用户直接指定任务，例如：

> 这次先搭目录。

则只做该任务。

如果用户只说：

> 继续。

则应根据 `STATUS.md` 提出 1～2 个边界清楚的候选工作单元，让用户选择或确认。

不要默认直接进入 PHY、MAC 或 LwIP。
