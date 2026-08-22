# STM32H743 + LAN8720 FreeRTOS Reference Example

本目录是 `Ethernet/` Driver Package 的完整参考工程，不是 Driver Package 本身。

验证硬件：

```text
MCU       : STM32H743VIT6
PHY       : LAN8720AI
Interface : RMII
RTOS      : FreeRTOS / CMSIS-RTOS2
Debug UART: USART1, PA9/PA10, 115200 8N1
```

当前工具版本：

```text
STM32CubeMX : 6.18.1
STM32CubeH7 : V1.13.0
HAL ETH     : 1.11.6
```

## 1. 目录关系

```text
repository root
├── Ethernet/                         ← 可复用 Driver Package
└── examples/
    └── STM32H743_LAN8720_FreeRTOS/   ← 本参考工程
        ├── BSP/
        ├── Core/
        ├── Drivers/
        ├── Middlewares/
        ├── cmake/
        ├── CMakeLists.txt
        ├── CMakePresets.json
        ├── STM32H743xx_FLASH.ld
        ├── stm32H7ethernet_demo.ioc
        ├── startup_stm32h743xx.s
        ├── build.sh
        └── flash.sh
```

Example 的顶层 `CMakeLists.txt` 通过 `../../Ethernet` 引用仓库根目录 Driver Package，因此不要把一份 Driver 副本复制进 Example。

## 2. CubeMX / 手工代码边界

CubeMX / ST 管理：

```text
stm32H7ethernet_demo.ioc
Core/**
Drivers/CMSIS/**
Drivers/STM32H7xx_HAL_Driver/**
Middlewares/Third_Party/FreeRTOS/**
cmake/stm32cubemx/CMakeLists.txt
startup / system 等生成内容
```

对 `Core/**` 的长期手工逻辑只放在 `USER CODE BEGIN / END` 区域。

不要手工修改：

```text
cmake/stm32cubemx/CMakeLists.txt
```

长期维护的 Driver 源码在仓库根目录 `Ethernet/`；当前板级 Port 位于：

```text
BSP/stm32h743vit6_iot/ethernet_port.c
```

## 3. 当前 Ethernet DMA 内存

```text
RAM_ETH      = 0x30040000 / 32 KiB
RX Desc      = 0x30040000
TX Desc      = 0x30040080
RX Pool      = 0x30042000 / 0x1800 / 4 × 1536 B
TX Pool      = 0x30044000 / 0x1800 / 4 × 1536 B
```

MPU：

```text
SRAM3 whole region : Normal Non-cacheable
first 256 B        : Device overlay for descriptors
```

当前 I-Cache / D-Cache 均关闭。

`STM32H743xx_FLASH.ld` 是本 Example 的板级 linker，不属于 Driver Package 固定配置。

## 4. 构建

本 Example 使用 CMake + Ninja + GNU Arm Embedded Toolchain。其他 Driver 用户不需要采用相同工具链。

在本目录执行：

```bash
./build.sh Debug --fresh
./build.sh Release --fresh
```

或者使用 CMake Preset：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

## 5. 检查 DMA map

Debug build 后：

```bash
grep -E "RxDescripSection|TxDescripSection|eth_dma_rx|eth_dma_tx|DMARxDscrTab|DMATxDscrTab" \
  build/Debug/stm32H7ethernet_demo.map
```

也可检查 ELF：

```bash
arm-none-eabi-nm -n build/Debug/stm32H7ethernet_demo.elf | \
  grep -E "DMARxDscrTab|DMATxDscrTab"
```

预期：

```text
DMARxDscrTab = 0x30040000
DMATxDscrTab = 0x30040080
.eth_dma_rx  = 0x30042000 / 0x1800
.eth_dma_tx  = 0x30044000 / 0x1800
```

不要只以“成功编译”替代 map / ELF 地址检查。

## 6. 烧录

构建完成后：

```bash
./flash.sh Debug
```

脚本会优先使用 `STM32_Programmer_CLI`，也支持 `st-flash`。可以先只检查环境和固件：

```bash
./flash.sh Debug --check
```

## 7. 异步 RX 启动日志

正常启动参考输出：

```text
[ETH] EthernetRxTask started
[ETH] BootstrapTask started
[ETH] PHY ready
[ETH] Auto-negotiation started
[ETH] Link up
[ETH] Speed=100M
[ETH] Duplex=Full
[ETH] MAC/DMA started
```

当前 Demo 使用 EtherType `0x88B5` 做 Raw Frame RX 验证。PC 连续发送 1000 帧、约 5 ms / Frame 时，已得到：

```text
[ETH] Async RX test 1000/1000 PASS, total=1000
```

该测试验证 IRQ → Thread Flag → RX Task → Driver Receive → Buffer recycle 的基础连续运行，不是吞吐极限或长时间 Stress Test。

## 8. FreeRTOS Task 集成

当前已验证代码仍由 CubeMX 创建 `EthernetRxTask`，其生成入口 wrapper 在 USER CODE 中注册 Demo Frame Handler，然后调用：

```c
EthernetRtos_RxTask(argument);
```

Driver Package 本身不创建 Task。

长期推荐方向是让 CubeMX Task Entry 使用 `EthernetRtos_RxTask` 并选择 `As weak`：CubeMX 负责 Task Object / priority / stack / allocation，Package 提供强定义 Task Entry。CubeMX 6.18.1 的 `As weak` 生成行为已经从同版本工程确认，但本 Example 的 `.ioc` 尚未执行这一改法后的 Generate Code + Build + On-board 回归，因此目前不直接修改生成文件假装已经完成。

在本 Example 上实际切换时，应通过 CubeMX UI 修改 Task code generation option，Generate Code 后检查：

```bash
git diff -- \
  stm32H7ethernet_demo.ioc \
  Core/Src/freertos.c
```

确认 CubeMX 生成 `__weak` Entry 且 `osThreadNew()` 仍由 CubeMX 管理，再重新执行 Debug / Release / 上板 RX 测试。

## 9. 当前验证等级

On-board Verified：

```text
PHY Reset / MDIO / ID / Address
Auto-negotiation
Link Up / Down
100M Full Duplex
Raw TX
Raw RX
Polling RX 1000 / 1000
ETH IRQ + CMSIS-RTOS2 Async RX 1000 / 1000
```

Driver Package 第一轮重构后已经重新完成 Debug Build 和 Async RX 1000 / 1000 上板回归。

本次把完整 Demo 移入 `examples/` 只改变仓库路径和 Example 构建引用，提交后仍需重新执行构建 / map / 上板回归，不能自动继承为新目录结构的 Build Verified / On-board Verified。
