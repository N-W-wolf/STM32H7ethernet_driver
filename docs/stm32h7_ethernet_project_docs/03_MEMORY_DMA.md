# Ethernet DMA / MPU / Cache Design

本文记录 STM32H7 Ethernet Driver 对 DMA 内存、Descriptor、Buffer、MPU、Cache 和 linker 的约束，并以当前 STM32H743 参考 Demo 的已验证布局作为示例。

## 1. 核心原则

Ethernet DMA 数据路径必须满足：

- DMA Master 能访问 Descriptor / Buffer 所在 SRAM；
- Descriptor / Buffer 地址由 linker 明确控制；
- 内存属性与 Cache 策略一致；
- Cache Line 对齐明确；
- CPU / DMA ownership 清晰；
- `.map` / ELF 能验证最终地址；
- 当前板的物理地址不进入通用 Driver Core。

普通 `static` 对象的默认放置不能作为 DMA 内存设计依据。

## 2. Driver Package 与目标工程的边界

通用代码：

```text
Ethernet/Src/ethernet_driver.c
```

只定义 DMA payload input section：

```text
.eth_dma_buffer.rx
.eth_dma_buffer.tx
```

并使用 `ETH_RX_DESC_CNT` / `ETH_TX_DESC_CNT` 和 1536 B Buffer 管理 ownership。

目标工程负责：

```text
CubeMX Descriptor section
linker MEMORY
Descriptor output section
RX/TX Buffer output section
MPU
Cache policy
DMA SRAM clock
map / ELF verification
```

当前板级 SRAM clock 通过：

```text
BSP/stm32h743vit6_iot/ethernet_port.c
→ EthernetPort_PrepareDmaMemory()
```

处理。

## 3. STM32H743 当前 SRAM 选择

当前参考 Demo 选择 SRAM3：

```text
SRAM3 / RAM_ETH
Base : 0x30040000
Size : 32 KiB
End  : 0x30047FFF
```

普通 D2 RAM 缩为：

```text
RAM_D2
0x30000000 ~ 0x3003FFFF
256 KiB
```

目的：避免普通 section 意外进入 SRAM3，并给 Ethernet DMA 留出显式空间。

其他 STM32H7 型号必须重新核对 Reference Manual 中 Ethernet DMA 的总线可达性，不得照抄该地址。

## 4. Descriptor

当前 HAL：

```text
ETH_RX_DESC_CNT = 4
ETH_TX_DESC_CNT = 4
sizeof(ETH_DMADescTypeDef) = 24 B
```

每组实际：

```text
4 × 24 = 96 B
```

布局：

| 对象 | 地址 | 实际大小 | 预留 slot |
| --- | --- | ---: | ---: |
| RX Descriptor | `0x30040000` | 96 B | 128 B |
| TX Descriptor | `0x30040080` | 96 B | 128 B |

CubeMX GCC 代码定义 input section：

```text
.RxDescripSection
.TxDescripSection
```

当前 linker：

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

## 5. RX / TX Buffer Pool

当前 Driver：

```text
RX Buffer Count = 4
TX Buffer Count = 4
Buffer Size     = 1536 B
Alignment       = 32 B
```

每个 Pool：

```text
4 × 1536 = 6144 B = 0x1800
```

当前参考布局：

| 对象 | 地址范围 | 大小 |
| --- | --- | ---: |
| RX Pool | `0x30042000 ~ 0x300437FF` | `0x1800` |
| TX Pool | `0x30044000 ~ 0x300457FF` | `0x1800` |

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

## 6. Linker ASSERT

当前参考 Demo 通过 ASSERT 把“代码能编译”升级为“内存布局至少在链接阶段可验证”。

关键检查：

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

修改 Descriptor 数量或 Buffer 大小时必须同步更新 linker 预留和断言。

## 7. RX ownership

当前 copy-based RX：

```text
DMA owns RX Buffer
→ Frame received / ETH IRQ
→ RX Task 调用 HAL_ETH_ReadData()
→ HAL_ETH_RxLinkCallback()
→ memcpy 到 Driver CPU 单帧暂存
→ 立即释放 DMA RX Buffer
→ HAL ETH_UpdateDescriptor()
→ Descriptor 可重新获得 Buffer
→ EthernetDriver_Receive() 再复制给调用者
```

上层不会持有 DMA RX Buffer。

该 ownership 在 polling 路径上已完成单帧和连续 1000 帧验证；重构前的 ETH IRQ + CMSIS-RTOS2 路径也完成 1000 / 1000 验证。

## 8. TX ownership

当前仍为 polling：

```text
Caller Frame
→ acquire TX DMA Buffer
→ memcpy
→ HAL_ETH_Transmit(timeout)
→ HAL_OK
→ release TX Buffer
```

若 HAL 返回错误，当前 Driver 不立即复用该 Buffer，因为 DMA ownership 可能尚未完全明确。

`HAL_ETH_Transmit_IT()`、`HAL_ETH_ReleaseTxPacket()` 和 Tx free callback 的最终 ownership 尚未实现。

## 9. MPU

当前 `.ioc` 直接配置 Cortex-M7 MPU，不使用 CubeMX Memory Management Tool 接管 Ethernet linker section。

Region 1：

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

Region 2：

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

Region 2 编号更高，因此前 256 B Descriptor 区使用 Device 属性，其余 SRAM3 使用 Normal Non-cacheable。

## 10. D-Cache

当前：

```text
I-Cache = Disabled
D-Cache = Disabled
```

SRAM3 本身又由 MPU 配为 Non-cacheable。

如果未来把 Buffer 放到 Cacheable RAM，必须重新设计并实测：

- Clean / Invalidate 时机；
- 32-byte Cache Line 对齐；
- 操作范围向 Cache Line 边界扩展；
- ownership 切换前后的 memory barrier；
- Descriptor 与 payload 各自的属性。

在 Cache 策略真正变化前，不向通用 Driver 提前加入无验证的 Cache maintenance 抽象。

## 11. SRAM3 clock 与初始化顺序

当前 Port：

```c
void EthernetPort_PrepareDmaMemory(void)
{
    __HAL_RCC_D2SRAM3_CLK_ENABLE();
}
```

调用：

```text
MPU_Config()
→ HAL_Init()
→ SystemClock_Config()
→ EthernetPort_PrepareDmaMemory()
→ MX_GPIO_Init()
→ MX_ETH_Init()
→ EthernetDriver_Init()
```

因此 HAL 接触 Descriptor 前 SRAM3 已准备完成。

## 12. CubeMX / MMT 边界

当前职责：

```text
.ioc
→ ETH 外设 / Descriptor 地址
→ MPU
→ NVIC / FreeRTOS

STM32H743xx_FLASH.ld
→ RAM_ETH
→ Descriptor output section
→ RX/TX Buffer output section
→ ASSERT

Ethernet Port
→ 当前板 DMA SRAM clock / PHY Reset / HAL Handle

Ethernet Driver
→ input section
→ Frame / Buffer ownership
```

当前不使用 MMT 自动生成 Ethernet DMA linker section。

## 13. Build / map 验证

GNU 示例：

```bash
grep -E "RxDescripSection|TxDescripSection|eth_dma_rx|eth_dma_tx|DMARxDscrTab|DMATxDscrTab" \
  build/Debug/stm32H7ethernet_demo.map

arm-none-eabi-nm -n build/Debug/stm32H7ethernet_demo.elf | \
  grep -E "DMARxDscrTab|DMATxDscrTab"
```

历史已验证结果：

```text
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
.eth_dma_rx  = 0x30042000 / 0x1800
.eth_dma_tx  = 0x30044000 / 0x1800
```

Package 化后的新目录结构必须重新执行 fresh build / map 验证，不能直接继承旧构建结果。

## 14. 移植检查表

换板时至少重新确认：

- MCU 是否含 Ethernet MAC；
- DMA Master 可访问的 SRAM；
- SRAM clock；
- linker MEMORY 是否重叠；
- Descriptor / Buffer 地址与大小；
- MPU Region base / size / priority；
- D-Cache 策略；
- map / ELF 实际地址；
- RX/TX ownership 在目标 HAL 版本上的行为。

完整接入步骤以根 `README.md` 为准。
