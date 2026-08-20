# STM32H7 Ethernet Demo

基于 STM32H743VIT6 的 Ethernet 基础工程，以 LAN8720AI 为首个验证 PHY，逐步完成 RMII、STM32H7 Ethernet MAC/DMA、FreeRTOS 和 LwIP 的完整通信链路。

## 硬件与软件基线

| 项目 | 当前配置 |
| --- | --- |
| MCU | STM32H743VIT6 |
| 封装 | LQFP100 |
| CPU | Arm Cortex-M7，最高 480 MHz |
| PHY | LAN8720AI |
| MAC/PHY 接口 | RMII |
| 驱动库 | STM32H7 HAL / CMSIS |
| RTOS | FreeRTOS / CMSIS-RTOS v2 |
| 网络协议栈 | LwIP（尚未接入） |
| HAL Timebase | TIM6 |
| Debug UART | USART1，PA9 TX / PA10 RX，115200 8N1 |
| 构建系统 | CMake + Ninja |
| 编译工具链 | GNU Arm Embedded Toolchain |
| 下载接口 | ST-Link / SWD |

## 当前功能

- STM32CubeMX 工程配置；
- STM32H743 启动文件和链接脚本；
- HAL、CMSIS 和基础 GPIO 初始化代码；
- FreeRTOS / CMSIS-RTOS v2 最小运行环境；
- `BootstrapTask`；
- LED1 周期心跳；
- USART1 调试输出；
- `printf -> _write() -> HAL_UART_Transmit()` 重定向；
- Debug、Release 两套 CMake 构建预设；
- ELF、BIN、HEX 固件生成；
- STM32CubeProgrammer CLI 或 `st-flash` 下载支持。

下一阶段计划实现：

- LAN8720AI Reset；
- MDIO/MDC；
- PHY ID；
- Strap 验证；
- Auto-negotiation；
- Link、Speed 和 Duplex 状态管理。

之后再逐步进入：

- STM32H7 Ethernet MAC、DMA Descriptor 和 Buffer；
- MPU、D-Cache 与 DMA 内存一致性处理；
- FreeRTOS 异步 Ethernet RX/TX；
- LwIP、静态 IPv4、Ping、UDP Echo 和 TCP Echo。

## 目录结构

```text
.
├── Core/                       # CubeMX 生成的应用入口和 MCU 初始化代码
│   ├── Inc/
│   └── Src/
├── Drivers/
│   ├── CMSIS/                  # ST / CubeMX
│   ├── STM32H7xx_HAL_Driver/   # ST / CubeMX
│   └── Ethernet/               # 后续手工维护的 Ethernet Driver
├── BSP/                        # 板级手工代码
│   └── stm32h743vit6_iot/
├── Middlewares/
│   ├── Third_Party/            # CubeMX 生成的 FreeRTOS 等第三方组件
│   └── Network/                # 后续手工维护的网络适配层
├── App/                        # 后续应用层示例
├── cmake/
│   ├── stm32cubemx/            # CubeMX 生成代码的 CMake 目标
│   ├── gcc-arm-none-eabi.cmake # GNU Arm 工具链配置
│   └── starm-clang.cmake       # Arm Clang 工具链配置
├── docs/                       # 架构、硬件基线、测试与项目状态文档
├── CMakeLists.txt              # 顶层手工维护 CMake 工程
├── CMakePresets.json           # Debug / Release 构建预设
├── STM32H743xx_FLASH.ld        # Flash 链接脚本
├── startup_stm32h743xx.s       # Cortex-M7 启动代码
├── stm32H7ethernet_demo.ioc    # STM32CubeMX 工程配置
├── build.sh                    # 配置和编译脚本
└── flash.sh                    # ST-Link 下载脚本
```

空的未来模块目录可能不会出现在 GitHub 中；对应模块开始开发时再创建实际源文件，不为占位提前建立空文件。

## M0 调试输出

当前基础调试串口：

```text
USART1
PA9  = TX
PA10 = RX
115200 / 8N1
```

Ubuntu 端可使用：

```bash
tio /dev/ttyACM0 -b 115200
```

当前验证板实测发现 UART 相关 PCB 丝印与有效原理图中的 TX/RX 标识相反。接线时以 MCU 实际信号和项目硬件基线文档为准。

当前 `printf` 路径仅用于低频启动和 Bring-up 调试，不用于 Ethernet IRQ 或高频收发路径。

## 环境要求

构建工程需要安装以下工具，并确保命令位于 `PATH`：

- CMake 3.22 或更高版本；
- Ninja（推荐）；
- GNU Arm Embedded Toolchain，包括：
  - `arm-none-eabi-gcc`
  - `arm-none-eabi-objcopy`
  - `arm-none-eabi-size`

烧录固件时，至少安装下列工具之一：

- STM32CubeProgrammer CLI：`STM32_Programmer_CLI`；
- stlink 工具集：`st-flash`。

需要修改外设配置或重新生成代码时，还需要 STM32CubeMX。

## 编译

### 使用构建脚本

默认编译 Debug 版本：

```bash
./build.sh
```

编译 Release 版本：

```bash
./build.sh Release
```

清理并重新编译：

```bash
./build.sh Debug --clean
```

丢弃现有 CMake 缓存并重新配置：

```bash
./build.sh Debug --fresh
```

查看脚本的全部选项：

```bash
./build.sh --help
```

### 直接使用 CMake Presets

```bash
cmake --preset Debug
cmake --build --preset Debug --parallel
```

Release 构建只需将 `Debug` 替换为 `Release`。

构建结果默认位于：

```text
build/Debug/
build/Release/
```

正常情况下会生成同名的 `.elf`、`.bin`、`.hex` 和 `.map` 文件。

当前已完成可编译、可烧录、可上板运行验证；Debug / Release 两套 `--fresh` 重复构建结果仍应按 `05_TEST_PLAN.md` 补充正式记录。

## 烧录

连接 ST-Link 后，可自动选择已安装的下载后端并烧录最新固件：

```bash
./flash.sh Debug
```

指定 Release 固件：

```bash
./flash.sh Release
```

指定下载工具或固件文件：

```bash
./flash.sh -b cube -f build/Debug/stm32H7ethernet_demo.elf
./flash.sh -b st-flash -f build/Debug/stm32H7ethernet_demo.bin
```

只检查工具与固件，不访问硬件：

```bash
./flash.sh Debug --check
```

裸 BIN 文件默认写入地址为 `0x08000000`，可通过 `--address` 修改。更多选项请执行：

```bash
./flash.sh --help
```

## STM32CubeMX 使用说明

使用 STM32CubeMX 打开根目录下的 `stm32H7ethernet_demo.ioc`。重新生成代码前注意：

1. `Core/**` 的手工逻辑只放在 `USER CODE BEGIN` / `USER CODE END` 区域内，除非项目明确接管文件；
2. `cmake/stm32cubemx/CMakeLists.txt` 由 CubeMX 管理，不手工维护；
3. `BSP/**`、后续 `Drivers/Ethernet/**`、`Middlewares/Network/**` 和 `App/**` 由项目手工维护；
4. 生成代码后检查源文件、宏定义和中断配置是否有无关变化；
5. 提交前检查 CubeMX 生成差异。

## 开发注意事项

STM32H7 Ethernet 开发中，DMA 可访问内存、Descriptor/Buffer 对齐、MPU 属性和 D-Cache 一致性会直接影响通信稳定性。在 M2 相关设计冻结前，不指定最终 DMA 内存地址，也不把某个 Cache 策略作为既定事实。

当前项目仍以最小可验证路径和正确性为优先，后续在数据路径稳定和压力测试完成后再评估零拷贝及性能优化。
