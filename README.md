# STM32H7 Ethernet Driver

面向 STM32H7 的可复用 Ethernet 驱动包，当前基于 STM32 HAL，提供 STM32H7 MAC/DMA、MDIO、PHY、板级 Port 以及可选的 CMSIS-RTOS2 接收适配层。

当前验证平台为：

```text
MCU       : STM32H743VIT6
PHY       : LAN8720AI
Interface : RMII
RTOS      : FreeRTOS / CMSIS-RTOS2
```

仓库的产品是根目录下的 [`Ethernet/`](Ethernet/) 驱动包；仓库中的 STM32CubeMX 工程、linker、CMake 和 BSP 是当前 STM32H743 + LAN8720 验证工程，用于展示一套完整接入方式，不是驱动包本身的固定依赖。

## 1. 驱动包架构

```text
Application / LwIP
        ↓
RTOS Adapter / ethernetif
        ↓
Ethernet Driver
    ├── STM32H7 MAC / DMA
    └── MDIO
        ↓
PHY Driver
        ↓
Ethernet Port
        ↓
STM32 HAL / Board
```

各层职责：

- `Ethernet Driver`：MAC/DMA、Descriptor、静态 RX/TX DMA Buffer、Frame API 和 HAL RX callback；
- `MDIO`：Clause 22 PHY 寄存器访问；
- `PHY`：具体 PHY 行为，当前提供 LAN8720；
- `Port`：目标工程提供的板级绑定，只暴露 HAL ETH Handle、PHY Reset 和 DMA SRAM 准备；
- `RTOS/CMSIS_RTOS2`：可选适配层，负责 RX IRQ 事件转为 Thread Flag，并在任务上下文读取完整 Frame；
- LwIP、UDP/TCP 和应用业务不进入底层 Driver。

驱动包目录：

```text
Ethernet/
├── Inc/
│   ├── ethernet_driver.h
│   └── ethernet_mdio.h
├── Src/
│   ├── ethernet_driver.c
│   └── ethernet_mdio.c
├── PHY/
│   └── LAN8720/
│       ├── Inc/lan8720.h
│       └── Src/lan8720.c
├── Port/
│   └── Inc/ethernet_port.h
└── RTOS/
    └── CMSIS_RTOS2/
        ├── Inc/ethernet_rtos.h
        └── Src/ethernet_rtos.c
```

迁移到另一块 STM32H7 板时，应优先复制整个 `Ethernet/` 目录，然后在目标工程中完成 CubeMX、linker 和 `ethernet_port.c` 配置。

## 2. 接入流程总览

推荐按以下顺序接入：

```text
1. 复制 Ethernet/
        ↓
2. CubeMX 配置 ETH / GPIO / MPU / NVIC / FreeRTOS
        ↓
3. 选择 Ethernet DMA 可访问 SRAM
        ↓
4. 修改 linker，固定 Descriptor 与 RX/TX Buffer section
        ↓
5. 实现 ethernet_port.c
        ↓
6. 把 Ethernet 源码加入自己的构建系统
        ↓
7. 初始化 HAL ETH 与 Ethernet Driver
        ↓
8. PHY Reset / MDIO / Auto-negotiation
        ↓
9. 配置 MAC Speed / Duplex 并启动
        ↓
10. 接入 RTOS / LwIP
```

STM32H7 Ethernet 的关键正确性问题主要集中在 DMA SRAM、Descriptor、Buffer、MPU、Cache 和 ownership。不要在未确认 DMA 可达性的情况下把普通 `static` 数组直接当作 Ethernet DMA Buffer。

## 3. CubeMX 配置

### 3.1 ETH 外设

在 CubeMX 中启用：

```text
Connectivity
→ ETH
→ Media Interface: RMII 或 MII
```

RMII 典型信号包括：

```text
REF_CLK
MDC
MDIO
CRS_DV
RXD0
RXD1
TX_EN
TXD0
TXD1
```

引脚必须根据目标 PCB / 原理图配置，不能照抄当前示例板。

当前验证工程使用 RMII，LAN8720AI 向 STM32 输出 50 MHz `RMII_REF_CLK`。

### 3.2 Descriptor 和 RX Buffer Length

当前示例配置：

```text
ETH_RX_DESC_CNT = 4
ETH_TX_DESC_CNT = 4
RxBuffLen       = 1536 B
RxDescAddress   = 0x30040000
TxDescAddress   = 0x30040080
```

CubeMX 生成的 `DMARxDscrTab[]` / `DMATxDscrTab[]` 通过 `.RxDescripSection` / `.TxDescripSection` 交给 linker 放置。

当前驱动自己的 RX/TX payload Buffer 并不依赖普通 `.bss` 放置，而是使用：

```text
.eth_dma_buffer.rx
.eth_dma_buffer.tx
```

再由目标工程 linker 决定物理地址。

### 3.3 MPU

如果使用 Cortex-M7 MPU 管理 Ethernet DMA 内存，需要根据目标 MCU 实际 SRAM 和 Cache 策略配置。

当前 STM32H743 示例：

```text
Region 1
Base        : 0x30040000
Size        : 32 KiB
Type        : Normal
Cacheable   : No
Bufferable  : No
Shareable   : Yes
Execute     : No

Region 2
Base        : 0x30040000
Size        : 256 B
Type        : Device
Cacheable   : No
Bufferable  : Yes
Shareable   : No
Execute     : No
```

Region 2 使用更高 Region Number 覆盖 Descriptor 区。

当前示例 I-Cache / D-Cache 均关闭。如果目标工程启用 D-Cache，必须重新设计并验证 DMA Buffer 的 Clean / Invalidate，不能直接沿用当前 Non-cacheable 假设。

### 3.4 ETH IRQ

异步 RX 需要开启 Ethernet global interrupt。

当前示例：

```text
ETH_IRQn Preemption Priority = 5
Sub Priority                 = 0
```

这个数值不是驱动包的固定要求。若 ETH ISR 会调用 FreeRTOS / CMSIS-RTOS2 的 ISR-safe API，则必须满足目标工程自己的 FreeRTOS 中断优先级约束。

当前示例：

```text
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
ETH_IRQn                                      = 5
```

即 ETH IRQ 的 library priority 数值不得高于 FreeRTOS 允许调用系统 API 的边界。

### 3.5 FreeRTOS Task

驱动包不创建 Task，也不决定 Task 的 stack、priority 或静态/动态分配方式。

应用工程负责创建 RX Task，任务实际逻辑由：

```c
EthernetRtos_RxTask(void *argument);
```

提供。

当前 CubeMX 示例仍由 CubeMX 创建 `EthernetRxTask` 对象，生成的任务入口只做示例 Frame Handler 注册，然后转入 `EthernetRtos_RxTask()`。因此 Task 资源仍然由工程本身显式管理。

## 4. Ethernet DMA 内存设计

### 4.1 为什么不能随便使用普通 static 数组

在 STM32H7 上，不同 SRAM 位于不同总线 / Domain，Ethernet DMA Master 并不能访问所有 CPU 可访问内存。

例如普通：

```c
static uint8_t rx_buffer[1536];
```

最终位置由 linker 决定。如果它落入 Ethernet DMA 不可访问的 RAM，CPU 访问正常并不代表 DMA 可以访问。

因此接入驱动时必须同时明确：

```text
目标 SRAM 物理地址
Ethernet DMA 可达性
SRAM clock
Descriptor 地址
RX/TX Buffer 地址
Cache line alignment
MPU 属性
D-Cache 策略
CPU / DMA ownership
linker section
map / ELF 实际结果
```

### 4.2 当前 STM32H743 示例内存

当前验证板将 STM32H743 SRAM3 单独划给 Ethernet：

```text
RAM_ETH
0x30040000 ~ 0x30047FFF
32 KiB
```

普通 D2 RAM 缩为：

```text
RAM_D2
0x30000000 ~ 0x3003FFFF
256 KiB
```

当前布局：

```text
0x30040000  RX Descriptor，4 × 24 B，实际 96 B
0x30040080  TX Descriptor，4 × 24 B，实际 96 B
0x30042000  RX Pool，4 × 1536 B = 0x1800
0x30044000  TX Pool，4 × 1536 B = 0x1800
```

这些地址是 STM32H743 当前示例，不是驱动包固定地址。迁移到其他 STM32H7 时必须重新查对应 Reference Manual。

## 5. Linker 配置

驱动包定义 section 名，目标工程 linker 决定物理地址。

### 5.1 MEMORY

当前 STM32H743 示例：

```ld
MEMORY
{
  DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  RAM     (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
  RAM_D2  (xrw) : ORIGIN = 0x30000000, LENGTH = 256K
  RAM_ETH (xrw) : ORIGIN = 0x30040000, LENGTH = 32K
  RAM_D3  (xrw) : ORIGIN = 0x38000000, LENGTH = 64K
  ITCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
}
```

关键点是避免 `RAM_D2` 和 `RAM_ETH` 地址重叠。

### 5.2 Descriptor section

```ld
.RxDescripSection 0x30040000 (NOLOAD) :
{
  . = ALIGN(32);
  KEEP(*(.RxDescripSection))
  . = ALIGN(32);
} >RAM_ETH

.TxDescripSection 0x30040080 (NOLOAD) :
{
  . = ALIGN(32);
  KEEP(*(.TxDescripSection))
  . = ALIGN(32);
} >RAM_ETH
```

### 5.3 RX / TX payload Buffer

```ld
.eth_dma_rx 0x30042000 (NOLOAD) :
{
  . = ALIGN(32);
  KEEP(*(.eth_dma_buffer.rx))
  . = ALIGN(32);
} >RAM_ETH

.eth_dma_tx 0x30044000 (NOLOAD) :
{
  . = ALIGN(32);
  KEEP(*(.eth_dma_buffer.tx))
  . = ALIGN(32);
} >RAM_ETH
```

当前 Driver 的 Pool 为 4 × 1536 B，因此每个 Pool 应为：

```text
4 × 1536 = 6144 B = 0x1800
```

### 5.4 建议增加 linker ASSERT

当前示例使用：

```ld
ASSERT(ADDR(.RxDescripSection) == 0x30040000,
       "Ethernet RX descriptor address mismatch")
ASSERT(SIZEOF(.RxDescripSection) <= 0x80,
       "Ethernet RX descriptors exceed reserved slot")

ASSERT(ADDR(.TxDescripSection) == 0x30040080,
       "Ethernet TX descriptor address mismatch")
ASSERT(SIZEOF(.TxDescripSection) <= 0x80,
       "Ethernet TX descriptors exceed reserved slot")

ASSERT(ADDR(.eth_dma_rx) == 0x30042000,
       "Ethernet RX buffer address mismatch")
ASSERT(SIZEOF(.eth_dma_rx) == 0x1800,
       "Ethernet RX buffer pool size mismatch")

ASSERT(ADDR(.eth_dma_tx) == 0x30044000,
       "Ethernet TX buffer address mismatch")
ASSERT(SIZEOF(.eth_dma_tx) == 0x1800,
       "Ethernet TX buffer pool size mismatch")
```

修改 Descriptor 数量或 Buffer 大小时，必须同步修改 linker 预留和断言。

### 5.5 构建后必须检查 map / ELF

不要只因为“成功编译”就认为 DMA 布局正确。

当前 GNU 工具链示例：

```bash
grep -E "RxDescripSection|TxDescripSection|eth_dma_rx|eth_dma_tx|DMARxDscrTab|DMATxDscrTab" \
  build/Debug/stm32H7ethernet_demo.map

arm-none-eabi-nm -n build/Debug/stm32H7ethernet_demo.elf | \
  grep -E "DMARxDscrTab|DMATxDscrTab"
```

应确认实际地址和目标设计一致，并且没有越出 DMA SRAM。

## 6. 实现 Ethernet Port

`Ethernet/Port/Inc/ethernet_port.h` 是 Driver 与目标 CubeMX 工程之间唯一的板级绑定接口：

```c
ETH_HandleTypeDef *EthernetPort_GetHandle(void);
void EthernetPort_PrepareDmaMemory(void);
void EthernetPort_PhyResetAssert(void);
void EthernetPort_PhyResetRelease(void);
```

当前 STM32H743 示例实现位于：

```text
BSP/stm32h743vit6_iot/ethernet_port.c
```

核心形式如下：

```c
#include "ethernet_port.h"

#include "eth.h"
#include "main.h"

ETH_HandleTypeDef *EthernetPort_GetHandle(void)
{
    return &heth;
}

void EthernetPort_PrepareDmaMemory(void)
{
    __HAL_RCC_D2SRAM3_CLK_ENABLE();
}

void EthernetPort_PhyResetAssert(void)
{
    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port, ETH_RESET_Pin, GPIO_PIN_RESET);
}

void EthernetPort_PhyResetRelease(void)
{
    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port, ETH_RESET_Pin, GPIO_PIN_SET);
}
```

换板时只修改这个 Port 实现和目标工程配置，不应为了 PCB 差异修改通用 `ethernet_driver.c`。

## 7. 初始化顺序

当前推荐顺序：

```text
MPU_Config()
↓
HAL_Init()
↓
SystemClock_Config()
↓
EthernetPort_PrepareDmaMemory()
↓
MX_GPIO_Init()
↓
MX_ETH_Init()
↓
EthernetDriver_Init()
↓
创建 RTOS Task / 启动 scheduler
↓
PHY Reset Release
↓
PHY Ready / Auto-negotiation
↓
读取 Speed / Duplex
↓
EthernetDriver_ConfigureLink()
↓
EthernetDriver_Start()
```

如果使用 `EthernetRtos_RxTask()`，在 `EthernetDriver_Start()` 前应确认：

```c
EthernetRtos_IsReady() == true
```

避免 MAC/DMA 已经开始接收，但 RX Task 还没有完成 ISR notification 绑定。

## 8. Driver API

### MAC / DMA

```c
void EthernetDriver_Init(void);
void EthernetDriver_SetRxEventHandler(EthernetDriverRxEventHandler handler, void *context);
bool EthernetDriver_ConfigureLink(EthernetLinkSpeed speed, EthernetDuplexMode duplex);
bool EthernetDriver_Start(void);
bool EthernetDriver_Transmit(const uint8_t *frame, uint16_t length, uint32_t timeout_ms);
EthernetRxResult EthernetDriver_Receive(uint8_t *frame, uint16_t capacity, uint16_t *length);
```

当前 TX 仍为 polling `HAL_ETH_Transmit()`；RX 由 `HAL_ETH_Start_IT()` + ETH IRQ 异步推进，在任务上下文调用 `EthernetDriver_Receive()`。

### MDIO

```c
bool EthernetMdio_Read(uint32_t phy_address, uint32_t register_address, uint32_t *value);
bool EthernetMdio_Write(uint32_t phy_address, uint32_t register_address, uint32_t value);
```

### LAN8720

```c
bool Lan8720_IsReady(uint32_t phy_address);
bool Lan8720_RestartAutoNegotiation(uint32_t phy_address);
bool Lan8720_GetStatus(uint32_t phy_address, Lan8720Status *status);
```

## 9. CMSIS-RTOS2 异步 RX

可选适配层：

```text
Ethernet/RTOS/CMSIS_RTOS2/
```

运行路径：

```text
ETH IRQ
↓
HAL_ETH_IRQHandler()
↓
HAL_ETH_RxCpltCallback()
↓
Ethernet Driver RX event
↓
osThreadFlagsSet()
↓
EthernetRtos_RxTask()
↓
反复 EthernetDriver_Receive()
直到 ETHERNET_RX_NONE
↓
用户 Frame Handler
```

RTOS Adapter 不创建 Task。

完整 Frame 通过同步 handler 在 RX Task 上下文交给上层：

```c
typedef void (*EthernetRtosRxFrameHandler)(
    const uint8_t *frame,
    uint16_t length,
    void *context);
```

注册：

```c
EthernetRtos_SetRxFrameHandler(MyRxHandler, NULL);
```

`frame` 指针只在 handler 调用期间有效，handler 返回后不得继续持有；需要长期保存时由上层自行复制。

ISR 中禁止执行：

```text
printf
协议解析
大量 memcpy
网络业务
阻塞等待
动态内存分配
```

## 10. 构建系统

`Ethernet/` 是普通 C 源码，不要求用户使用本仓库的 CMake。

至少需要把以下源文件加入目标工程：

```text
Ethernet/Src/ethernet_driver.c
Ethernet/Src/ethernet_mdio.c
需要的 PHY Driver
目标板 ethernet_port.c
```

如果使用 CMSIS-RTOS2 Adapter，再加入：

```text
Ethernet/RTOS/CMSIS_RTOS2/Src/ethernet_rtos.c
```

Include path：

```text
Ethernet/Inc
Ethernet/Port/Inc
对应 PHY/Inc
如使用 RTOS：Ethernet/RTOS/CMSIS_RTOS2/Inc
```

目标工程还需要自己的 STM32H7 HAL、CMSIS 和 CubeMX/手工生成的 ETH 初始化代码。

当前仓库参考工程使用 CMake + GNU Arm Embedded Toolchain，仅作为示例；使用 STM32CubeIDE、Keil、IAR 或其他构建系统时，只需按上述 source/include 关系加入驱动。

## 11. 当前验证状态

| 能力 | 状态 |
| --- | --- |
| LAN8720 Reset / MDIO / PHY ID | 已上板验证 |
| Auto-negotiation / Link / Speed / Duplex | 100M Full 已上板验证 |
| DMA Descriptor / RX/TX Pool linker 布局 | Build / map 验证通过 |
| 裸 Ethernet TX | 已上板验证 |
| Polling RX | 单帧、连续 1000 帧已上板验证 |
| ETH IRQ + CMSIS-RTOS2 异步 RX | 连续 1000 / 1000 帧已上板验证 |
| 异步 TX completion | 未实现 |
| DMA error / timeout recovery | 未实现 |
| 完整 Link Down / Up MAC lifecycle | 未实现 |
| LwIP `ethernetif` | 未实现 |
| Ping / UDP / TCP | 未实现 |
| D-Cache 开启后的数据路径 | 未验证 |

连续 1000 帧测试约为 5 ms / Frame，用于验证 IRQ、Task Notification 和 Buffer recycle，不代表 100 Mbit/s 高负载压力测试通过。

## 12. 当前参考工程

仓库根目录当前同时保留 STM32H743VIT6 + LAN8720AI 的完整参考工程，包括：

```text
stm32H7ethernet_demo.ioc
Core/
Drivers/CMSIS/
Drivers/STM32H7xx_HAL_Driver/
Middlewares/
BSP/stm32h743vit6_iot/
STM32H743xx_FLASH.ld
CMakeLists.txt
build.sh
flash.sh
```

参考工程使用：

```text
CubeMX       : 6.18.1
STM32CubeH7  : V1.13.0
HAL ETH      : 1.11.6
```

本仓库的 `build.sh` / `flash.sh` 只服务于该参考工程，不属于 Driver API。

更深入的架构、内存设计和项目验证记录见 `docs/`。
