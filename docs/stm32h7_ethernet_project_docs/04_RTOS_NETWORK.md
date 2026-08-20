# FreeRTOS / LwIP Runtime Design

- 状态：Pending M3
- 当前用途：占位并声明该专题的冻结边界。

## 当前已知原则

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

- ISR 保持短小；
- ISR 不处理协议业务；
- FreeRTOS ISR API 必须满足中断优先级约束；
- LwIP 线程模型必须按实际 `NO_SYS` / `tcpip_thread` 配置确定；
- 应用 API（Socket / Netconn / Raw）尚未冻结；
- Task Priority 和 Stack Size 必须测量后确定。

## M3 前需要冻结

- `NO_SYS`；
- `tcpip_thread`；
- Ethernet RX Task 是否独立存在；
- RX 唤醒机制；
- TX 完成同步机制；
- ETH IRQ Priority；
- Link Poll 所在上下文；
- LwIP API；
- 线程安全调用边界；
- 各任务栈和优先级；
- 统计与监控方式。
