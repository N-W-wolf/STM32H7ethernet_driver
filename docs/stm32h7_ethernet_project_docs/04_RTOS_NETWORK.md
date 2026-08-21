# FreeRTOS / LwIP Runtime Design

本文描述 Ethernet 数据路径与 FreeRTOS / LwIP 的运行时边界。当前 FreeRTOS 基础环境已经存在，Ethernet IRQ 异步收发、`ethernetif` 和 LwIP 网络接口尚未实现。

## 1. 运行路径

目标运行关系：

```text
PHY / MAC
    ↓
DMA
    ↓
ETH IRQ
    ↓ notification
Ethernet RX context
    ↓
ethernetif
    ↓
LwIP
    ↓
Application
```

## 2. ISR 约束

ETH ISR 只执行必要的硬件处理和任务通知。

允许：

- 调用 HAL Ethernet IRQ Handler；
- 记录必要状态；
- 更新轻量事件标志；
- 使用符合优先级约束的 FreeRTOS FromISR API；
- 请求必要的上下文切换。

禁止：

- 协议解析；
- UDP / TCP 业务；
- 应用控制逻辑；
- `printf`；
- 动态内存申请；
- 无界循环；
- 等待网络对端；
- 大量数据复制。

## 3. FreeRTOS 中断优先级

当前 `FreeRTOSConfig.h` 使用：

```text
configPRIO_BITS = 4
configLIBRARY_LOWEST_INTERRUPT_PRIORITY = 15
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
```

如果 ETH ISR 调用 FreeRTOS FromISR API，NVIC library priority 的数值必须满足 FreeRTOS 可调用系统 API 的范围；不得把 ETH IRQ 配置成高于允许阈值后继续调用 FromISR API。

ETH IRQ 的最终优先级尚未确定。

## 4. RX 上下文

Ethernet RX 数据不在 ISR 中直接送入协议业务。

RX 需要独立、可界定的任务上下文或 LwIP 允许的输入上下文，负责：

- 读取 HAL RX 完成的数据；
- 完成 Buffer ownership 交接；
- 将 Frame 交给 `ethernetif`；
- 更新错误 / drop 统计；
- 及时把 DMA Buffer 归还给接收路径。

RX Task 是否单独存在、使用 Task Notification 还是其他轻量同步机制，当前尚未确定。

## 5. TX 同步

TX API 不允许无限等待 Descriptor 或网络对端。

需要明确：

- Buffer / Descriptor 可用性；
- 发送 timeout；
- DMA 完成通知；
- TX Buffer 回收；
- Link Down 时的返回行为；
- DMA Error 时的恢复行为。

当前 TX Frame 数据路径尚未实现。

## 6. PHY Link 管理

PHY Driver 本身不依赖 FreeRTOS。

当前 PHY 状态通过周期轮询获取，已经能够读取：

- Link；
- Auto-negotiation；
- Speed；
- Duplex。

Link polling 的最终任务归属和周期需要与 Ethernet Runtime 一起确定。周期参数不得进入 PHY Driver 形成固定依赖。

## 7. LwIP 线程模型

LwIP 尚未接入，因此以下配置当前没有固定值：

- `NO_SYS`；
- `tcpip_thread` 参数；
- Socket / Netconn / Raw API 选择；
- pbuf / memp 参数；
- LwIP Heap / Pool 大小；
- Ethernet RX 上下文与 `tcpip_thread` 的调用边界。

实现时必须遵守所选 LwIP API 的线程安全规则，不能从任意任务直接调用只允许 TCP/IP 线程访问的内部接口。

## 8. 任务资源

任务优先级和栈大小必须通过实际运行测量确定。

至少需要记录：

- stack high-water mark；
- 网络负载下任务运行时间；
- RX/TX drop；
- DMA error；
- CPU load；
- Link 抖动 / 重连时的任务行为。

不在文档中把未经测量的示例数字写成固定配置。

## 9. 动态内存

Ethernet Driver 的 DMA Descriptor、DMA Buffer 和高速路径对象应采用固定容量和明确 ownership。

LwIP 可以使用其自身的 pool / heap，但必须通过 `lwipopts.h` 明确预算。

Ethernet IRQ 和高速收发路径禁止隐藏的 `malloc` / `free`。

## 10. 支持状态

| 项目 | 状态 |
| --- | --- |
| FreeRTOS / CMSIS-RTOS v2 基础环境 | 已实现 |
| PHY 周期 Link polling | 已实现，当前作为验证载体 |
| ETH IRQ | 未实现 |
| RX Task / RX notification | 未实现 |
| TX completion notification | 未实现 |
| `ethernetif` | 未实现 |
| LwIP 网络接口 | 未实现 |
| Static IPv4 / Ping | 未实现 |
| UDP / TCP Application | 未实现 |
