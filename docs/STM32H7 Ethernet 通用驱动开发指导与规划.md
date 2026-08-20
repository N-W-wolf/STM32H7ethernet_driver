# STM32H7 Ethernet 通用驱动开发指导与规划

- 状态：Draft v0.1
- 目标平台：STM32H7，第一阶段以 STM32H743 为主要验证平台
- 软件环境：STM32 HAL + FreeRTOS + LwIP
- 网络接口：优先 RMII + 外置 PHY
- 适用范围：需要通过 Ethernet 与 PC、Linux 上位机或其他网络设备通信的 STM32H7 控制板

---

## 1. 文档目的

本文用于指导 STM32H7 Ethernet 通用驱动及其配套网络组件的设计、实现和测试。

当前阶段的目标是建立一套可以在不同 STM32H7 控制板之间复用的 Ethernet 基础组件，使应用程序能够稳定使用 UDP、TCP 等网络通信能力。

第一版优先完成：

- STM32H7 Ethernet MAC 初始化与运行；
- Ethernet DMA 描述符和收发缓冲区管理；
- RMII PHY 初始化、状态读取与 Link 检测；
- PHY 驱动与具体 PCB 解耦；
- FreeRTOS 下的异步 Ethernet 收发；
- LwIP 网络栈接入；
- 静态 IPv4；
- ICMP Ping；
- UDP Echo；
- 基础链路状态和错误统计；
- DMA、Cache 和内存布局验证；
- 长时间通信和高负载测试。

当前阶段不设计机器人控制协议、`HostLink`、`CommandFrame`、`StateFrame`、控制会话和机器人安全状态机。

这些功能未来作为 Ethernet 网络能力的上层用户接入。

这种划分延续现有 STM32H7 开发指导中的原则：底层模块保持明确的职责边界，上层不直接依赖具体硬件实现；板级配置负责外设、引脚、DMA、内存区和时钟等板卡差异。

---

# 2. 总体目标

最终希望达到以下使用方式：

```text
              Application
          UDP / TCP / 自定义协议
                  │
                  ▼
                LwIP
                  │
                  ▼
             ethernetif
                  │
                  ▼
         STM32H7 Ethernet Driver
              │          │
              │          └── PHY Driver
              │
              ▼
             BSP
              │
              ▼
        STM32H7 + PHY
```

应用层不应直接操作：

```text
ETH_HandleTypeDef
ETH_DMADescTypeDef
HAL_ETH_xxx()
PHY register
DMA descriptor
RMII GPIO
```

应用只使用 LwIP 或未来建立在 LwIP 之上的网络接口。

换用另一块 STM32H7 PCB 时，应尽量只修改：

```text
BSP
BoardConfig
PHY 配置
链接脚本 / MPU 配置
```

而保持以下模块不变：

```text
Ethernet Driver
ethernetif
LwIP
应用层网络逻辑
```

---

# 3. 设计原则

## 3.1 板级差异和驱动逻辑分离

不同 PCB 之间经常变化的是：

- RMII/MII 引脚；
- PHY 型号；
- PHY 地址；
- PHY Reset GPIO；
- REF_CLK 来源；
- ETH 外设时钟；
- DMA 内存区；
- MPU 配置。

这些信息属于 BSP 或板级配置。

Ethernet Driver 不应直接写死：

```c
GPIOG
GPIO_PIN_2
PHY_ADDR = 0
```

---

## 3.2 Ethernet Driver 不处理 IP、UDP 和 TCP

Ethernet Driver 处理的是 Ethernet Frame。

它不应该知道：

```text
192.168.1.10
UDP 5000
TCP Connection
DNS
DHCP
```

这些功能属于 LwIP。

驱动边界应保持：

```text
Ethernet Frame
      ↑↓
Ethernet Driver
```

而：

```text
UDP / TCP
   ↑↓
 LwIP
```

---

## 3.3 中断中不处理网络业务

Ethernet IRQ 只完成：

- 硬件状态确认；
- DMA 完成状态处理；
- 必要的时间或错误记录；
- 通知网络任务；
- 尽快退出中断。

禁止在 Ethernet IRQ 或 HAL 回调中：

- 解析 UDP；
- 执行 TCP 业务；
- 处理应用协议；
- 大量复制数据；
- `printf`；
- 动态申请内存；
- 执行耗时业务。

这与整个 STM32H7 项目对中断的要求一致：中断只进行必要的硬件处理和任务通知，复杂处理转移到任务上下文。

---

## 3.4 运行期间不依赖动态内存

底层驱动应尽量采用：

```text
固定 DMA Descriptor
固定 DMA Buffer
固定大小统计对象
静态 FreeRTOS 对象
```

运行期间避免在实时通信路径调用：

```text
malloc
free
new
delete
```

现有开发指导同样要求实时路径采用固定容量缓冲区，并明确缓冲区满时的行为。

LwIP 自身可能使用其内部内存池，但应通过 `lwipopts.h` 明确配置和预算。

---

# 4. 软件分层

推荐将 Ethernet 子系统划分为五层。

```text
┌──────────────────────────────┐
│ Application                  │
│ UDP Echo / TCP / Future App  │
├──────────────────────────────┤
│ LwIP                         │
│ IPv4 / ARP / ICMP / UDP/TCP  │
├──────────────────────────────┤
│ ethernetif                   │
│ LwIP ↔ Ethernet Driver       │
├──────────────────────────────┤
│ Ethernet Driver              │
│ MAC / DMA / PHY abstraction  │
├──────────────────────────────┤
│ BSP / Board Port             │
│ GPIO / Clock / Reset / MPU   │
└──────────────────────────────┘
```

---

# 5. BSP / Board Port

## 5.1 职责

BSP 负责描述“这块 PCB 上 Ethernet 是怎样连接的”。

包括：

```text
ETH 外设实例
RMII / MII 模式
RMII GPIO
ETH 时钟
PHY Reset
PHY 地址
REF_CLK
IRQ
DMA 内存区域
Cache / MPU 配置
```

BSP 不处理：

```text
Ethernet Frame
LwIP
UDP
TCP
应用协议
```

---

## 5.2 推荐接口

可以建立：

```c
typedef struct
{
    ETH_HandleTypeDef *heth;

    uint32_t phy_address;

    GPIO_TypeDef *phy_reset_port;
    uint16_t phy_reset_pin;

} EthernetBoardConfig;
```

每块板提供：

```c
extern const EthernetBoardConfig g_ethernet_board_config;
```

例如：

```c
const EthernetBoardConfig g_ethernet_board_config =
{
    .heth = &heth,
    .phy_address = 0,
    .phy_reset_port = GPIOG,
    .phy_reset_pin = GPIO_PIN_2,
};
```

具体内容根据实际 PCB 修改。

---

## 5.3 Board Port

推荐提供少量板级函数：

```c
void BoardEthernet_InitHardware(void);

void BoardEthernet_PhyReset(bool reset);

void BoardEthernet_DelayMs(uint32_t ms);

uint32_t BoardEthernet_GetTickMs(void);
```

必要时再加入：

```c
void BoardEthernet_CacheClean(
    const void *address,
    size_t length);

void BoardEthernet_CacheInvalidate(
    const void *address,
    size_t length);
```

Ethernet Driver 不应该直接操作具体 GPIO。

---

# 6. Ethernet Driver

## 6.1 职责

Ethernet Driver 是整个组件的核心。

主要负责：

```text
ETH MAC 初始化
DMA Descriptor 初始化
DMA RX/TX Buffer 管理
启动和停止 MAC
发送 Ethernet Frame
接收 Ethernet Frame
ETH IRQ 处理
DMA 错误处理
MAC 错误处理
PHY 管理入口
Link 状态维护
统计信息维护
```

---

## 6.2 不负责

Ethernet Driver 不应包含：

```text
IP 地址
ARP 业务逻辑
UDP
TCP
Socket
自定义网络协议
机器人协议
业务超时策略
```

---

## 6.3 推荐基础接口

底层建议保持 C 接口：

```c
typedef enum
{
    ETHERNET_LINK_DOWN = 0,
    ETHERNET_LINK_UP,

} EthernetLinkState;
```

统计：

```c
typedef struct
{
    uint32_t rx_frames;
    uint32_t tx_frames;

    uint32_t rx_errors;
    uint32_t tx_errors;

    uint32_t rx_dropped;
    uint32_t tx_dropped;

    uint32_t dma_errors;

} EthernetStatistics;
```

对外接口：

```c
bool Ethernet_Init(void);

bool Ethernet_Start(void);

void Ethernet_Stop(void);

EthernetLinkState Ethernet_GetLinkState(void);

void Ethernet_GetStatistics(
    EthernetStatistics *statistics);
```

数据收发接口的最终形式需要结合 HAL ETH 和 `ethernetif` 设计。

不要为了形式完整，过早建立与 HAL 数据模型完全重复的一套复杂对象。

---

# 7. STM32H7 MAC 与 DMA

STM32H7 Ethernet MAC/DMA 是整个 Driver 中 MCU 相关性最高的部分。

这一层可以直接接受：

```c
ETH_HandleTypeDef
ETH_DMADescTypeDef
HAL_ETH_xxx()
```

其实现主要围绕：

```text
HAL_ETH_Init
HAL_ETH_Start
HAL_ETH_Stop
HAL_ETH_Transmit
HAL_ETH_ReadData

HAL_ETH_RxCpltCallback
HAL_ETH_TxCpltCallback
HAL_ETH_ErrorCallback
```

展开。

应用代码和 LwIP 上层不得直接访问 `heth`。

---

# 8. PHY 驱动

## 8.1 为什么 PHY 要独立

STM32H7 内部包含 Ethernet MAC，但通常需要外接 PHY。

可能使用：

```text
LAN8742A
DP83848
RTL8201
KSZ8081
...
```

不同 PHY 的：

```text
PHY ID
Reset 行为
寄存器定义
自动协商
Link 状态
速度读取
双工读取
特殊状态寄存器
```

存在差异。

因此 PHY Driver 应独立于 STM32H7 MAC Driver。

---

## 8.2 PHY 公共状态

推荐定义：

```c
typedef enum
{
    PHY_SPEED_10M,
    PHY_SPEED_100M,

} EthernetPhySpeed;
```

```c
typedef enum
{
    PHY_DUPLEX_HALF,
    PHY_DUPLEX_FULL,

} EthernetPhyDuplex;
```

```c
typedef struct
{
    bool link_up;

    EthernetPhySpeed speed;
    EthernetPhyDuplex duplex;

} EthernetPhyStatus;
```

---

## 8.3 PHY Driver 接口

可以使用函数表：

```c
typedef struct
{
    bool (*init)(void);

    bool (*get_status)(
        EthernetPhyStatus *status);

} EthernetPhyDriver;
```

例如：

```c
extern const EthernetPhyDriver LAN8742_Driver;
extern const EthernetPhyDriver DP83848_Driver;
```

第一版只有一种 PHY 时也可以先直接调用对应驱动，但目录和职责应提前分开。

---

# 9. MDIO / MDC

PHY Driver 需要读写 PHY Register。

建议由 STM32H7 Ethernet MAC 层提供：

```c
bool EthernetMac_ReadPhyRegister(
    uint32_t phy_address,
    uint32_t reg,
    uint32_t *value);
```

```c
bool EthernetMac_WritePhyRegister(
    uint32_t phy_address,
    uint32_t reg,
    uint32_t value);
```

数据流为：

```text
LAN8742 Driver
      │
      ▼
PHY Read / Write API
      │
      ▼
STM32H7 MAC Driver
      │
      ▼
HAL ETH
      │
      ▼
MDIO / MDC
      │
      ▼
PHY
```

PHY Driver 不直接依赖具体 `ETH_HandleTypeDef`。

---

# 10. DMA Descriptor 与 Buffer

## 10.1 基本要求

Ethernet DMA Descriptor 和数据缓冲区必须静态分配。

例如：

```c
#define ETH_RX_DESC_COUNT    4U
#define ETH_TX_DESC_COUNT    4U

#define ETH_RX_BUFFER_SIZE   1536U
#define ETH_TX_BUFFER_SIZE   1536U
```

具体数量后续根据测试结果调整。

不要先追求很大的 Buffer 数量。

---

## 10.2 内存位置

STM32H743 的不同 SRAM 并不具有相同 DMA 可访问性。

Ethernet DMA Buffer 不允许随意放置。

现有开发指导已经规定：

- 外设 DMA Buffer 禁止放入 DTCM；
- DMA Buffer 位置必须通过链接脚本明确；
- Cache 操作需要考虑 Cache Line 对齐。

因此建议建立：

```text
.eth_dma
```

专用段。

例如：

```c
#define ETH_DMA_ALIGN \
    __attribute__((aligned(32)))

#define ETH_DMA_SECTION \
    __attribute__((section(".eth_dma")))
```

然后：

```c
ETH_DMA_SECTION
ETH_DMA_ALIGN
static ETH_DMADescTypeDef rx_desc[ETH_RX_DESC_COUNT];
```

```c
ETH_DMA_SECTION
ETH_DMA_ALIGN
static uint8_t rx_buffer[
    ETH_RX_DESC_COUNT][ETH_RX_BUFFER_SIZE];
```

实际内存地址由 Linker Script 决定。

---

# 11. MPU 与 D-Cache

STM32H7 Ethernet 开发必须把 DMA 和 Cache 作为第一阶段问题处理。

不能等出现偶发网络错误以后再补。

---

## 11.1 推荐第一版方案

第一版优先考虑：

```text
专用 Ethernet DMA 区域
        +
MPU 设置为合适的 Non-cacheable 区域
```

目标是降低 DMA 与 CPU Cache 一致性问题的复杂度。

等基本驱动稳定以后，再评估：

```text
Cacheable
+
Clean / Invalidate
```

是否能够带来值得采用的性能收益。

---

## 11.2 如果采用 Cacheable DMA Buffer

必须保证：

DMA TX：

```text
CPU 写 Buffer
      ↓
Cache Clean
      ↓
DMA 发送
```

DMA RX：

```text
DMA 写 Buffer
      ↓
Cache Invalidate
      ↓
CPU 读取
```

还必须保证：

```text
地址按 Cache Line 对齐
长度按 Cache Line 处理
DMA 持有 Buffer 时 CPU 不修改
```

这些要求与现有 H7 开发指导一致。

---

# 12. Buffer 所有权

每一个 Ethernet Buffer 必须具有明确所有权。

RX：

```text
DMA Owns
   │
   │ Receive Complete
   ▼
CPU / ethernetif Owns
   │
   │ Processing Complete
   ▼
DMA Owns
```

TX：

```text
CPU Owns
   │
   │ Data Ready
   ▼
DMA Owns
   │
   │ Transmission Complete
   ▼
CPU Owns
```

禁止出现：

```text
DMA 正在使用 Buffer
        +
CPU 同时修改 Buffer
```

Buffer 状态必须可以从代码逻辑中明确判断。

---

# 13. ethernetif

`ethernetif` 是：

```text
LwIP
 ↕
Ethernet Driver
```

之间的适配层。

它既不是 Ethernet Driver，也不是业务应用。

---

## 13.1 主要职责

包括：

```text
LwIP netif 初始化
low_level_init
low_level_output
Ethernet RX 输入
pbuf 与 Driver Buffer 之间的数据交互
Link 状态同步
```

典型接口包括：

```c
static void low_level_init(
    struct netif *netif);
```

```c
static err_t low_level_output(
    struct netif *netif,
    struct pbuf *p);
```

以及 Ethernet RX → LwIP 的处理流程。

---

## 13.2 设计目标

Ethernet Driver 不应知道：

```c
struct netif
struct pbuf
```

而 `ethernetif` 可以同时知道：

```text
LwIP
Ethernet Driver
```

因此未来如果更换 TCP/IP Stack：

```text
Ethernet Driver
```

原则上保持不变。

只替换协议栈适配层。

---

# 14. LwIP

第一阶段使用 LwIP 提供：

```text
ARP
IPv4
ICMP
UDP
TCP
```

初期建议关闭没有必要的功能。

第一版网络环境优先：

```text
Static IPv4
No DHCP
No DNS
No mDNS
No TLS
No HTTP Server
```

首先保证基本链路稳定。

---

# 15. FreeRTOS 架构

FreeRTOS 场景下，Ethernet 收发必须采用异步方式。

推荐数据流：

```text
               Ethernet PHY
                    │
                    ▼
                ETH MAC
                    │
                    ▼
                   DMA
                    │
                    ▼
                ETH IRQ
                    │
                    │ Notify
                    ▼
        Ethernet / LwIP RX Task
                    │
                    ▼
                ethernetif
                    │
                    ▼
                  LwIP
                    │
           ┌────────┴────────┐
           ▼                 ▼
       UDP App             TCP App
```

---

# 16. Ethernet IRQ

Ethernet IRQ 的优先级需要满足 FreeRTOS ISR API 的限制。

如果 ISR 中调用：

```c
xTaskNotifyFromISR()
```

或者：

```c
xSemaphoreGiveFromISR()
```

其 NVIC Priority 必须满足 FreeRTOS 对 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 的要求。

这一部分应统一纳入整个工程的中断优先级规划。

Ethernet IRQ 中原则上只做：

```text
确认 ETH/DMA 状态
记录错误
完成必要 HAL 处理
通知 RX Task
触发必要的 TX Complete 处理
退出
```

---

# 17. Ethernet RX Task

推荐建立一个网络输入任务。

例如：

```text
EthRxTask
```

概念流程：

```c
for (;;)
{
    wait_for_rx_notification();

    while (ethernet_frame_available())
    {
        process_one_received_frame();
    }
}
```

实际处理通过：

```text
Ethernet Driver
      ↓
ethernetif
      ↓
LwIP
```

完成。

如果使用 LwIP 的 `tcpip_thread`，任务和消息投递关系需要按照 LwIP 的线程模型执行。

不要让应用任务绕过 LwIP 线程安全要求直接操作不允许跨线程调用的 Raw API。

---

# 18. LwIP API 选择

LwIP 常见接口包括：

```text
Raw API
Netconn API
Socket API
```

第一版建议根据使用目标选择。

如果主要用于学习和验证：

```text
Socket API
```

使用最直观。

如果未来追求更低开销和更明确的实时行为：

```text
Raw API
```

可以再评估。

当前阶段不需要提前建立：

```text
INetworkTransport
UdpTransport
TcpTransport
```

等复杂业务抽象。

先把 Ethernet Driver 和 LwIP 基础设施稳定下来。

---

# 19. Link 状态管理

Ethernet Link 状态主要由 PHY 决定。

Driver 应至少能够区分：

```text
Link Down
Link Up 10M Half
Link Up 10M Full
Link Up 100M Half
Link Up 100M Full
```

状态变化流程可以为：

```text
PHY status
    │
    ▼
Ethernet Driver
    │
    ▼
ethernetif
    │
    ▼
LwIP netif Link State
```

插拔网线时：

```text
拔线
 ↓
PHY Link Down
 ↓
netif_set_link_down()
```

重新连接：

```text
PHY Link Up
 ↓
更新 MAC Speed / Duplex
 ↓
netif_set_link_up()
```

具体 MAC 重配置流程需要结合当前 STM32 HAL ETH 实现验证。

---

# 20. Link 检测机制

第一版可以使用周期任务检测 PHY。

例如：

```text
100 ms ～ 500 ms
```

检查一次 Link。

可以放在：

```text
Ethernet Link Task
```

也可以放进已有网络管理任务。

不要为了 Link 检测单独创建复杂调度系统。

如果 PHY 提供 Link Interrupt，并且后续确实需要，可以再接入 GPIO IRQ。

第一版轮询更容易验证。

---

# 21. 错误与统计

Ethernet Driver 至少记录：

```text
RX frame count
TX frame count

RX error
TX error

RX drop
TX drop

DMA error

PHY Link Up count
PHY Link Down count

Driver reset count
```

LwIP 层还可以另外统计：

```text
UDP
TCP
pbuf
memory pool
```

应用层不要依赖串口日志判断通信是否健康。

应通过结构化统计对象读取。

---

# 22. 调试输出

高频 Ethernet 路径禁止直接：

```c
printf(...)
```

推荐：

```text
IRQ / Driver
    ↓
计数器 / 固定事件
    ↓
低优先级 Monitor Task
    ↓
限速打印
```

现有 STM32H7 指导也要求高优先级路径只记录固定大小事件或计数器，低优先级任务负责限速日志。

---

# 23. 推荐目录结构

建议工程结构：

```text
Drivers/
└── Ethernet/
    ├── include/
    │   ├── ethernet.h
    │   ├── ethernet_types.h
    │   ├── ethernet_config.h
    │   └── ethernet_phy.h
    │
    ├── src/
    │   ├── ethernet.c
    │   └── ethernet_stm32h7.c
    │
    └── phy/
        ├── lan8742.c
        ├── lan8742.h
        ├── dp83848.c
        └── dp83848.h

BSP/
└── <board>/
    ├── board_ethernet.c
    └── board_ethernet.h

Middleware/
└── LwIP/
    ├── ethernetif.c
    ├── ethernetif.h
    └── lwipopts.h

App/
└── EthernetDemo/
    ├── ethernet_demo.c
    ├── udp_echo.c
    └── tcp_echo.c
```

如果 CubeMX 自动生成：

```text
Core/Src/ethernetif.c
```

第一阶段可以保留自动生成结构。

但后续应逐渐控制：

```text
哪些是 CubeMX 管理代码
哪些是项目自己的 Driver
```

避免把大量自定义业务代码长期写进 CubeMX 自动生成区域。

---

# 24. C / C++ 边界

Ethernet 基础设施推荐主要使用 C：

```text
BSP                    C
STM32 HAL              C
Ethernet Driver        C
PHY Driver             C
ethernetif             C
LwIP                   C
```

上层应用可以使用：

```text
C
或
C++
```

未来机器人项目如果使用 C++，可以在外层建立：

```cpp
class NetworkInterface;
class UdpTransport;
class HostLink;
```

底层 Ethernet Driver 不需要因此改写。

这符合整个 H7 项目推荐的 C/C++ 使用方式：CubeMX、HAL、中断和底层 BSP 保持窄 C 接口，上层逻辑再根据需要使用受限 C++。

---

# 25. 第一阶段：PHY Bring-up

目标：

```text
STM32
 ↕ MDIO/MDC
PHY
```

完成：

```text
PHY Reset
读取 PHY ID
初始化 PHY
启动 Auto Negotiation
读取 Link
读取 Speed
读取 Duplex
```

测试：

```text
未插网线 → Link Down

插入网线
    ↓
Link Up
100 Mbps
Full Duplex

拔线
    ↓
Link Down
```

串口可以低频输出：

```text
Ethernet PHY detected
PHY ID: ...

Link UP
Speed: 100 Mbps
Duplex: Full
```

退出条件：

PHY 连续插拔、重启和上电均可以可靠识别状态。

---

# 26. 第二阶段：MAC + DMA

完成：

```text
ETH MAC init
DMA Descriptor
RX Buffer
TX Buffer
ETH IRQ
RX Complete
TX Complete
```

重点验证：

```text
DMA Buffer 地址正确
Descriptor 地址正确
Buffer Alignment 正确
无 DTCM DMA Buffer
MPU / Cache 配置正确
```

在这一阶段就要检查 DMA、Cache 和 Buffer 所有权，不能等接入 LwIP 后再一起调试。

---

# 27. 第三阶段：LwIP + Ping

完成：

```text
ethernetif
LwIP
Static IPv4
ARP
ICMP
```

测试配置示例：

```text
PC:
192.168.10.1

STM32:
192.168.10.2

Mask:
255.255.255.0
```

PC：

```bash
ping 192.168.10.2
```

至少测试：

```text
持续 Ping
快速插拔网线
STM32 重启
PC 重启
不同 Ping 包长度
长时间运行
```

退出条件：

网络可长期稳定 Ping，无异常 HardFault、DMA 错误和内存损坏。

---

# 28. 第四阶段：UDP Echo

实现：

```text
PC
 │
 │ UDP
 ▼
STM32
 │
 │ Echo
 ▼
PC
```

测试：

```text
不同 Payload Size
不同发送频率
持续高频 UDP
突然停止发送
重新发送
拔线
重新连接
STM32 重启
```

记录：

```text
RX packets
TX packets
Drop
Error
CPU usage
Task stack
LwIP memory usage
```

退出条件：

UDP 可以稳定长时间通信。

---

# 29. 第五阶段：TCP Echo

UDP 稳定以后再验证 TCP。

目的不是将 TCP 作为未来机器人控制链路的默认选择，而是验证整个 Ethernet + LwIP 组件具有通用网络能力。

测试：

```text
connect
send
receive
disconnect
reconnect
client crash
cable disconnect
STM32 reset
```

避免第一阶段同时调试 TCP 状态机和底层 Ethernet。

---

# 30. 第六阶段：压力测试

完成完整基础网络功能以后，进行高负载测试。

至少包括：

```text
持续 UDP RX
持续 UDP TX
双向通信
接近 MTU 的 UDP Payload
大量小包
随机包长度
快速 Link Up / Down
持续数小时运行
```

观察：

```text
HardFault
DMA Error
RX Drop
TX Drop
pbuf exhaustion
FreeRTOS stack
CPU load
Cache inconsistency
内存破坏
```

---

# 31. FreeRTOS 测试项

必须检查：

```text
EthRxTask stack high-water mark
tcpip_thread stack
任务优先级
任务阻塞状态
ISR → Task 通知
高负载调度延迟
FreeRTOS Heap
LwIP Memory Pool
```

禁止只验证：

```text
“能够 Ping 通”
```

Ping 成功只能证明最基本路径可以运行。

---

# 32. 单元测试

可以脱离目标板测试的部分应尽量测试。

例如：

```text
PHY Register Decode
Link State Decode
Speed / Duplex Decode
Driver State Machine
Buffer Index
Ring Buffer 边界
错误统计
配置合法性
```

HAL 强相关和 DMA 行为则主要通过目标板集成测试验证。

---

# 33. 配置设计

建议至少分成：

```text
EthernetDriverConfig
EthernetBoardConfig
LwipConfig
```

其中：

`EthernetBoardConfig`：

```text
heth
PHY address
PHY type
PHY reset
RMII/MII
```

`EthernetDriverConfig`：

```text
RX descriptor count
TX descriptor count
Buffer size
Timeout
```

LwIP 配置继续通过：

```text
lwipopts.h
```

管理。

不要建立一个巨大：

```text
EthernetConfig
```

把 MCU、PCB、PHY、LwIP 和应用配置全部放在一起。

---

# 34. API 设计原则

所有 Driver API 应满足：

```text
参数明确
状态明确
错误可检测
无隐藏阻塞
无不受控动态内存
无应用语义
```

不建议提供：

```c
Ethernet_SendToPC(...)
```

因为 Driver 不知道什么叫“PC”。

也不建议：

```c
Ethernet_SendUdp(...)
```

UDP 属于 LwIP。

底层只处理 Ethernet Device。

---

# 35. 超时原则

Driver 内部所有可能等待硬件的流程必须有明确超时。

禁止：

```c
while (!done)
{
}
```

无限等待。

初始化阶段如 PHY Auto Negotiation 等流程可以等待，但必须：

```text
有 deadline
有错误返回
有统计
有日志
```

运行阶段优先通过：

```text
IRQ
DMA
Task Notification
```

驱动流程。

---

# 36. 第一版不追求 Zero Copy

第一版允许：

```text
DMA Buffer
   ↓ copy
LwIP pbuf
```

或者：

```text
LwIP pbuf
   ↓ copy
DMA Buffer
```

只要性能满足需求。

不要一开始同时实现：

```text
Zero Copy
Cacheable DMA
Custom pbuf
复杂 Buffer Pool
多 Buffer Scatter/Gather
```

第一版优先保证：

```text
正确
可测
稳定
边界清晰
```

完成性能测试后再决定是否需要 Zero Copy。

---

# 37. 第一版不做复杂网络管理

暂时不需要：

```text
DHCP
DNS
mDNS
HTTP
WebSocket
TLS
NTP
SNMP
```

这些都可以作为未来网络 Middleware 能力增加。

第一版只验证：

```text
Static IPv4
ICMP
UDP
TCP
```

---

# 38. 与未来机器人软件的关系

未来接入机器人系统时，基础架构可以直接保留：

```text
STM32H7 Ethernet Driver
        ↓
ethernetif
        ↓
LwIP
```

当前：

```text
LwIP
 ↓
UDP Echo
```

未来变成：

```text
LwIP
 ↓
UdpTransport
 ↓
HostLink
 ↓
RobotRuntime
```

因此机器人协议不会侵入：

```text
PHY
MAC
DMA
Ethernet Driver
ethernetif
LwIP
```

现有机器人下位机指导也已经将 `HostLink` 定义为上下位机帧、链路状态和通信协议所在的位置，而底层网络只负责提供通信能力。

---

# 39. 第一版验收标准

Ethernet Driver v0.1 至少满足：

- [ ] STM32H743 + 指定 PHY 可以稳定启动；
- [ ] PHY ID 可以正确读取；
- [ ] Link Up / Down 可以正确检测；
- [ ] Speed / Duplex 可以正确识别；
- [ ] Ethernet DMA Descriptor 和 Buffer 位于合法内存区；
- [ ] DMA Buffer 不位于 DTCM；
- [ ] DMA Buffer 对齐正确；
- [ ] MPU / Cache 策略明确并经过验证；
- [ ] IRQ 中不存在复杂业务处理；
- [ ] FreeRTOS RX 处理采用异步方式；
- [ ] LwIP 可以稳定工作；
- [ ] PC 可以持续 Ping STM32；
- [ ] UDP Echo 稳定工作；
- [ ] TCP Echo 稳定工作；
- [ ] 插拔网线后可以恢复；
- [ ] STM32 重启后网络可以恢复；
- [ ] PC 重启后网络可以恢复；
- [ ] 高频 UDP 压力测试不存在明显异常；
- [ ] RX/TX/Error/Drop 具备统计；
- [ ] FreeRTOS 任务栈有测量；
- [ ] 长时间测试不存在 HardFault、内存损坏或异常卡死；
- [ ] 应用代码不直接操作 `HAL_ETH_xxx()`；
- [ ] 更换同类 STM32H7 控制板主要修改 BSP 和配置；
- [ ] 更换 PHY 时主要新增或替换 PHY Driver。

---

# 40. 推荐开发顺序

整个开发过程按以下顺序推进：

```text
Board / CubeMX
      ↓
PHY MDIO/MDC
      ↓
PHY Link Detection
      ↓
MAC Init
      ↓
DMA Descriptor / Buffer
      ↓
IRQ
      ↓
MPU / Cache
      ↓
ethernetif
      ↓
LwIP
      ↓
Ping
      ↓
UDP Echo
      ↓
TCP Echo
      ↓
压力测试
      ↓
接口整理
      ↓
形成可复用 Driver
```

不要先写完整的抽象层再第一次连接网线。

每完成一个层次，都应有能够独立验证它的测试程序。

---

# 41. 推荐里程碑

## M1：PHY 可用

完成：

```text
PHY Reset
PHY ID
MDIO/MDC
Link
Speed
Duplex
```

---

## M2：Ethernet MAC/DMA 可用

完成：

```text
MAC
Descriptor
Buffer
IRQ
DMA
MPU
Cache
```

---

## M3：IP 网络可用

完成：

```text
ethernetif
LwIP
Static IPv4
Ping
```

---

## M4：UDP 可用

完成：

```text
UDP Echo
高频通信
统计
Link 恢复
```

---

## M5：TCP 可用

完成：

```text
TCP Echo
连接和断连
异常恢复
```

---

## M6：形成通用组件

完成：

```text
Driver API 整理
Board Port 整理
PHY 抽象
配置整理
测试整理
文档整理
```

这一阶段以后，才认为 Ethernet Driver v0.1 基本完成。

---

# 42. 最终职责边界

整个 Ethernet 基础设施最终保持以下关系：

```text
Application
负责：
“网络数据代表什么”

LwIP
负责：
“IP / UDP / TCP 怎样工作”

ethernetif
负责：
“LwIP 怎样使用这个 Ethernet Device”

Ethernet Driver
负责：
“STM32H7 怎样稳定收发 Ethernet Frame”

PHY Driver
负责：
“PHY 怎样初始化以及当前链路状态是什么”

BSP
负责：
“这块 PCB 的 Ethernet 硬件具体怎样连接”
```

其中最重要的约束是：

> Ethernet Driver 只解决 Ethernet Device 本身的问题；协议栈、应用通信协议和机器人业务依次位于其上层。板级差异通过 BSP 和配置注入，不进入通用驱动核心。

第一版优先实现一个能够稳定运行、能够被测试、能够换板复用的基础组件。等 Ping、UDP、TCP、长时间压力测试和 DMA/Cache 验证全部完成后，再考虑 Zero Copy、复杂网络管理以及机器人上位机通信协议。