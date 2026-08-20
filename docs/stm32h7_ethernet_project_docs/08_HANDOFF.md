# Latest Handoff

- 来源工作单元：M0 工程基线与目录搭建
- 日期：2026-08-20
- 当前阶段：M0 收尾完成，下一阶段为 M1 PHY Bring-up

## 1. 本次完成内容

M0 已建立并上板验证最小工程运行基线：

- STM32H743VIT6 CubeMX 基础工程；
- FreeRTOS / CMSIS-RTOS v2；
- HAL Timebase = TIM6；
- `BootstrapTask`；
- LED1 周期心跳；
- USART1 基础调试输出；
- `printf -> _write() -> HAL_UART_Transmit() -> USART1`；
- Ubuntu `/dev/ttyACM0` 串口接收；
- CubeMX 生成代码与手工维护代码边界；
- BSP 调试输出文件；
- 项目状态文档纳入版本控制。

当前工程使用：

```text
CubeMX          : 6.18.1
STM32CubeH7     : V1.13.0
HAL component   : 当前仓库源码为 H7 HAL 1.11.6
RTOS            : FreeRTOS / CMSIS-RTOS v2
HAL Timebase    : TIM6
Debug UART      : USART1, PA9 TX / PA10 RX, 115200 8N1
```

## 2. 本次修改 / 新增代码

当前与 M0 手工逻辑直接相关的文件：

```text
BSP/stm32h743vit6_iot/board_debug.c
Core/Src/freertos.c                  # 仅 USER CODE 区域加入心跳与 printf
CMakeLists.txt                       # 引入 BSP 调试源文件
```

CubeMX 生成或更新：

```text
Core/Inc/FreeRTOSConfig.h
Core/Inc/usart.h
Core/Src/freertos.c
Core/Src/usart.c
Core/Src/main.c
Core/Src/stm32h7xx_it.c
Core/Src/stm32h7xx_hal_timebase_tim.c（若由当前 CubeMX 版本按该形式生成）
Middlewares/Third_Party/FreeRTOS/**
cmake/stm32cubemx/CMakeLists.txt
stm32H7ethernet_demo.ioc
```

具体生成文件名以后续仓库实际内容为准，不依赖其他 CubeH7 版本示例。

## 3. 当前接口与代码位置

### Bootstrap 心跳

位置：

```text
Core/Src/freertos.c
StartBootstrapTask()
```

当前职责：

```text
LED1 周期翻转
+
低频 printf 启动日志
```

该任务仅用于 M0 基线验证，不代表未来 Ethernet Task 的最终任务模型、优先级或栈大小。

### Debug UART

硬件初始化由 CubeMX 管理：

```text
Core/Src/usart.c
MX_USART1_UART_Init()
```

手工调试重定向位于：

```text
BSP/stm32h743vit6_iot/board_debug.c
_write()
```

当前调用链：

```text
printf
  ↓
_write
  ↓
HAL_UART_Transmit
  ↓
USART1
```

该阻塞调试路径仅允许低频使用，不进入 Ethernet ISR 或高频数据路径。

## 4. 已执行测试与结果

已实际确认：

- [x] FreeRTOS Scheduler 正常启动；
- [x] `BootstrapTask` 可持续运行；
- [x] LED1 心跳正常；
- [x] USART1 初始化正常；
- [x] `printf` 重定向链路正常；
- [x] Ubuntu 端可以接收到周期日志；
- [x] 当前固件可以编译、烧录并上板运行。

尚未在本工作单元中形成完整记录：

- [ ] Debug / Release 两套 `--fresh` 可重复构建结果。

因此 `05_TEST_PLAN.md` 中“工程可重复编译”仍保留为待补测试项，不把未记录结果写成已完成事实。

## 5. 板级实测注意事项

当前验证板实测发现 UART 相关 PCB 丝印与有效原理图的 TX/RX 标识相反。

已验证 MCU 信号定义仍为：

```text
PA9  = USART1_TX
PA10 = USART1_RX
```

后续调试接线以实际 MCU 信号和当前有效原理图为准，不依赖该处 PCB 丝印。

该结论仅适用于当前验证板。

## 6. 本次新增 Accepted 设计决定

已在 `06_DECISIONS.md` 新增：

- D011：M0 FreeRTOS 与 HAL 时间基线；
- D012：M0 基础调试输出；
- D013：CubeMX 生成代码与手工代码边界。

仍未冻结：

- Ethernet Task 优先级；
- ETH IRQ 优先级；
- DMA / MPU / Cache 策略；
- PHY Address / Strap 最终值；
- LwIP API 和任务模型。

## 7. 下一工作单元开始时必须读取

1. `00_PROJECT.md`
2. `01_ARCHITECTURE.md`
3. `02_HARDWARE_BASELINE.md`
4. `06_DECISIONS.md`
5. `07_STATUS.md`
6. 本文件
7. LAN8720A/LAN8720AI Datasheet
8. 当前有效 STM32H743VIT6 原理图
9. 当前仓库使用的 HAL 源码中与 ETH / PHY Management 相关的实现

## 8. 下一工作单元推荐边界

下一工作单元进入 **M1 PHY Bring-up**。

建议按最小可验证顺序推进：

1. 核对 LAN8720AI strap；
2. 明确 PHY Reset 时序与 BSP 边界；
3. 检查当前 HAL 1.11.6 的 ETH / PHY Management API；
4. 建立最小 MDIO Read / Write；
5. 读取 PHY ID；
6. 再处理 Auto-negotiation、Link、Speed、Duplex。

M1 不进入：

- Ethernet DMA Descriptor / Buffer；
- MPU / Cache 最终方案；
- LwIP；
- Ping；
- UDP / TCP。
