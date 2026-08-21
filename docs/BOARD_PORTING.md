# STM32H7 Ethernet 板级迁移指南

本文说明如何把当前 Ethernet 组件迁移到另一块 STM32H7 控制板。迁移的目标是把变化限制在 CubeMX 配置、BSP、PHY 驱动和板级内存配置中，尽量不修改通用 Ethernet Driver、`ethernetif`、LwIP 和应用网络逻辑。

## 1. 迁移边界

通常需要修改：

```text
stm32H7ethernet_demo.ioc
BSP/<board>/board_ethernet.c/.h
板级 linker / MPU 配置
PHY Driver（仅 PHY 型号变化时）
顶层构建配置（仅需要选择不同板级文件时）
```

原则上不应因为换 PCB 而修改：

```text
Ethernet Driver 的通用 Frame / ownership 逻辑
ethernetif
LwIP
Application 网络逻辑
```

## 2. 迁移前需要确认的硬件事实

先从当前有效 PCB / 原理图确认：

- MCU 精确型号和封装；
- MCU 是否包含 Ethernet MAC；
- MII / RMII 模式；
- PHY 型号；
- PHY 地址；
- PHY Reset GPIO；
- MDIO / MDC 引脚；
- RMII/MII 数据引脚；
- REF_CLK 来源和方向；
- PHY strap；
- PHY 电源与 regulator 配置；
- PHY interrupt 是否使用；
- 变压器 / RJ45 连接；
- Ethernet DMA 可访问 SRAM。

不要从另一块 STM32H7 板直接复制 GPIO、SRAM 地址或 PHY strap。

## 3. CubeMX 配置

使用项目对应版本的 STM32CubeMX 打开 `.ioc`。

需要配置：

- Ethernet Media Interface；
- RMII/MII GPIO；
- PHY Reset 对应普通 GPIO；
- Ethernet Descriptor 地址；
- Cortex-M7 MPU；
- ETH IRQ（需要异步收发时）；
- FreeRTOS 基础环境。

当前项目约定：

- `Core/**` 手工代码只写在 `USER CODE BEGIN / END` 区域；
- `cmake/stm32cubemx/CMakeLists.txt` 不手工修改；
- Ethernet DMA linker section 不由 CubeMX Memory Management Tool 自动生成；
- MPU 参数可以由 `.ioc` 保存；
- Generate Code 后必须检查 diff。

## 4. BSP

为新板建立对应目录，例如：

```text
BSP/<board>/
    board_ethernet.c
    board_ethernet.h
```

当前板级接口：

```c
void BoardEthernet_PhyResetAssert(void);
void BoardEthernet_PhyResetRelease(void);
void BoardEthernet_PrepareDmaMemory(void);
```

迁移时根据实际 PCB 实现：

- PHY nRST 拉低 / 释放；
- DMA SRAM 所需时钟；
- 其他真正属于板级的准备操作。

如果新板的 DMA SRAM 不需要额外时钟准备，`BoardEthernet_PrepareDmaMemory()` 可以保持为空实现，但通用 Driver 不应因此加入 MCU 地址判断。

## 5. PHY

如果仍使用 LAN8720AI，并且 PHY 行为与当前硬件一致，优先复用现有 PHY Driver，只更新板级：

- PHY Address；
- Reset；
- REF_CLK / strap 配置；
- 必要的板级连接。

如果更换 PHY 型号，应新增对应 PHY Driver，至少重新确认：

- PHY ID；
- Clause 22 / Clause 45 管理方式；
- Reset timing；
- Auto-negotiation；
- Link 状态寄存器；
- Speed / Duplex 获取方法；
- latch 行为；
- interrupt / strap 特性。

不要把新 PHY 的特殊寄存器判断写进 STM32H7 MAC/DMA Driver。

## 6. DMA SRAM 选择

这是迁移中最容易引入隐蔽错误的部分。

必须根据目标 MCU Reference Manual 确认 Ethernet DMA Master 能访问哪些 SRAM。

检查：

- SRAM 物理地址；
- 总容量；
- Ethernet DMA 可达性；
- 是否被其他模块占用；
- SRAM clock；
- Cache 属性；
- MPU Region 对齐要求。

不要把普通 `.bss`、DTCM 或任意 `static` 数组直接当作 Ethernet DMA Buffer。

当前 STM32H743VIT6 参考配置：

```text
RAM_ETH
0x30040000 ~ 0x30047FFF
SRAM3 / 32 KiB
```

这只是当前验证板方案，迁移时需要重新确认。

## 7. Linker

当前工程使用根目录：

```text
STM32H743xx_FLASH.ld
```

表达当前板的 Ethernet DMA 物理布局。

迁移时需要：

- 从普通 RAM 中划出 DMA 专用 region；
- 固定 CubeMX Descriptor input section；
- 为 RX/TX Buffer 建立明确 output section；
- 使用 `ALIGN()`；
- 使用 `ASSERT()` 检查地址、大小和 section 非空；
- 构建后检查 `.map` / ELF。

不建议使用脚本通过正则或字符串替换修改 `.ld`。linker 配置属于板级硬件事实，应保持显式、可审查。

如果项目需要同时维护多块板，可以在实际出现多板需求时把 linker 拆成 board-specific 文件并由构建系统选择。不要提前维护无实际用户的模板体系。

## 8. MPU / Cache

根据选择的 DMA SRAM 配置 Cortex-M7 MPU。

当前验证板参考：

```text
SRAM3 整体
0x30040000 / 32 KiB
Normal, Non-cacheable

Descriptor overlay
0x30040000 / 256 B
Device, Non-cacheable
```

迁移时不能只复制这个地址，需要重新确认：

- Region Base；
- Region Size；
- Base 与 Size 对齐；
- TEX / Cacheable / Bufferable；
- Shareable；
- Execute Never；
- Region 重叠优先级。

如果新板决定把 DMA Buffer 放在 Cacheable RAM，必须实现并验证 D-Cache Clean / Invalidate，不能沿用当前 Non-cacheable 假设。

## 9. Descriptor 与 Buffer

至少确认：

- `ETH_RX_DESC_CNT`；
- `ETH_TX_DESC_CNT`；
- HAL Descriptor 结构大小；
- RX/TX Descriptor 实际地址；
- RX Buffer 数量和大小；
- TX Buffer 数量和大小；
- 32-byte Cache Line 对齐；
- CPU / DMA ownership；
- HAL RX Allocate / Link callback 行为；
- TX completion / free 行为。

Descriptor 数量变化后，必须重新检查 linker slot 和 `.map`，不能假设原地址仍然安全。

## 10. ETH IRQ 与 FreeRTOS

启用 ETH IRQ 后必须核对：

- `configPRIO_BITS`；
- `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`；
- ETH NVIC priority；
- ISR 是否调用 FreeRTOS FromISR API；
- callback 中是否存在阻塞、`printf` 或协议业务。

中断只做必要硬件处理和任务通知。

## 11. 构建验证

每次迁移至少执行：

```bash
./build.sh Debug --fresh
```

检查 `.map`：

```bash
grep -E "RxDescripSection|TxDescripSection|DMARxDscrTab|DMATxDscrTab" \
  build/Debug/stm32H7ethernet_demo.map
```

也可以检查 ELF：

```bash
arm-none-eabi-nm -n build/Debug/stm32H7ethernet_demo.elf | \
  grep -E "DMARxDscrTab|DMATxDscrTab"
```

如果已经建立 RX/TX Buffer section，还需要检查：

- Buffer 是否全部位于 DMA 可达 SRAM；
- section 是否越界；
- 地址是否满足对齐要求。

## 12. 上板验证

建议按硬件依赖从低到高验证：

1. PHY Reset；
2. MDIO Read / Write；
3. PHY ID / Address / Strap；
4. Auto-negotiation；
5. Link / Speed / Duplex；
6. Descriptor / Buffer DMA 访问；
7. 裸 Ethernet Frame TX；
8. 裸 Ethernet Frame RX；
9. IRQ / FreeRTOS 异步收发；
10. LwIP / Ping / UDP / TCP。

每一项都区分：

```text
Static Review
Build Verified
On-board Verified
Measured
```

成功编译不能写成功能已经上板验证。

## 13. 自动化建议

自动化优先用于“验证结果”，不用于“修改硬件配置”。

适合脚本化：

- 解析 `.map` / ELF；
- 检查 Descriptor / Buffer 地址；
- 检查 alignment；
- 检查是否越出 DMA region；
- CI 中阻止错误布局合入。

暂不建议脚本化：

- regex patch CubeMX 生成 `.ld`；
- 自动猜测目标 MCU 的 DMA SRAM；
- 自动覆盖 MPU / `.ioc` 配置。

如果板卡数量增多，再评估结构化 Board Config + linker template 生成方案。
