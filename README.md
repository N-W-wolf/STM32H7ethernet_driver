# STM32H7 Ethernet Driver

面向 STM32H7 的可复用 Ethernet 基础组件，采用 STM32 HAL + FreeRTOS + LwIP 的软件环境。当前验证硬件为 STM32H743VIT6 + LAN8720AI，MAC 与 PHY 通过 RMII 连接。

项目重点是把 PHY、STM32H7 MAC/DMA、板级配置和网络协议栈分层，使同一套 Ethernet Driver 可以迁移到不同 STM32H7 控制板。

## 功能支持状态

| 功能 | 状态 | 验证情况 |
| --- | --- | --- |
| STM32CubeMX / CMake 基础工程 | 已实现 | 可编译、可烧录、可上板运行 |
| FreeRTOS / CMSIS-RTOS v2 基础运行环境 | 已实现 | 已上板验证 |
| USART1 调试输出 | 已实现 | 已上板验证 |
| LAN8720AI 硬件 Reset | 已实现 | 已上板验证 |
| MDIO / MDC 读写 | 已实现 | 已上板验证 |
| PHY ID / Address / Strap | 已实现 | 已上板验证 |
| Auto-negotiation | 已实现 | 已上板验证 |
| Link / Speed / Duplex 读取 | 已实现 | 100 Mbit/s Full Duplex 已上板验证 |
| Ethernet DMA 专用 SRAM 与 Descriptor 布局 | 已实现 | 已完成 Build / map 验证 |
| MPU Ethernet DMA 内存属性 | 已实现 | 已完成静态检查与构建验证 |
| RX/TX 数据 Buffer Pool | 未实现 | - |
| 裸 Ethernet Frame RX/TX | 未实现 | - |
| ETH IRQ + FreeRTOS 异步收发 | 未实现 | - |
| LwIP `ethernetif` | 未实现 | - |
| Static IPv4 / Ping | 未实现 | - |
| UDP Echo | 未实现 | - |
| TCP Echo | 未实现 | - |

## 验证硬件

| 项目 | 配置 |
| --- | --- |
| MCU | STM32H743VIT6 |
| 封装 | LQFP100 |
| PHY | LAN8720AI |
| MAC/PHY 接口 | RMII |
| PHY Address | 0 |
| PHY Boot MODE | `111` / All capable + Auto-negotiation |
| PHY 本地时钟 | 25 MHz 晶振 |
| RMII REF_CLK | LAN8720AI 输出到 STM32 PA1 |
| Debug UART | USART1，PA9 TX / PA10 RX，115200 8N1 |

详细硬件连接见 [`docs/stm32h7_ethernet_project_docs/02_HARDWARE_BASELINE.md`](docs/stm32h7_ethernet_project_docs/02_HARDWARE_BASELINE.md)。

## 软件架构

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

主要约束：

- Application 不直接操作 `HAL_ETH_xxx()`；
- Ethernet Driver 不引入 IP、UDP、TCP 或机器人业务语义；
- PHY Driver 不依赖 LwIP；
- BSP 只承担板级差异；
- DMA 内存位置、MPU 和 linker 属于显式配置，不依赖普通 `static` 对象的默认放置；
- Ethernet IRQ 保持短小，不执行协议解析、业务逻辑或 `printf`。

详细说明见 [`01_ARCHITECTURE.md`](docs/stm32h7_ethernet_project_docs/01_ARCHITECTURE.md)。

## 目录结构

```text
.
├── BSP/
│   └── stm32h743vit6_iot/       # 当前验证板的 PHY Reset、DMA SRAM 准备等板级代码
├── Core/                         # CubeMX 生成的应用入口和 MCU 初始化代码
├── Drivers/
│   ├── CMSIS/                    # ST / CubeMX
│   ├── STM32H7xx_HAL_Driver/     # ST / CubeMX
│   └── Ethernet/
│       ├── Inc/                  # Ethernet 公共接口
│       ├── Src/                  # STM32H7 Ethernet 相关实现
│       └── PHY/                  # PHY 驱动
├── Middlewares/
│   └── Third_Party/              # FreeRTOS 等 CubeMX 生成组件
├── cmake/
│   ├── stm32cubemx/              # CubeMX 生成的 CMake 目标
│   ├── gcc-arm-none-eabi.cmake
│   └── starm-clang.cmake
├── docs/
│   ├── BOARD_PORTING.md          # 板级迁移指南
│   └── stm32h7_ethernet_project_docs/
├── CMakeLists.txt                # 顶层项目 CMake
├── CMakePresets.json
├── STM32H743xx_FLASH.ld          # 当前验证板 linker，包含 Ethernet DMA 内存布局
├── stm32H7ethernet_demo.ioc      # STM32CubeMX 工程配置
├── build.sh
└── flash.sh
```

## 环境依赖

构建工程需要：

- CMake 3.22 或更高版本；
- Ninja；
- GNU Arm Embedded Toolchain：
  - `arm-none-eabi-gcc`
  - `arm-none-eabi-objcopy`
  - `arm-none-eabi-size`
- STM32CubeMX 6.18.1，用于修改 `.ioc` 和重新生成 CubeMX 管理代码。

烧录固件至少安装一种工具：

- STM32CubeProgrammer CLI：`STM32_Programmer_CLI`；
- stlink：`st-flash`。

Ubuntu 可使用发行版包管理器安装 CMake、Ninja 和 stlink；GNU Arm Embedded Toolchain 与 STM32CubeMX 建议使用 ST / Arm 官方发行版本，并确保对应命令位于 `PATH`。

## 编译

Debug：

```bash
./build.sh
```

Release：

```bash
./build.sh Release
```

清理并重新编译：

```bash
./build.sh Debug --clean
```

丢弃 CMake 缓存并重新配置：

```bash
./build.sh Debug --fresh
```

也可以直接使用 CMake Presets：

```bash
cmake --preset Debug
cmake --build --preset Debug --parallel
```

构建产物默认位于：

```text
build/Debug/
build/Release/
```

正常构建会生成 `.elf`、`.bin`、`.hex` 和 `.map` 文件。

## 烧录

自动选择可用下载后端：

```bash
./flash.sh Debug
```

指定 Release：

```bash
./flash.sh Release
```

指定下载工具或固件：

```bash
./flash.sh -b cube -f build/Debug/stm32H7ethernet_demo.elf
./flash.sh -b st-flash -f build/Debug/stm32H7ethernet_demo.bin
```

只检查工具与固件：

```bash
./flash.sh Debug --check
```

裸 BIN 默认写入 `0x08000000`。完整参数见：

```bash
./flash.sh --help
```

## 调试输出

```text
USART1
PA9  = TX
PA10 = RX
115200 / 8N1
```

Linux 端示例：

```bash
tio /dev/ttyACM0 -b 115200
```

当前验证板的 UART PCB 丝印与有效原理图中的 TX/RX 标识存在反向情况，接线以 MCU 实际信号和硬件基线文档为准。

`printf` 仅用于低频启动和诊断，不用于 Ethernet IRQ 或高频 RX/TX 数据路径。

## STM32CubeMX 与手工代码边界

CubeMX / ST 管理：

```text
stm32H7ethernet_demo.ioc
Core/**
Drivers/CMSIS/**
Drivers/STM32H7xx_HAL_Driver/**
cmake/stm32cubemx/CMakeLists.txt
CubeMX 生成的 Third_Party middleware
```

`Core/**` 中长期手工逻辑只写入 `USER CODE BEGIN / END` 区域。

项目手工维护：

```text
BSP/**
Drivers/Ethernet/**
顶层 CMakeLists.txt
项目脚本
技术文档
STM32H743xx_FLASH.ld 中的项目 Ethernet DMA 布局
```

当前 Ethernet DMA 内存不使用 CubeMX Memory Management Tool 自动管理。MPU 参数保存在 `.ioc` 中，linker 中的 Ethernet DMA section 由项目显式维护。重新 Generate Code 后应检查 `.ioc`、`Core/**` 和 linker diff，避免覆盖项目配置。

## Ethernet DMA 内存

当前验证板将 STM32H743 SRAM3 `0x30040000 ~ 0x30047FFF` 作为 Ethernet DMA 专用内存。RX/TX Descriptor 已固定地址并通过 map 文件验证；RX/TX 数据 Buffer Pool 尚未实现。

详细布局、MPU 属性和验证方法见 [`03_MEMORY_DMA.md`](docs/stm32h7_ethernet_project_docs/03_MEMORY_DMA.md)。

## 板级迁移

迁移到另一块 STM32H7 控制板时，需要重新确认 RMII/MII、PHY、Reset、时钟、DMA 可达 SRAM、MPU、linker 和 IRQ 等板级差异。

迁移步骤见 [`docs/BOARD_PORTING.md`](docs/BOARD_PORTING.md)。

## 技术文档

- [`01_ARCHITECTURE.md`](docs/stm32h7_ethernet_project_docs/01_ARCHITECTURE.md)：软件分层和职责边界；
- [`02_HARDWARE_BASELINE.md`](docs/stm32h7_ethernet_project_docs/02_HARDWARE_BASELINE.md)：当前验证板硬件事实；
- [`03_MEMORY_DMA.md`](docs/stm32h7_ethernet_project_docs/03_MEMORY_DMA.md)：DMA / MPU / linker 设计；
- [`04_RTOS_NETWORK.md`](docs/stm32h7_ethernet_project_docs/04_RTOS_NETWORK.md)：FreeRTOS / LwIP 运行约束；
- [`BOARD_PORTING.md`](docs/BOARD_PORTING.md)：板级迁移指南。
