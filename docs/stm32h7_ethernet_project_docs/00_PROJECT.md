# STM32H7 Ethernet 通用驱动项目

- 状态：Active / M1 Ready
- 第一验证平台：STM32H743VIT6
- 第一验证 PHY：LAN8720AI
- 接口：RMII
- 软件环境：STM32 HAL + FreeRTOS + LwIP

## 1. 项目目标

开发一套面向 STM32H7 的可复用 Ethernet 基础组件。

第一版以 STM32H743VIT6 + LAN8720AI 为验证平台，完成从 PHY、MAC/DMA、FreeRTOS 异步收发到 LwIP 的完整基础链路，使 STM32 能够稳定与 PC/Linux 上位机进行网络通信。

第一版目标包括：

- LAN8720AI PHY 初始化与管理；
- MDIO/MDC 读写；
- PHY ID、Link、Speed、Duplex 获取；
- STM32H7 Ethernet MAC 初始化；
- Ethernet DMA Descriptor 与 Buffer 管理；
- STM32H7 MPU / D-Cache / DMA 内存一致性处理；
- FreeRTOS 下异步 Ethernet RX/TX；
- LwIP `ethernetif` 适配；
- 静态 IPv4；
- ICMP Ping；
- UDP Echo；
- TCP Echo；
- 错误统计、链路恢复和压力测试；
- 将板级差异、PHY 差异和通用驱动逻辑分离。

## 2. 当前不做

第一版暂不处理：

- 机器人 HostLink；
- CommandFrame / StateFrame；
- 机器人控制和安全状态机；
- DHCP、DNS、mDNS、TLS、HTTP 等扩展网络服务；
- 为未来可能存在的网络后端提前建立复杂统一接口；
- Zero Copy 极致优化；
- 与当前任务无关的 STM32 外设驱动。

机器人系统以后作为该 Ethernet 基础组件的上层用户接入。

## 3. 稳定的软件分层

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
    ↓
STM32 HAL / Hardware
```

基本依赖约束：

- Application 不直接操作 `HAL_ETH_xxx()`；
- LwIP 不进入 Ethernet Driver 内部；
- Ethernet Driver 不处理 UDP/TCP/IP 业务；
- PHY Driver 不依赖 LwIP；
- BSP 只承担板级差异；
- 板级引脚、Reset、时钟连接等信息不能散落在通用驱动中。

详细设计以 `01_ARCHITECTURE.md` 为准。

## 4. 开发里程碑

### M0：项目基线

状态：已完成运行基线，Debug / Release fresh build 的重复构建记录待补。

已完成：

- 项目文档与硬件基线；
- 实际代码仓库结构；
- CubeMX 生成代码与手工代码边界；
- FreeRTOS / CMSIS-RTOS v2 最小工程；
- TIM6 HAL Timebase；
- BootstrapTask；
- LED 心跳；
- USART1 基础调试输出；
- BSP 调试重定向；
- 上板运行验证。

### M1：PHY Bring-up

完成：

- PHY Reset；
- MDIO/MDC；
- PHY ID；
- PHY 初始化；
- Auto-negotiation；
- Link；
- Speed；
- Duplex。

### M2：MAC / DMA

完成：

- MAC；
- Descriptor；
- RX/TX Buffer；
- IRQ；
- DMA；
- MPU；
- Cache；
- 基础 Frame RX/TX。

### M3：LwIP + IP

完成：

- `ethernetif`；
- LwIP；
- Static IPv4；
- ARP；
- ICMP Ping。

### M4：UDP

完成：

- UDP Echo；
- 高频通信；
- 统计；
- Link 恢复。

### M5：TCP

完成：

- TCP Echo；
- Connect / Disconnect；
- 异常恢复。

### M6：通用化与验收

完成：

- Driver API 整理；
- Board Port 整理；
- PHY 抽象复核；
- 压力测试；
- 文档整理；
- 可复用性验证。

## 5. 多对话协作方式

聊天记录只承担讨论。

项目真实状态必须落到：

- 代码；
- `DECISIONS.md`；
- `STATUS.md`；
- `HANDOFF.md`；
- 专题设计文档。

每个新对话开始后：

1. 读取本文件；
2. 读取 `01_ARCHITECTURE.md`；
3. 读取 `02_HARDWARE_BASELINE.md`；
4. 读取 `06_DECISIONS.md`；
5. 读取 `07_STATUS.md`；
6. 读取 `08_HANDOFF.md`；
7. 根据当前状态和用户当前需求确定本对话职责；
8. 只在确定的职责范围内工作。

如果用户已经明确指定当前任务，则直接围绕该任务工作，不重新安排整个项目。

## 6. 项目资料优先级

发生冲突时，原则上采用以下优先级：

### 硬件连接

实际 PCB / 当前有效原理图  
→ 芯片 Datasheet  
→ 项目文档  
→ 推断

### LAN8720AI 行为

LAN8720A/LAN8720Ai Datasheet  
→ 当前驱动实测  
→ ST 示例或其他参考代码  
→ 网络资料

### STM32H743 外设行为

STM32H743 Reference Manual / Datasheet  
→ 当前项目使用的 HAL 源码  
→ STM32CubeH7 官方示例  
→ 其他资料

### 项目架构

Accepted `DECISIONS.md`  
→ `01_ARCHITECTURE.md`  
→ 专题设计文档  
→ 当前讨论中的临时方案

如果高优先级资料之间发生冲突，应显式记录，不能自行选择一个结果继续实现。

## 7. 文档职责

- `00_PROJECT.md`：项目范围、目标和协作规则；
- `01_ARCHITECTURE.md`：Ethernet 技术总纲；
- `02_HARDWARE_BASELINE.md`：当前验证板硬件事实；
- `03_MEMORY_DMA.md`：M2 前后形成，记录 DMA / MPU / Cache 最终方案；
- `04_RTOS_NETWORK.md`：M3 前后形成，记录 FreeRTOS + LwIP 任务和线程模型；
- `05_TEST_PLAN.md`：分阶段测试和最终验收；
- `06_DECISIONS.md`：跨模块设计决定；
- `07_STATUS.md`：当前项目事实和进度；
- `08_HANDOFF.md`：最近一次工作单元的交接信息。

`03_MEMORY_DMA.md`、`04_RTOS_NETWORK.md` 应在对应实现阶段基于资料和实际代码完善，不提前冻结。
