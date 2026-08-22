# FreeRTOS / LwIP Runtime Design

本文描述 Driver Package 与 FreeRTOS / CMSIS-RTOS2 / LwIP 的运行时边界。当前 async RX 已实现并上板验证；async TX、ethernetif 和 LwIP 尚未实现。

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

未来 ethernetif 可以在该任务上下文把 Frame 转换为 LwIP pbuf；具体 pbuf ownership 在 M3 再冻结。

## 6. CubeMX Task generation

当前 Reference Example 的已验证版本仍使用 CubeMX 生成的 `StartEthernetRxTask()` wrapper，在 USER CODE 中注册 Demo Handler 后调用 `EthernetRtos_RxTask()`。

产品化后的倾向方案：

```text
Task Entry : EthernetRtos_RxTask
Generation : As weak
```

CubeMX 6.18.1 的同版本生成结果已确认 `As weak` 会生成 `__weak` Entry，并保留 CubeMX 对 Task attributes / `osThreadNew()` 的管理。这与 D021 边界一致。

但当前 Example 尚未实际切换后执行 Generate Code + Build + On-board，因此 D023 仍为 Proposed。不要直接手改 generated `freertos.c` outside USER CODE。

## 7. 非 CubeMX 用户

可以直接使用 RTOS API 创建 Task，入口指向 `EthernetRtos_RxTask()`。Driver Package 不依赖 `.ioc` 的具体 Task 名或 CubeMX 生成的 Task Handle 变量。

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

第二阶段将 Reference Example 移入 `examples/` 后仍需重新 Build / On-board，当前目录移动提交只能标记 Static Review。
