# FreeRTOS / LwIP Runtime Design

本文描述 Ethernet Driver Package 与 FreeRTOS / CMSIS-RTOS2 / LwIP 的运行时边界。

当前已经实现并上板验证 ETH IRQ + CMSIS-RTOS2 异步 RX；异步 TX、`ethernetif` 和 LwIP 尚未实现。

## 1. 目标运行路径

```text
PHY / MAC
    ↓
DMA
    ↓
ETH IRQ
    ↓
Ethernet Driver RX event
    ↓
CMSIS-RTOS2 Thread Flag
    ↓
Ethernet RX Task
    ↓
Rx Frame Handler
    ↓
ethernetif
    ↓
LwIP
    ↓
Application
```

当前已验证到 `Rx Frame Handler`。

## 2. Driver Core 与 RTOS 的边界

Driver Core：

```text
Ethernet/Src/ethernet_driver.c
```

不 include：

```text
FreeRTOS.h
task.h
cmsis_os2.h
```

Driver 只提供通用 RX complete 事件：

```c
void EthernetDriver_SetRxEventHandler(EthernetDriverRxEventHandler handler, void *context);
```

`HAL_ETH_RxCpltCallback()` 在 ISR 上下文调用该轻量 handler。

具体 RTOS notification 由可选 Adapter 实现：

```text
Ethernet/RTOS/CMSIS_RTOS2/
```

## 3. ISR 约束

ETH ISR 只允许：

- `HAL_ETH_IRQHandler()`；
- HAL RX complete callback；
- 轻量状态记录；
- RTOS ISR-safe notification；
- 必要的上下文切换请求。

禁止：

- 协议解析；
- UDP / TCP 业务；
- 应用控制逻辑；
- `printf`；
- 动态内存；
- 大量数据复制；
- 无界循环；
- 阻塞等待。

当前 CMSIS-RTOS2 Adapter 在 RX event 中调用 `osThreadFlagsSet()`。当前 wrapper 在 ISR 内部最终使用 FreeRTOS FromISR API。

## 4. 中断优先级

当前 `FreeRTOSConfig.h`：

```text
configPRIO_BITS = 4
configLIBRARY_LOWEST_INTERRUPT_PRIORITY = 15
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
```

当前参考 Demo：

```text
ETH_IRQn preemption priority = 5
subpriority                  = 0
```

由于 ETH ISR 会调用 RTOS ISR-safe API，NVIC library priority 数值必须处于 FreeRTOS 允许调用系统 API 的范围。

`5` 只是当前已验证参考值，不冻结为所有板子的固定配置。

## 5. Task ownership

RTOS Adapter 不创建 Task。

应用 / CubeMX 负责：

```text
Task object
priority
stack
static / dynamic allocation
lifetime
```

Package 提供：

```c
void EthernetRtos_RxTask(void *argument);
```

当前 Demo 仍由 CubeMX 创建 `EthernetRxTask`，其生成入口只作为 wrapper：

```text
StartEthernetRxTask()
→ 注册 Demo RxFrameHandler
→ EthernetRtos_RxTask()
```

最终是否推荐 CubeMX `As external` 还是 `As weak` 尚未冻结，需要用 CubeMX 6.18.1 实际生成比较。

## 6. RX Task 初始化

`EthernetRtos_RxTask()` 启动后：

```text
osThreadGetId()
→ 保存自身 Task Handle
→ EthernetDriver_SetRxEventHandler()
→ g_ready = true
→ 等待 Thread Flag
```

应用在启动 MAC/DMA 前可以通过：

```c
EthernetRtos_IsReady()
```

确认 RX runtime 已完成绑定。

这样不依赖“RX Task priority 恰好高于 BootstrapTask”这一偶然时序。

## 7. RX notification 与 drain

一次 Thread Flag 不是“一帧计数器”。多个 RX IRQ 可以在 Task 得到 CPU 前合并成一个事件。

因此 Task 被唤醒后必须：

```text
EthernetDriver_Receive()
→ FRAME: 继续读取
→ FRAME: 继续读取
→ ...
→ ETHERNET_RX_NONE: 停止 drain，重新等待事件
```

不能写成“一次通知只调用一次 Receive”。

当前 Adapter 已按该方式实现。

## 8. Frame 向上交付

Adapter API：

```c
void EthernetRtos_SetRxFrameHandler(EthernetRtosRxFrameHandler handler, void *context);
```

Handler 在 Ethernet RX Task 上下文同步执行。

ownership：

```text
frame pointer
只在 Handler 调用期间有效
Handler 返回后不得继续持有
```

如果上层需要长期保存 Frame，需要自行 copy。

当前 Demo 的 `0x88B5` 测试统计放在 `freertos.c` 的 Demo Handler 中，不进入 Driver Package。

未来 LwIP 接入时，可以由 Handler 创建/填充 pbuf 并交给 `ethernetif`，但具体 pbuf ownership 和 TCP/IP thread 边界将在 LwIP 工作单元中冻结。

## 9. RX Buffer ownership

Driver 内部当前 copy-based：

```text
DMA RX Buffer
→ HAL_ETH_ReadData()
→ HAL_ETH_RxLinkCallback()
→ memcpy Driver CPU Frame
→ 立即归还 DMA RX Buffer
→ HAL 重建 Descriptor
→ EthernetDriver_Receive() 复制给 RTOS Adapter CPU Frame
→ Rx Frame Handler
```

上层从不持有 DMA RX Buffer。

## 10. TX

当前 TX 仍为 polling：

```text
Caller Frame
→ memcpy Static TX DMA Buffer
→ HAL_ETH_Transmit(timeout)
→ HAL_OK
→ release TX Buffer
```

尚未实现：

- `HAL_ETH_Transmit_IT()`；
- TX complete task notification；
- `HAL_ETH_ReleaseTxPacket()` / Tx free callback ownership；
- error / timeout 后统一回收。

因此“FreeRTOS 异步 RX 已实现”不等于“异步收发均完成”。

## 11. PHY Link 管理

PHY Driver 自身不依赖 RTOS。

当前 Bootstrap 首次 Auto-negotiation 成功后：

```text
Lan8720Status
→ Speed / Duplex 映射
→ EthernetRtos_IsReady()
→ EthernetDriver_ConfigureLink()
→ EthernetDriver_Start()
```

长期 PHY polling 当前仍只记录 Link 变化，没有完成：

```text
Link Down
→ HAL_ETH_Stop_IT()
→ pending ownership cleanup
→ Link Up
→ MAC reconfigure
→ HAL_ETH_Start_IT()
```

完整 Link lifecycle 尚未冻结。

## 12. LwIP

尚未接入，因此以下仍未冻结：

- `NO_SYS`；
- `tcpip_thread` 参数；
- Socket / Netconn / Raw API；
- pbuf / memp；
- LwIP heap / pool；
- RX Frame Handler 与 `ethernetif` / tcpip thread 的具体调用边界。

## 13. 任务资源与测量

当前 Demo `EthernetRxTask` 使用：

```text
Priority   = AboveNormal
Stack      = 256 words / 1024 B
Allocation = Dynamic
```

这些是 bring-up 参数，不是最终冻结值。

后续至少测量：

- stack high-water mark；
- 网络负载下 Task runtime；
- RX/TX drop；
- DMA error；
- CPU load；
- Link 抖动 / 重连行为。

## 14. 当前验证状态

| 项目 | 状态 |
| --- | --- |
| FreeRTOS / CMSIS-RTOS2 基础环境 | 已实现 |
| Polling Raw TX | 已实现并上板验证 |
| Polling Raw RX | 单帧 + 1000 / 1000 上板验证 |
| ETH IRQ | 已实现并上板验证 |
| CMSIS-RTOS2 RX Thread Flag | 已实现并上板验证 |
| RX Task drain | 已实现并上板验证 |
| Async RX 1000 / 1000 | 已上板验证，重构前固件 `6b2f1f4...` |
| Package 化 RTOS Adapter | Static Review，待重新 build / 上板 |
| Async TX completion | 未实现 |
| DMA error / drop 统计 | 未实现 |
| 完整 Link lifecycle | 未实现 |
| `ethernetif` / LwIP | 未实现 |
| Ping / UDP / TCP | 未实现 |
