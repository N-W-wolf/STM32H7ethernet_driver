# STM32H7 Ethernet Demo

基于 STM32H743VIT6 的 Ethernet 基础工程，计划以 LAN8720AI 为首个验证 PHY，逐步完成 RMII、STM32H7 Ethernet MAC/DMA、FreeRTOS 和 LwIP 的完整通信链路。

> 当前项目仍处于工程基线阶段。仓库已经包含可编译的 STM32CubeMX/CMake 基础工程，但 PHY、Ethernet MAC/DMA、FreeRTOS 和 LwIP 尚未正式接入。详细进度请查看 [`docs/stm32h7_ethernet_project_docs/07_STATUS.md`](docs/stm32h7_ethernet_project_docs/07_STATUS.md)。

## 硬件与软件基线

| 项目 | 当前配置 |
| --- | --- |
| MCU | STM32H743VIT6 |
| 封装 | LQFP100 |
| CPU | Arm Cortex-M7，最高 480 MHz |
| PHY | LAN8720AI（计划首个验证型号） |
| MAC/PHY 接口 | RMII |
| 驱动库 | STM32H7 HAL / CMSIS |
| RTOS | FreeRTOS（规划中） |
| 网络协议栈 | LwIP（规划中） |
| 构建系统 | CMake + Ninja |
| 编译工具链 | GNU Arm Embedded Toolchain |
| 下载接口 | ST-Link / SWD |

## 当前功能

- STM32CubeMX 工程配置；
- STM32H743 启动文件和链接脚本；
- HAL、CMSIS 和基础 GPIO 初始化代码；
- Debug、Release 两套 CMake 构建预设；
- ELF、BIN、HEX 固件生成；
- STM32CubeProgrammer CLI 或 `st-flash` 下载支持。

计划逐步实现：

- LAN8720AI Reset、MDIO/MDC、PHY ID 和自动协商；
- Link、Speed 和 Duplex 状态管理；
- STM32H7 Ethernet MAC、DMA Descriptor 和 Buffer；
- MPU、D-Cache 与 DMA 内存一致性处理；
- FreeRTOS 异步收发；
- LwIP、静态 IPv4、Ping、UDP Echo 和 TCP Echo。

## 目录结构

```text
.
├── Core/                       # CubeMX 生成的应用入口和 MCU 初始化代码
│   ├── Inc/
│   └── Src/
├── Drivers/                    # STM32 HAL、CMSIS 设备头文件及源码
├── cmake/
│   ├── stm32cubemx/            # CubeMX 生成代码的 CMake 目标
│   ├── gcc-arm-none-eabi.cmake # GNU Arm 工具链配置
│   └── starm-clang.cmake       # Arm Clang 工具链配置
├── docs/                       # 架构、硬件基线、测试与项目状态文档
├── CMakeLists.txt              # 顶层 CMake 工程
├── CMakePresets.json           # Debug / Release 构建预设
├── STM32H743xx_FLASH.ld        # Flash 链接脚本
├── startup_stm32h743xx.s       # Cortex-M7 启动代码
├── stm32H7ethernet_demo.ioc    # STM32CubeMX 工程配置
├── build.sh                    # 配置和编译脚本
└── flash.sh                    # ST-Link 下载脚本
```

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

使用 STM32CubeMX 打开根目录下的 `stm32H7ethernet_demo.ioc`。重新生成代码前建议注意：

1. 将自定义代码放在 CubeMX 的 `USER CODE BEGIN` / `USER CODE END` 区域内；
2. 生成代码后检查 `cmake/stm32cubemx/CMakeLists.txt` 中的源文件和宏定义；
3. 不要删除或覆盖顶层自定义构建、烧录脚本；
4. 重新编译 Debug 和 Release 配置，确认生成结果一致；
5. 提交前检查 CubeMX 生成差异，避免无关配置变化进入版本库。

## 开发注意事项

STM32H7 Ethernet 开发中，DMA 可访问内存、Descriptor/Buffer 对齐、MPU 属性和 D-Cache 一致性会直接影响通信稳定性。在相关设计冻结前，请勿随意指定 DMA 内存地址或启用未经验证的 Cache 策略。

当前项目仍以打通基础链路和验证正确性为首要目标，后续再进行零拷贝和性能优化。

