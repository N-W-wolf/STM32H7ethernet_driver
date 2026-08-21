# STM32H7 Ethernet Architecture

本文描述 STM32H7 Ethernet 通用驱动的稳定软件分层、依赖方向和模块职责。项目实现以 STM32 HAL + FreeRTOS + LwIP 为运行环境，当前验证硬件为 STM32H743VIT6 + LAN8720AI + RMII。

## 1. 总体分层

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

稳定依赖方向只允许自上而下调用。底层模块不得反向依赖应用业务。

核心约束：

- Application 不直接操作 `HAL_ETH_xxx()`；
- LwIP 不进入 Ethernet Driver 内部；
- Ethernet Driver 不处理 IP、UDP、TCP、Socket 或机器人业务协议；
- PHY Driver 不依赖 LwIP；
- BSP 只处理板级差异；
- CubeMX、HAL、中断和底层驱动保持 C 接口；
- 不为暂未出现的需求预建复杂抽象。

## 2. BSP / Board Port

BSP 描述某块 PCB 上 Ethernet 相关硬件如何连接和准备。

当前职责包括：

- PHY Reset GPIO；
- Ethernet DMA 所用板级 SRAM 的准备；
- 需要由板级代码显式使能的 SRAM / 外设相关时钟；
- 其他无法由通用 Driver 推导的硬件差异。

当前验证板提供：

```c
void BoardEthernet_PhyResetAssert(void);
void BoardEthernet_PhyResetRelease(void);
void BoardEthernet_PrepareDmaMemory(void);
```

`BoardEthernet_PrepareDmaMemory()` 在当前 STM32H743VIT6 板上使能 D2 SRAM3 时钟。

BSP 不处理 Ethernet Frame、PHY 协议行为、LwIP 或应用业务。

## 3. MDIO Wrapper

PHY 通过 STM32 Ethernet MAC 的 Management Interface 访问 Clause 22 寄存器。

当前接口：

```c
EthernetMdio_Read()
EthernetMdio_Write()
```

该层负责封装当前工程使用的 STM32H7 HAL 1.11.6 PHY Management API，并限制 PHY / Register 地址等基本参数。

PHY Driver 不直接依赖 `ETH_HandleTypeDef` 的具体使用方式。

## 4. PHY Driver

PHY Driver 负责具体 PHY 芯片行为，包括：

- PHY ID；
- Reset 后可管理状态判断；
- Auto-negotiation；
- Link；
- Speed；
- Duplex；
- PHY 特定状态寄存器解析。

LAN8720AI 当前接口：

```c
Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

约束：

- 只通过 MDIO Wrapper 访问 PHY；
- 不依赖 FreeRTOS；
- 不依赖 LwIP；
- 不通过固定长延时判断 PHY ready；
- 对 MDIO 无响应值进行错误处理；
- Link 状态读取遵守 PHY Datasheet 的 latch 行为。

当前验证板已完成 PHY Reset、MDIO Read/Write、PHY ID、Address、Strap、Auto-negotiation、Link Up/Down、100 Mbit/s 和 Full Duplex 上板验证。

## 5. STM32H7 MAC / DMA

STM32H7 MAC / DMA 层可以直接使用：

```text
ETH_HandleTypeDef
ETH_DMADescTypeDef
HAL_ETH_xxx()
```

该层职责包括：

- MAC 配置；
- DMA Descriptor 管理；
- RX/TX Buffer ownership；
- Ethernet Frame 发送和接收；
- DMA / MAC 错误处理；
- ETH IRQ 对接；
- 为 `ethernetif` 提供稳定的 Frame 级接口。

当前 Frame 级接口：

```c
void EthernetDriver_Init(void);
bool EthernetDriver_ConfigureLink(EthernetLinkSpeed speed, EthernetDuplexMode duplex);
bool EthernetDriver_Start(void);
bool EthernetDriver_Transmit(const uint8_t *frame, uint16_t length, uint32_t timeout_ms);
EthernetRxResult EthernetDriver_Receive(uint8_t *frame, uint16_t capacity, uint16_t *length);
```

当前实现使用 4 个 RX Descriptor、4 个 TX Descriptor，并分别配置 4 个 1536 B 静态 DMA Buffer。第一版数据路径采用 copy-based ownership：

```text
RX DMA Buffer
    ↓ HAL_ETH_RxLinkCallback()
CPU 侧单帧暂存
    ↓
调用者 Frame Buffer
```

RX DMA Buffer 在 callback 完成复制后立即归还 RX Pool，HAL 可重新挂接给 Descriptor。

TX 路径为：

```text
调用者 Frame
    ↓ memcpy
TX DMA Buffer
    ↓
HAL_ETH_Transmit() polling
    ↓ HAL_OK
归还 TX Buffer
```

PHY Auto-negotiation 得到的 Speed / Duplex 由调用层转换为通用 Driver 类型，再通过 `EthernetDriver_ConfigureLink()` 写入 MAC。MAC/DMA 使用 `EthernetDriver_Start()` 启动。

裸 Frame TX 已由 PC 抓包上板验证；裸 Frame RX 已完成单帧与连续 1000 帧验证。该结果证明基础 Buffer recycle 和 polling 数据路径可持续工作，不代表高负载压力测试已经完成。

ETH IRQ、FreeRTOS 异步 RX/TX、DMA 错误恢复和完整 Link change 生命周期尚未实现。

DMA / MPU / linker 的详细配置见 `03_MEMORY_DMA.md`。

## 6. ethernetif

`ethernetif` 是 LwIP 与 Ethernet Driver 的适配层。

职责：

```text
LwIP pbuf
    ↕
ethernetif
    ↕
Ethernet Frame API
```

`ethernetif` 可以根据性能和 ownership 规则进行必要的数据复制，但不承担板级 GPIO、PHY Reset 或 MAC 寄存器配置。

当前 `ethernetif` 尚未实现。

## 7. LwIP 与 Application

LwIP 负责：

- ARP；
- IPv4；
- ICMP；
- UDP；
- TCP；
- pbuf / memory pool；
- 网络接口状态。

Application 只通过 LwIP 或建立在 LwIP 之上的应用接口使用网络能力。

Static IPv4、Ping、UDP Echo 和 TCP Echo 当前尚未实现。

机器人 HostLink、CommandFrame、StateFrame 和控制业务不属于 Ethernet Driver。

## 8. FreeRTOS 与中断边界

ETH ISR 必须保持短小，只执行：

- HAL / DMA 必要中断处理；
- 状态记录；
- 任务通知；
- 快速退出。

禁止在 ETH ISR 或高频 HAL callback 中：

- 协议解析；
- 应用业务；
- `printf`；
- 无界循环；
- 动态内存申请；
- 长时间阻塞。

如果 ETH ISR 调用 FreeRTOS FromISR API，中断优先级必须满足当前 `FreeRTOSConfig.h` 的 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 约束。

任务优先级、栈大小和最终 RX/TX 唤醒机制必须通过实际运行测量确定，不在架构文档中固定经验数字。

运行时约束见 `04_RTOS_NETWORK.md`。

## 9. DMA / Cache / MPU 边界

Ethernet DMA Descriptor 和 Buffer 必须显式放置，不能依赖默认 `.bss` 或普通 `static` 数组所在内存恰好可被 DMA 访问。

必须明确：

- Ethernet DMA Master 可访问的 SRAM；
- Descriptor 地址；
- Buffer 地址；
- Cache Line 对齐；
- MPU Memory Attribute；
- D-Cache Clean / Invalidate 策略；
- DMA / CPU ownership；
- linker section；
- `.map` / ELF 中的实际地址。

当前验证板使用独立 SRAM3 作为 Ethernet DMA 专用内存，并将该区域配置为 Non-cacheable；Descriptor 额外使用 Device memory 属性覆盖。RX/TX Buffer section 也显式放置在该 SRAM3 中。

## 10. CubeMX 与项目维护边界

CubeMX / ST 管理：

```text
stm32H7ethernet_demo.ioc
Core/**
Drivers/CMSIS/**
Drivers/STM32H7xx_HAL_Driver/**
cmake/stm32cubemx/CMakeLists.txt
CubeMX 生成的 Third_Party middleware
```

`Core/**` 的长期手工逻辑只放在 `USER CODE BEGIN / END` 区域。

项目手工维护：

```text
BSP/**
Drivers/Ethernet/**
Middlewares/Network/**
App/**
顶层 CMakeLists.txt
项目脚本
技术文档
Ethernet DMA 相关 linker 配置
```

当前项目不使用 CubeMX Memory Management Tool 自动管理 Ethernet DMA linker section。MPU 参数由 `.ioc` 保存，Ethernet DMA 物理 section 由项目 linker 显式控制。

## 11. 可移植性边界

迁移到另一块 STM32H7 PCB 时，优先只修改：

```text
CubeMX .ioc
BSP / Board Port
PHY Driver（仅 PHY 型号变化时）
板级 linker / MPU
```

通用组件原则上保持：

```text
Ethernet Driver 的 Frame / ownership 逻辑
ethernetif
LwIP
Application 网络逻辑
```

板级迁移的具体步骤见 `docs/BOARD_PORTING.md`。

## 12. 实现状态约束

文档只把已经存在于当前代码或已经得到明确验证的能力写成“已实现”或“已验证”。

尚未实现的内容直接标记为“未实现”；成功编译不等同于上板验证，代码存在也不等同于功能已经正确。
