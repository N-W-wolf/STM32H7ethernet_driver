# Ethernet DMA / MPU / Cache Design

本文记录 STM32H7 Ethernet DMA 使用的内存、Descriptor、Buffer、MPU、Cache 和 linker 约束。内容以当前代码、STM32H743 内存架构、构建结果和已完成的裸 Frame 上板验证为依据。

## 1. 设计目标

Ethernet DMA 数据路径必须满足：

- DMA Master 能够访问 Descriptor 和 Buffer 所在 SRAM；
- Descriptor / Buffer 地址由 linker 明确控制；
- 内存属性与 D-Cache 策略一致；
- Cache Line 对齐要求明确；
- CPU / DMA ownership 清晰；
- 实际地址能够通过 `.map` / ELF 验证；
- 板级内存地址不散落到通用 Ethernet Driver 中。

普通 `static` 数组的默认放置不能作为 Ethernet DMA 内存方案。

## 2. STM32H743 内存选择

STM32H743 Ethernet DMA 通过 D2 AHB 系统访问 SRAM。当前配置不使用 DTCM 作为 Ethernet DMA 内存。

当前验证板选择 SRAM3：

```text
SRAM3
Base : 0x30040000
Size : 32 KiB
End  : 0x30047FFF
```

理由：

- Ethernet DMA 可访问该区域；
- 可以与普通应用 RAM 分离；
- 32 KiB 可以容纳当前 Descriptor 和 RX/TX Buffer Pool；
- 32 KiB 适合配置为一个 MPU Region；
- 换板时可以通过板级 linker / MPU 替换物理 SRAM，而无需把地址写入通用 Driver。

## 3. Linker 内存划分

当前 `STM32H743xx_FLASH.ld` 将原有连续 D2 RAM 拆分为：

```ld
RAM_D2  (xrw) : ORIGIN = 0x30000000, LENGTH = 256K
RAM_ETH (xrw) : ORIGIN = 0x30040000, LENGTH = 32K
```

对应：

```text
0x30000000 ~ 0x3003FFFF  RAM_D2   256 KiB
0x30040000 ~ 0x30047FFF  RAM_ETH   32 KiB
```

这样普通 `RAM_D2` section 不会意外占用 SRAM3。

## 4. Descriptor 布局

当前 HAL 配置：

```text
ETH_RX_DESC_CNT = 4
ETH_TX_DESC_CNT = 4
```

当前 HAL 1.11.6 的 `ETH_DMADescTypeDef` 软件结构大小为 24 B，因此 4 个 Descriptor 实际占用：

```text
4 × 24 B = 96 B
```

地址布局：

| 对象 | 地址 | 实际大小 | 预留 slot |
| --- | --- | ---: | ---: |
| RX Descriptor | `0x30040000` | 96 B | 128 B |
| TX Descriptor | `0x30040080` | 96 B | 128 B |

GCC 下，CubeMX 在 `Core/Src/eth.c` 中使用：

```text
.RxDescripSection
.TxDescripSection
```

项目 linker 将两个 input section 固定到 SRAM3：

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

linker 同时检查：

- RX / TX section 地址必须匹配；
- 每组 Descriptor 不得超过 128 B slot；
- RX / TX section 不得为空。

这些检查可以在 CubeMX section 名变化、Descriptor 数量增加或布局被意外覆盖时尽早让链接失败。

## 5. RX/TX Buffer Pool

当前通用 Driver 使用：

```text
RX Buffer Count = ETH_RX_DESC_CNT = 4
TX Buffer Count = ETH_TX_DESC_CNT = 4
Buffer Size     = 1536 B
Alignment       = 32 B
```

通用 Driver 只定义 input section：

```text
.eth_dma_buffer.rx
.eth_dma_buffer.tx
```

物理地址由当前板 linker 决定：

| 对象 | 地址范围 | 大小 |
| --- | --- | ---: |
| RX Buffer Pool | `0x30042000 ~ 0x300437FF` | `0x1800` / 6144 B |
| TX Buffer Pool | `0x30044000 ~ 0x300457FF` | `0x1800` / 6144 B |

linker：

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

并固定检查：

```text
ADDR(.eth_dma_rx) = 0x30042000
SIZEOF(.eth_dma_rx) = 0x1800
ADDR(.eth_dma_tx) = 0x30044000
SIZEOF(.eth_dma_tx) = 0x1800
```

因此当前 RAM_ETH 的关键布局为：

```text
0x30040000  RX Descriptor
0x30040080  TX Descriptor
0x30042000  RX Buffer Pool, 4 × 1536 B
0x30044000  TX Buffer Pool, 4 × 1536 B
0x30045800  当前 TX Pool 结束后的首地址
0x30048000  RAM_ETH End + 1
```

通用 `Drivers/Ethernet` 中不写入这些物理 SRAM 地址。

## 6. Buffer ownership

### RX

`HAL_ETH_Start()` 建立 RX Descriptor 时，通过强符号 `HAL_ETH_RxAllocateCallback()` 从静态 RX Pool 分配 Buffer。

Polling 接收时：

```text
DMA owns RX Buffer
    ↓ Frame received
HAL_ETH_ReadData()
    ↓
HAL_ETH_RxLinkCallback()
    ↓ memcpy
CPU side g_rx_frame
    ↓
RX DMA Buffer immediately released to pool
    ↓
HAL ETH_UpdateDescriptor()
    ↓
Buffer can be allocated to an RX Descriptor again
```

`EthernetDriver_Receive()` 再把完整 `g_rx_frame` 复制到调用者提供的 Frame Buffer。上层不会长期持有 DMA RX Buffer。

当前这种 copy-based ownership 已完成单帧和连续 1000 帧上板验证。

### TX

当前 TX 使用 polling：

```text
Caller Frame
    ↓ memcpy
Acquire static TX DMA Buffer
    ↓
HAL_ETH_Transmit()
    ↓ HAL_OK
Release TX DMA Buffer
```

正常发送成功路径已经上板验证。

如果 `HAL_ETH_Transmit()` 返回错误，当前 Driver 不立即把该 TX Buffer 标记为空闲，因为 DMA 状态可能尚未完全确定。完整 DMA error recovery 和 Buffer 统一回收机制尚未实现。

异步 `HAL_ETH_Transmit_IT()` 的 completion ownership 不属于当前 polling 基线，后续需要结合 `HAL_ETH_ReleaseTxPacket()` / Tx free callback 单独设计。

## 7. MPU 配置

当前 `.ioc` 直接配置 Cortex-M7 MPU，不使用 Memory Management Tool 自动接管 Ethernet 内存。

### Region 1：整个 SRAM3

```text
Base          : 0x30040000
Size          : 32 KiB
TEX           : 1
Access        : Full Access
Execute       : Never
Shareable     : Yes
Cacheable     : No
Bufferable    : No
```

该组合将 SRAM3 作为 Normal Non-cacheable 内存。

### Region 2：Descriptor overlay

```text
Base          : 0x30040000
Size          : 256 B
TEX           : 0
Access        : Full Access
Execute       : Never
Shareable     : No
Cacheable     : No
Bufferable    : Yes
```

该组合使用 Device memory 属性覆盖 SRAM3 前 256 B。

因为 Region 2 编号高于 Region 1，最终属性为：

```text
0x30040000 ~ 0x300400FF
    Device / Non-cacheable

0x30040100 ~ 0x30047FFF
    Normal / Non-cacheable
```

Descriptor 位于前 256 B 内，RX/TX Buffer Pool 位于 Region 1 的 Normal Non-cacheable 区域。

## 8. D-Cache 策略

当前工程：

```text
CPU I-Cache : Disabled
CPU D-Cache : Disabled
```

Ethernet DMA 专用 SRAM3 已配置为 Non-cacheable，因此该区域的设计允许以后开启全局 D-Cache 时继续保持 CPU / DMA 一致性，不要求为该区域增加 `SCB_CleanDCache_by_Addr()` / `SCB_InvalidateDCache_by_Addr()`。

如果将来把 Ethernet Buffer 改到 Cacheable RAM，必须重新设计：

- Clean / Invalidate 时机；
- 32-byte Cache Line 对齐；
- 操作长度向 Cache Line 边界扩展；
- ownership 切换前后的内存屏障；
- 与 HAL Descriptor 生命周期的配合。

在 Cache 属性改变前，不在通用 Driver 中提前加入无实际需求的 Cache maintenance 抽象。

## 9. SRAM3 时钟

当前验证板在 `BSP/stm32h743vit6_iot/board_ethernet.c` 中提供：

```c
void BoardEthernet_PrepareDmaMemory(void);
```

实现显式使能：

```c
__HAL_RCC_D2SRAM3_CLK_ENABLE();
```

调用位置：

```text
Core/Src/main.c
USER CODE BEGIN SysInit
```

调用顺序：

```text
MPU_Config()
↓
HAL_Init()
↓
SystemClock_Config()
↓
BoardEthernet_PrepareDmaMemory()
↓
MX_GPIO_Init()
↓
MX_USART1_UART_Init()
↓
MX_ETH_Init()
↓
EthernetDriver_Init()
```

因此 SRAM3 在 Ethernet HAL 初始化接触 Descriptor 前已经准备完成，Driver software ownership 也在 MAC/DMA Start 前初始化。

## 10. CubeMX / MMT 管理边界

当前项目不使用 CubeMX Memory Management Tool 自动生成 Ethernet DMA linker section。

当前职责：

```text
.ioc
    → 保存 ETH Descriptor 地址
    → 保存 Cortex-M7 MPU Region

STM32H743xx_FLASH.ld
    → 保存当前板 RAM_ETH 物理布局
    → 固定 RX/TX Descriptor section
    → 固定 RX/TX Buffer Pool section
    → 提供 linker ASSERT

BSP
    → 准备当前板 DMA SRAM 时钟

Ethernet Driver
    → 定义通用 Buffer input section
    → 管理 Frame / Buffer ownership
```

重新使用 CubeMX Generate Code 后必须检查相关 diff，尤其是：

```text
stm32H7ethernet_demo.ioc
Core/Src/main.c
Core/Src/eth.c
STM32H743xx_FLASH.ld
```

`cmake/stm32cubemx/CMakeLists.txt` 仍由 CubeMX 管理，不手工修改。

## 11. 地址验证

构建后应检查 `.map` 或 ELF symbol，不能只根据 linker 源文件推断实际地址。

示例：

```bash
grep -E "RxDescripSection|TxDescripSection|eth_dma_rx|eth_dma_tx|DMARxDscrTab|DMATxDscrTab" \
  build/Debug/stm32H7ethernet_demo.map
```

或者：

```bash
arm-none-eabi-nm -n build/Debug/stm32H7ethernet_demo.elf | \
  grep -E "DMARxDscrTab|DMATxDscrTab"
```

当前已验证结果：

```text
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
.eth_dma_rx  = 0x30042000 / 0x1800
.eth_dma_tx  = 0x30044000 / 0x1800
```

Descriptor output section 实际大小均为 96 B，Buffer Pool output section 均为 6144 B。

## 12. 数据路径验证

当前已完成：

```text
Raw TX:
STM32H743 → LAN8720AI → PC
60 B / EtherType 0x88B5
PC tcpdump 实际抓包成功

Raw RX:
PC → LAN8720AI → STM32H743
60 B 单帧接收成功
连续 1000 / 1000 帧接收成功
PC 发送间隔约 5 ms
```

连续 1000 帧结果证明当前 4 RX Descriptor + 4 RX Buffer 的基础 recycle 路径能够反复工作，不代表吞吐极限、长时间稳定性或高负载 drop 已经验证。

## 13. 可移植性要求

迁移到另一块板时，需要重新确认：

- MCU 的 Ethernet DMA 可达内存；
- 选用 SRAM 的容量与总线可达性；
- SRAM 时钟是否需要显式使能；
- MPU Region 的 Base / Size / 属性；
- linker MEMORY region；
- Descriptor section 地址；
- Buffer section 地址；
- map / ELF 实际地址。

通用 Ethernet Driver 中不应出现：

```text
0x30040000
SRAM3
RAM_D2
RAM_ETH
```

这些属于当前板级内存配置。

完整迁移步骤见 `docs/BOARD_PORTING.md`。

## 14. 自动化原则

当前不使用脚本通过正则或字符串替换自动修改 linker script。

板级配置保持显式、可审查；自动化优先用于验证，例如检查 `.map` / ELF 中 Descriptor、Buffer、对齐和区域边界。

如果项目出现多块板并需要统一生成多个 linker，可以再引入结构化板级配置和模板生成，但不以修改 CubeMX 生成文本为主要机制。
