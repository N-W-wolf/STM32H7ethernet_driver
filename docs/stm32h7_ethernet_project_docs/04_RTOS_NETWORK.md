# FreeRTOS / LwIP Runtime Design

本文描述 Ethernet 数据路径与 FreeRTOS / LwIP 的运行时边界。当前 FreeRTOS 基础环境和 polling 裸 Frame 数据路径已经存在，ETH IRQ 异步收发、`ethernetif` 和 LwIP 网络接口尚未实现。

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

当前已验证的 polling 基线为：

```text
PHY Auto-negotiation
    ↓
MAC Speed / Duplex sync
    ↓
HAL_ETH_Start()
    ↓
EthernetDriver_Transmit() / EthernetDriver_Receive()
```

polling 路径用于验证 MAC/DMA、Buffer ownership 和 Frame 收发，不是最终 FreeRTOS 网络运行模型。

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

当前 polling Driver 已完成：

- `HAL_ETH_RxAllocateCallback()` 为 RX Descriptor 分配静态 DMA Buffer；
- `HAL_ETH_RxLinkCallback()` 将 DMA Buffer 数据复制到 CPU 侧单帧暂存区；
- callback 后立即归还 RX DMA Buffer；
- `EthernetDriver_Receive()` 将完整 Frame 复制给调用者；
- 单帧与连续 1000 帧 RX 已完成上板验证。

最终异步 RX 需要独立、可界定的任务上下文或 LwIP 允许的输入上下文，负责：

- 由 ETH IRQ / callback 唤醒；
- 调用 `HAL_ETH_ReadData()` / Driver RX 接口读取完成 Frame；
- 将 Frame 交给 `ethernetif`；
- 更新错误 / drop 统计；
- 避免在 ISR 中执行协议处理或大块复制。

RX Task 是否单独存在、使用 Task Notification 还是其他轻量同步机制尚未冻结，但下一实现优先验证 ETH IRQ + Task Notification + RX Task 的最小路径。

## 5. TX 同步与 ownership

当前 polling TX 已实现：

```text
Caller Frame
    ↓ memcpy
Static TX DMA Buffer
    ↓
HAL_ETH_Transmit(timeout)
    ↓ HAL_OK
Release TX Buffer
```

TX API 具有显式 timeout，不允许无限等待。

当前已上板验证正常发送成功路径。以下内容尚未完成：

- `HAL_ETH_Transmit_IT()` 异步发送；
- DMA completion notification；
- `HAL_ETH_ReleaseTxPacket()` / Tx free callback 的 Buffer 回收；
- DMA error / timeout 后的完整 Buffer recovery；
- Link Down 时的统一返回与清理行为。

因此当前 polling TX 是 bring-up / 基础 Frame API 基线，不等同于最终异步 TX 设计。

## 6. PHY Link 管理

PHY Driver 本身不依赖 FreeRTOS。

当前 PHY 状态通过周期轮询获取，已经能够读取：

- Link；
- Auto-negotiation；
- Speed；
- Duplex。

Bootstrap 过程中，首次 Auto-negotiation 成功后会把 PHY Speed / Duplex 同步到 MAC 并启动 MAC/DMA。

当前长期 polling 仍只记录 Link Up / Down 变化，尚未实现：

```text
Link Down
→ HAL_ETH_Stop()
→ ownership / pending packet cleanup
→ 等待 Link Up
→ 重新读取 Speed / Duplex
→ MAC reconfigure
→ HAL_ETH_Start()
```

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

Ethernet Driver 的 DMA Descriptor、DMA Buffer 和高速路径对象采用固定容量和明确 ownership。

当前 RX/TX DMA Buffer 均为 4 × 1536 B 静态 Pool，不使用隐藏动态分配。

LwIP 可以使用其自身的 pool / heap，但必须通过 `lwipopts.h` 明确预算。

Ethernet IRQ 和高速收发路径禁止隐藏的 `malloc` / `free`。

## 10. 支持状态

| 项目 | 状态 |
| --- | --- |
| FreeRTOS / CMSIS-RTOS v2 基础环境 | 已实现 |
| PHY 周期 Link polling | 已实现，当前作为验证载体 |
| MAC Speed / Duplex 同步 | 已实现，100M Full 已上板验证 |
| Polling Raw TX | 已实现并上板验证 |
| Polling Raw RX | 已实现，单帧与连续 1000 帧已上板验证 |
| ETH IRQ | 未实现 |
| RX Task / RX notification | 未实现 |
| TX completion notification | 未实现 |
| 完整 Link change MAC lifecycle | 未实现 |
| `ethernetif` | 未实现 |
| LwIP 网络接口 | 未实现 |
| Static IPv4 / Ping | 未实现 |
| UDP / TCP Application | 未实现 |
