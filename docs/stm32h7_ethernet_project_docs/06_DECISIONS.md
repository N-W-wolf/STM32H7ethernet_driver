# Design Decisions

本文件只记录会跨模块影响后续工作的设计决定。

状态含义：

- `Accepted`：后续对话默认必须遵守；
- `Proposed`：当前倾向方案，尚未冻结；
- `Superseded`：已被后续决定替代；
- `Rejected`：讨论过但不采用。

如果要改变 `Accepted` 决策，应新增一条决策说明原因，并将旧决策标记为 `Superseded`，不要直接删除历史。

---

## D001 第一验证平台

- 状态：Accepted
- 日期：2026-08-20

决定：

第一版 Bring-up 使用：

```text
STM32H743VIT6
+
LAN8720AI
+
RMII
```

驱动整体目标仍是面向 STM32H7 的可复用组件。

---

## D002 软件运行环境

- 状态：Accepted
- 日期：2026-08-20

第一版采用：

```text
STM32 HAL
FreeRTOS
LwIP
```

Ethernet RX/TX 按 FreeRTOS 异步场景设计。

---

## D003 基础软件分层

- 状态：Accepted
- 日期：2026-08-20

保持：

```text
Application
    ↓
LwIP
    ↓
ethernetif
    ↓
Ethernet Driver
    ├── STM32H7 MAC / DMA
    └── PHY abstraction
             ↓
          PHY Driver
    ↓
BSP / Board Port
```

不允许把 IP/UDP/TCP 或机器人业务塞入 Ethernet Driver。

---

## D004 底层语言边界

- 状态：Accepted
- 日期：2026-08-20

以下部分优先使用 C：

- BSP；
- PHY；
- STM32H7 MAC / DMA；
- `ethernetif`；
- LwIP 接口代码。

未来上层应用可以使用 C 或 C++。

---

## D005 第一版网络能力范围

- 状态：Accepted
- 日期：2026-08-20

第一版验证：

```text
Static IPv4
ICMP Ping
UDP Echo
TCP Echo
```

暂不加入：

```text
DHCP
DNS
mDNS
TLS
HTTP
```

---

## D006 第一版不主动追求 Zero Copy

- 状态：Accepted
- 日期：2026-08-20

优先完成可验证、稳定的 Ethernet 数据路径。

允许在 `ethernetif` 与 DMA Buffer / LwIP pbuf 之间复制。

只有在正确性、压力测试和性能测量完成后，才评估 Zero Copy。

---

## D007 DMA / Cache 第一版方案

- 状态：Proposed
- 日期：2026-08-20

当前倾向：

```text
专用 .eth_dma 区域
+
明确 MPU 属性
+
优先选择易验证的一致性方案
```

是否最终全部使用 Non-cacheable，需要在 M2 根据 STM32H743 内存映射、HAL 数据结构和性能测试确认。

在确认前，任何模块不得假定最终 Cache 策略已经冻结。

---

## D008 PHY Link 检测

- 状态：Proposed
- 日期：2026-08-20

第一版倾向使用周期轮询 PHY Link。

暂不依赖 PHY nINT。

具体周期在 M1/M3 结合任务结构确定。

---

## D009 LwIP 应用 API

- 状态：Proposed
- 日期：2026-08-20

用于 Echo/Bring-up 的应用 API 暂未冻结。

候选：

- Socket API：调试直观；
- Netconn API；
- Raw API：开销低，但线程模型要求更严格。

在 M3 前确定。

---

## D010 多对话项目状态管理

- 状态：Accepted
- 日期：2026-08-20

聊天历史不作为项目唯一状态源。

所有跨对话的重要信息必须进入：

- 代码；
- `DECISIONS.md`；
- `STATUS.md`；
- `HANDOFF.md`；
- 对应专题文档。

每个新对话只承担一个经过当前对话确认的工作范围。
