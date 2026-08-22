# STM32H7 Ethernet Architecture

本文描述 STM32H7 Ethernet Driver Package 的稳定分层、依赖方向和模块职责。当前验证硬件为 STM32H743VIT6 + LAN8720AI + RMII；Reference Example 位于 `examples/STM32H743_LAN8720_FreeRTOS/`。

运行时 callback、weak symbol、IRQ/Task 交接和 RX Buffer ownership 的详细原理说明见：[`docs/ETHERNET_RUNTIME_FLOW.md`](../ETHERNET_RUNTIME_FLOW.md)。

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

只允许稳定的自上而下依赖。Application 不直接操作 HAL ETH；Driver 不处理 IP/UDP/TCP/机器人业务；PHY 不依赖 RTOS/LwIP；Port 不处理 Frame 或协议。

## 2. Driver Package

根目录：

```text
Ethernet/
├── Inc/
├── Src/
├── PHY/
├── Port/
└── RTOS/
```

这是用户迁移到另一工程时复制的产品目录。STM32CubeMX 生成代码、HAL、FreeRTOS、linker、CMake、BSP Example 都不放入 Package。

## 3. Ethernet Port

Port 解决通用 Driver 与目标工程之间必须存在的板级绑定：

```c
ETH_HandleTypeDef *EthernetPort_GetHandle(void);
void EthernetPort_PrepareDmaMemory(void);
void EthernetPort_PhyResetAssert(void);
void EthernetPort_PhyResetRelease(void);
```

通用 Driver 不 include Demo `eth.h`，也不假设 Handle 名为 `heth`。当前参考板实现位于：

```text
examples/STM32H743_LAN8720_FreeRTOS/BSP/stm32h743vit6_iot/ethernet_port.c
```

当前实现绑定 CubeMX `heth`、PC0 PHY reset 和 D2 SRAM3 clock。

## 4. MDIO / PHY

MDIO Wrapper 封装当前 STM32H7 HAL 1.11.6 PHY Management API：

```text
EthernetMdio_Read()
EthernetMdio_Write()
```

LAN8720 Driver：

```text
Lan8720_IsReady()
Lan8720_RestartAutoNegotiation()
Lan8720_GetStatus()
```

PHY Driver 只通过 MDIO Wrapper 访问 PHY，不依赖 RTOS/LwIP；Reset 后等待、polling 和 timeout 由调用层管理。

## 5. MAC / DMA Driver

当前 Frame API：

```c
void EthernetDriver_Init(void);
bool EthernetDriver_ConfigureLink(EthernetLinkSpeed speed, EthernetDuplexMode duplex);
bool EthernetDriver_Start(void);
bool EthernetDriver_Transmit(const uint8_t *frame, uint16_t length, uint32_t timeout_ms);
EthernetRxResult EthernetDriver_Receive(uint8_t *frame, uint16_t capacity, uint16_t *length);
```

另外提供轻量 RX event 注册：

```text
EthernetDriver_SetRxEventHandler()
```

HAL RX complete callback 由 Driver Core 接收，只向上转发 ISR event，不在 ISR 中读取 Frame。

当前使用 4 RX + 4 TX Descriptor、4×1536 B RX Pool、4×1536 B TX Pool。第一版 copy-based ownership：

```text
RX DMA Buffer
→ HAL_ETH_RxLinkCallback()
→ copy 到 CPU 单帧暂存
→ 立即归还 RX Pool
→ EthernetDriver_Receive() copy 给调用者
```

TX 当前仍为 polling：

```text
caller frame
→ copy 到 TX DMA Buffer
→ HAL_ETH_Transmit(timeout)
→ HAL_OK 后归还 Buffer
```

异步 TX completion 尚未实现。

## 6. CMSIS-RTOS2 Adapter

可选目录：

```text
Ethernet/RTOS/CMSIS_RTOS2/
```

Adapter 不创建 Task。应用/CubeMX 管理 Task object、priority、stack、allocation；Package 提供：

```text
EthernetRtos_SetRxFrameHandler()
EthernetRtos_IsReady()
EthernetRtos_RxTask()
```

运行链路：

```text
ETH IRQ
→ HAL_ETH_IRQHandler()
→ HAL_ETH_RxCpltCallback()
→ Driver RX event
→ osThreadFlagsSet()
→ EthernetRtos_RxTask()
→ drain EthernetDriver_Receive() until ETHERNET_RX_NONE
→ synchronous Frame Handler
```

Frame Handler 在任务上下文执行，frame pointer 仅在 Handler 调用期间有效。

当前 Reference Example 已采用并验证：

```text
Task Entry : EthernetRtos_RxTask
Generation : As weak
```

CubeMX 管理 Task attributes / `osThreadNew()`，Package 提供同名强定义 Task Entry；Generate Code、Build 和 On-board async RX 回归均已通过。

## 7. ethernetif / LwIP

未来 `ethernetif` 位于 LwIP 与 Frame API 之间：

```text
LwIP pbuf
↕
ethernetif
↕
Ethernet Frame API / RTOS runtime
```

当前 ethernetif / LwIP 尚未实现。Driver 不提前包含 pbuf、IP、Socket 或协议语义。

## 8. DMA / MPU / linker 边界

物理 DMA 地址属于目标工程，不属于 Driver Package。必须显式确定：

- DMA Master 可达 SRAM；
- Descriptor / Buffer 地址；
- 32-byte alignment；
- MPU Memory Attribute；
- D-Cache 策略；
- ownership；
- linker section；
- map/ELF 实际地址。

当前 STM32H743 Example 使用 SRAM3 `0x30040000 / 32 KiB`，Non-cacheable，前 256 B Device overlay。详细见 `03_MEMORY_DMA.md`。

## 9. CubeMX 与维护边界

Reference Example 的 CubeMX/ST 管理内容：

```text
examples/STM32H743_LAN8720_FreeRTOS/stm32H7ethernet_demo.ioc
examples/STM32H743_LAN8720_FreeRTOS/Core/**
examples/STM32H743_LAN8720_FreeRTOS/Drivers/**
examples/STM32H743_LAN8720_FreeRTOS/Middlewares/**
examples/STM32H743_LAN8720_FreeRTOS/cmake/stm32cubemx/CMakeLists.txt
```

Core 手工代码只进入 USER CODE；`cmake/stm32cubemx/CMakeLists.txt` 不手工修改。

手工维护产品：`Ethernet/**`。板级 linker 和 Example CMake 属于 Reference Example 配置。

## 10. 可移植性

换板时优先只改变：

```text
目标 CubeMX 配置
目标 ethernet_port.c
板级 linker / MPU / DMA SRAM
PHY Driver（仅 PHY 型号变化时）
Task / RTOS 资源配置
```

不应因为 PCB 差异修改通用 Frame ownership、MDIO Wrapper、RTOS Adapter 或未来 ethernetif。

## 11. 当前验证

已 On-board Verified：PHY bring-up、Raw TX/RX、polling RX 1000/1000、ETH IRQ + CMSIS-RTOS2 async RX 1000/1000。

Driver Package 化、Reference Example 移入 `examples/`、CubeMX `As weak` Task Entry 方案均已完成 Build / map / On-board 回归。