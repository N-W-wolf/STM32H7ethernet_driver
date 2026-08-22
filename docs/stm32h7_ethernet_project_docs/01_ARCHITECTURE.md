# STM32H7 Ethernet Architecture

本文描述 STM32H7 Ethernet Driver Package 的稳定分层、依赖方向和模块职责。当前验证环境为 STM32 HAL + FreeRTOS / CMSIS-RTOS2，验证硬件为 STM32H743VIT6 + LAN8720AI + RMII。

## 1. 总体分层

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

稳定依赖方向只允许自上而下。

核心约束：

- Application 不直接操作 `HAL_ETH_xxx()`；
- LwIP 不进入 Ethernet Driver Core；
- Ethernet Driver 不处理 IP、UDP、TCP、Socket 或机器人业务；
- PHY Driver 不依赖 LwIP / FreeRTOS；
- Driver Core 不依赖某个 Demo 的 `eth.h` / `heth`；
- Port 只绑定目标工程和板级差异；
- RTOS Adapter 不创建 Task，不决定 stack / priority；
- ISR 只处理必要 HAL 中断和轻量事件通知；
- 不为未出现的需求建立复杂抽象。

## 2. Driver Package

仓库需要复制到目标工程的通用代码统一位于：

```text
Ethernet/
├── Inc/
├── Src/
├── PHY/
├── Port/
└── RTOS/
```

当前具体结构：

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

当前 STM32H743 完整工程仍暂时位于仓库根目录。它是参考 Demo，待 Package 化回归验证完成后再整体迁入 `examples/`。

## 3. Ethernet Port

Port 是 Driver Package 与目标 CubeMX / HAL 工程之间的最薄绑定层。

通用接口：

```c
ETH_HandleTypeDef *EthernetPort_GetHandle(void);
void EthernetPort_PrepareDmaMemory(void);
void EthernetPort_PhyResetAssert(void);
void EthernetPort_PhyResetRelease(void);
```

职责：

- 返回目标工程实际 HAL ETH Handle；
- 准备 Ethernet DMA 所需板级 SRAM / clock；
- 控制 PHY Reset GPIO。

因此通用 `ethernet_driver.c` / `ethernet_mdio.c` 不需要 include 某个 Demo 的 `eth.h`，也不假定 HAL Handle 变量名一定为 `heth`。

当前 STM32H743 参考实现：

```text
BSP/stm32h743vit6_iot/ethernet_port.c
```

它知道当前 Demo 的 `eth.h`、`heth`、PC0 Reset 和 SRAM3 clock；这些内容不进入通用 Driver。

物理 SRAM 地址、MPU 和 linker 仍属于目标工程配置，而不是 Port API 参数。

## 4. MDIO Wrapper

接口：

```c
EthernetMdio_Read()
EthernetMdio_Write()
```

该层封装 STM32H7 HAL 1.11.6 的 PHY Management API，并通过 `EthernetPort_GetHandle()` 获取目标工程 ETH Handle。

PHY Driver 不直接依赖 `ETH_HandleTypeDef` 的具体实例名字。

## 5. PHY Driver

PHY Driver 负责：

- PHY ID；
- Reset 后可管理状态；
- Auto-negotiation；
- Link；
- Speed；
- Duplex；
- PHY 特定寄存器解析。

当前 LAN8720 接口：

```c
Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

约束：

- 只通过 MDIO Wrapper 访问 PHY；
- 不依赖 FreeRTOS；
- 不依赖 LwIP；
- Reset 等待、poll period 和 timeout 由调用层负责。

当前验证板的 Reset、MDIO、PHY ID、Address、Strap、Auto-negotiation、Link Up/Down、100 Mbit/s Full Duplex 已完成上板验证。

## 6. STM32H7 MAC / DMA Driver Core

职责：

- MAC Speed / Duplex 配置；
- RX/TX Descriptor 与 DMA Buffer ownership；
- Frame TX/RX；
- HAL RX Allocate / Link callback；
- HAL RX complete 事件转发；
- 后续 DMA / MAC error 处理。

当前接口：

```c
void EthernetDriver_Init(void);
void EthernetDriver_SetRxEventHandler(EthernetDriverRxEventHandler handler, void *context);
bool EthernetDriver_ConfigureLink(EthernetLinkSpeed speed, EthernetDuplexMode duplex);
bool EthernetDriver_Start(void);
bool EthernetDriver_Transmit(const uint8_t *frame, uint16_t length, uint32_t timeout_ms);
EthernetRxResult EthernetDriver_Receive(uint8_t *frame, uint16_t capacity, uint16_t *length);
```

当前配置：

```text
RX Descriptor = 4
TX Descriptor = 4
RX Buffer     = 4 × 1536 B
TX Buffer     = 4 × 1536 B
```

第一版继续采用 copy-based ownership。

RX：

```text
DMA RX Buffer
→ HAL_ETH_RxLinkCallback()
→ memcpy 到 Driver CPU 单帧暂存
→ 立即归还 RX DMA Buffer
→ EthernetDriver_Receive()
→ 调用者 Buffer
```

TX 当前仍为 polling：

```text
调用者 Frame
→ memcpy TX DMA Buffer
→ HAL_ETH_Transmit(timeout)
→ HAL_OK 后归还 TX Buffer
```

`EthernetDriver_Start()` 当前使用 `HAL_ETH_Start_IT()`，因此 RX 由 ETH IRQ 异步推进。异步 TX completion 尚未实现。

## 7. HAL RX complete 与通用事件

`HAL_ETH_RxCpltCallback()` 由 Driver Core 提供强定义。

它不读取 Frame，只把 ISR 事件转交给：

```c
EthernetDriverRxEventHandler
```

因此 Driver Core 不需要认识 FreeRTOS。

ISR event handler 必须保持短小，不得执行：

- `printf`；
- 协议解析；
- 大量数据复制；
- 阻塞；
- 应用业务。

## 8. CMSIS-RTOS2 Adapter

可选 Adapter：

```text
Ethernet/RTOS/CMSIS_RTOS2/
```

接口：

```c
void EthernetRtos_SetRxFrameHandler(EthernetRtosRxFrameHandler handler, void *context);
bool EthernetRtos_IsReady(void);
void EthernetRtos_RxTask(void *argument);
```

运行路径：

```text
ETH_IRQHandler()
→ HAL_ETH_IRQHandler()
→ HAL_ETH_RxCpltCallback()
→ Driver generic RX event
→ osThreadFlagsSet()
→ EthernetRtos_RxTask()
→ drain EthernetDriver_Receive() until ETHERNET_RX_NONE
→ RxFrameHandler()
```

Adapter 不创建 Task。应用 / CubeMX 负责 Task object、priority、stack 和 static/dynamic allocation。

RX Task 通过 `osThreadGetId()` 保存自身 Handle，因此 Adapter 不依赖 CubeMX 生成的 `EthernetRxTaskHandle` 变量名。

Frame Handler 在 RX Task 上下文同步执行。Frame pointer 只在 Handler 调用期间有效，不能在返回后继续持有。

异步 RX 在重构前路径上已完成连续 1000 / 1000 帧上板验证；Package 化后的新边界仍需重新 build / 上板回归。

## 9. ethernetif / LwIP

`ethernetif` 负责：

```text
LwIP pbuf
    ↕
ethernetif
    ↕
Ethernet Frame
```

它不承担 PHY Reset、板级 SRAM clock 或 MAC 寄存器配置。

当前 `ethernetif`、Static IPv4、Ping、UDP Echo、TCP Echo 尚未实现。

未来可以让 RTOS Adapter 的 Frame Handler 把完整 Frame 交给 `ethernetif`，但具体 pbuf / thread 边界要在 LwIP 工作单元中冻结。

## 10. DMA / Cache / MPU 边界

Ethernet DMA Descriptor 和 Buffer 必须显式放置，不能依赖默认 `.bss` 或普通 `static` 对象恰好位于 DMA 可达 RAM。

必须明确：

- DMA Master 可访问的 SRAM；
- Descriptor / Buffer 地址；
- Cache line alignment；
- MPU memory attribute；
- D-Cache Clean / Invalidate；
- CPU / DMA ownership；
- linker section；
- map / ELF 实际地址。

当前 STM32H743 示例使用 SRAM3 Non-cacheable 区域，详细见 `03_MEMORY_DMA.md`。

## 11. CubeMX 与手工维护边界

当前参考 Demo 中，CubeMX / ST 管理：

```text
stm32H7ethernet_demo.ioc
Core/**
Drivers/CMSIS/**
Drivers/STM32H7xx_HAL_Driver/**
cmake/stm32cubemx/CMakeLists.txt
CubeMX Third_Party middleware
```

`Core/**` 手工逻辑只进入 USER CODE 区域。

通用 Ethernet 手工源码统一位于：

```text
Ethernet/**
```

当前板级绑定实现暂时位于 Demo BSP。根 linker 中的 Ethernet DMA section 是当前参考板显式配置。

## 12. 可移植性

换一块 STM32H7 板时，原则上：

```text
复制 Ethernet/
+
配置目标 CubeMX ETH / MPU / NVIC / FreeRTOS
+
修改目标 linker
+
实现 ethernet_port.c
```

不应因为 PCB 不同而修改 Driver Core 的 Frame / Buffer ownership 逻辑。

完整用户接入流程以根 `README.md` 为准。
