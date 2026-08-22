# FreeRTOS / LwIP Runtime Design

本文描述 Driver Package 与 FreeRTOS / CMSIS-RTOS2 / LwIP 的运行时边界。当前 async RX 已实现并上板验证；async TX、ethernetif 和 LwIP 尚未实现。

如果需要理解 weak symbol、HAL callback、运行时 Handler 注册、IRQ/Task 交接和 RX Buffer ownership 的完整调用过程，见：[`docs/ETHERNET_RUNTIME_FLOW.md`](../ETHERNET_RUNTIME_FLOW.md)。

## 1. 运行时分层

```text
Application / LwIP
        ↓
Frame Handler / ethernetif
        ↓
CMSIS-RTOS2 Adapter
        ↓
Ethernet Driver
        ↓
HAL ETH / DMA
```

Driver Core 不 include FreeRTOS/CMSIS-RTOS2；RTOS Adapter 是可选层。

## 2. Task ownership

Adapter 不创建 Task，也不隐藏 heap 使用。

```text
Application / CubeMX
→ Task object
→ priority
→ stack
→ static/dynamic allocation

Ethernet RTOS Adapter
→ EthernetRtos_RxTask()
→ task handle
→ Thread Flag
→ Receive drain
```

当前 API：

```text
EthernetRtos_SetRxFrameHandler()
EthernetRtos_IsReady()
EthernetRtos_RxTask()
```

Task priority / stack 当前参考值只是 bring-up 参数，不作为 Driver 固定值；最终应根据实际 stack high-water mark 和系统调度测量调整。

## 3. RX IRQ 路径

当前已验证：

```text
ETH_IRQHandler()
→ HAL_ETH_IRQHandler()
→ HAL_ETH_RxCpltCallback()
→ Ethernet Driver generic RX event
→ EthernetRtos_OnRxEvent()
→ osThreadFlagsSet()
→ EthernetRtos_RxTask()
```

ISR 只做必要 HAL 处理和事件通知，不执行 Frame copy、协议解析、应用业务或 `printf`。

当前 ETH IRQ priority = 5，与当前 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5` 配合。这个数字属于 Reference Example，不冻结为通用配置。

## 4. RX Task

Task 启动时：

```text
osThreadGetId()
→ 保存自身 handle
→ EthernetDriver_SetRxEventHandler()
→ ready = true
```

MAC/DMA Start 前应确认：

```c
EthernetRtos_IsReady() == true
```

避免 MAC 已接收但 notification 尚未绑定。

收到 Thread Flag 后必须 drain：

```text
EthernetDriver_Receive()
→ frame
EthernetDriver_Receive()
→ frame
...
EthernetDriver_Receive()
→ ETHERNET_RX_NONE
```

Thread Flag 是事件位，不是 Packet Counter；多个 IRQ 可以合并成一次任务唤醒。

## 5. Frame Handler

应用通过：

```c
EthernetRtos_SetRxFrameHandler(handler, context);
```

在任务上下文接收完整 Frame。frame pointer 只在 Handler 调用期间有效，返回后不得继续持有；需要长期保存时由上层复制。

Reference Example 的 `0x88B5` / 1000 Frame 计数属于 Demo 测试逻辑，不进入 Package。

未来 ethernetif 可以在该任务上下文把 Frame 转换为 LwIP pbuf；具体 pbuf ownership 在 LwIP 工作单元再冻结。

## 6. CubeMX Task generation

当前 Reference Example 已采用：

```text
Task Entry : EthernetRtos_RxTask
Generation : As weak
```

CubeMX 6.18.1 的实际生成结果：

- CubeMX 继续生成 Task attributes 和 `osThreadNew()`；
- generated `freertos.c` 提供 `__weak void EthernetRtos_RxTask(void *argument)` stub；
- `Ethernet/RTOS/CMSIS_RTOS2/Src/ethernet_rtos.c` 提供同名强定义；
- 链接时由 Package 强定义承担实际 RX Task 逻辑。

该方式已经在当前 Reference Example 上完成 Generate Code、Build 和 On-board async RX 回归，因此 D023 已冻结为 Accepted。

非 CubeMX 用户可以直接使用 CMSIS-RTOS2 / FreeRTOS API 创建 Task，入口同样指向 `EthernetRtos_RxTask()`。

## 7. Runtime registration 边界

真正的运行时 Handler 注册有两层：

```text
EthernetDriver_SetRxEventHandler()
→ Driver → RTOS Adapter

EthernetRtos_SetRxFrameHandler()
→ RTOS Adapter → Application / ethernetif
```

前者由 `EthernetRtos_RxTask()` 内部自动完成，普通使用者通常不需要直接调用；后者是上层决定完整 Frame 最终交给谁的接口。

HAL 的 `HAL_ETH_RxCpltCallback()`、`HAL_ETH_RxLinkCallback()`、`HAL_ETH_RxAllocateCallback()` 属于 HAL 固定 callback，不是通过上述注册接口绑定的函数。

## 8. TX Runtime

当前 TX 仍使用 polling `HAL_ETH_Transmit()`。尚未实现：

```text
HAL_ETH_Transmit_IT()
→ TX IRQ
→ HAL_ETH_ReleaseTxPacket()
→ TxFree callback
→ TX Buffer recycle
```

异步 TX completion ownership 必须单独设计，不能用 RX 的 Thread Flag 方案顺手推断。

## 9. Error / Link lifecycle

尚未完成：

- DMA fatal / RBU / timeout recovery；
- RX/TX drop/error 统计；
- Link Down 时 MAC stop；
- Link Up 后 speed/duplex reconfigure/start；
- 初始 Auto-negotiation 超时后晚到 Link Up 的完整启动路径。

当前 PHY Link 继续轮询，200 ms 只是 bring-up 值。

## 10. LwIP

尚未实现：ethernetif、netif state、ARP、IPv4、Ping、UDP、TCP。

进入 LwIP 后仍保持：

```text
LwIP
→ ethernetif
→ Ethernet Driver / RTOS runtime
```

Driver Core 不引入 LwIP API。

## 11. 当前验证

On-board Verified：

```text
ETH IRQ
→ Driver RX event
→ CMSIS-RTOS2 Thread Flag
→ RX Task drain
→ Frame Handler
→ RX Buffer recycle
```

连续 1000 / 1000，PC 约 5 ms / Frame。该结果不代表高负载或长时间压力测试。

Reference Example 移入 `examples/` 后已经重新完成 Build / map / On-board 回归；CubeMX `As weak` Task Entry 方案也已完成同样回归。