# Test Plan

- 状态：Active
- 说明：本文件定义各里程碑的验收边界；具体命令、脚本、频率和时长在对应阶段补充。

## M0：项目基线

通过条件：

- [ ] 工程可重复编译；
- [x] FreeRTOS 最小任务正常；
- [x] 基本调试输出正常；
- [x] CubeMX 生成代码和自定义代码边界明确；
- [x] 项目目录与架构文档一致。

当前实际验证结果：

- `BootstrapTask` 可以持续运行；
- LED1 周期心跳正常；
- `printf -> _write() -> HAL_UART_Transmit() -> USART1` 链路正常；
- Linux 端可通过 `/dev/ttyACM0` 接收调试日志；
- USART1 使用 PA9 TX / PA10 RX，115200 8N1；
- 调试中发现当前 PCB UART 丝印与有效原理图 TX/RX 标识相反，已记录到 `02_HARDWARE_BASELINE.md`。

说明：本工作单元已经完成一次可编译、可烧录、可运行的 M0 固件验证；`工程可重复编译` 项保留未勾选，待后续明确执行并记录 Debug / Release fresh build 后再关闭。

## M1：PHY Bring-up

测试：

- [ ] PHY Reset；
- [ ] MDIO Read；
- [ ] MDIO Write；
- [ ] PHY ID；
- [ ] PHY Address；
- [ ] Strap 配置；
- [ ] Auto-negotiation；
- [ ] Link Up；
- [ ] Link Down；
- [ ] 10/100 Mbit/s 状态；
- [ ] Half/Full Duplex 状态；
- [ ] 连续插拔网线；
- [ ] 多次 STM32 重启；
- [ ] 多次 PHY Reset。

退出条件：

PHY 在重复上电、Reset、插拔网线后均可稳定返回正确状态。

## M2：MAC / DMA

测试：

- [ ] Descriptor 地址；
- [ ] Buffer 地址；
- [ ] DMA 可访问性；
- [ ] Cache / MPU 属性；
- [ ] TX Frame；
- [ ] RX Frame；
- [ ] IRQ；
- [ ] RX/TX 错误统计；
- [ ] 长时间 DMA；
- [ ] 高负载下无内存破坏。

退出条件：

MAC/DMA 数据路径稳定，地址、Cache 和 Buffer ownership 均有明确证据。

## M3：LwIP + Ping

测试：

- [ ] Static IPv4；
- [ ] ARP；
- [ ] Ping；
- [ ] 不同 Ping payload；
- [ ] 持续 Ping；
- [ ] 拔线恢复；
- [ ] MCU 重启恢复；
- [ ] PC 重启恢复。

退出条件：

可长期稳定 Ping，无 HardFault、DMA Error、内存泄漏或明显 pbuf 异常。

## M4：UDP

测试：

- [ ] UDP Echo；
- [ ] 小包；
- [ ] 接近 MTU 的大包；
- [ ] 随机长度；
- [ ] 高频 RX；
- [ ] 高频 TX；
- [ ] 双向通信；
- [ ] 拔线恢复；
- [ ] 长时间运行；
- [ ] Drop/Error 统计。

## M5：TCP

测试：

- [ ] Connect；
- [ ] Send / Receive；
- [ ] Disconnect；
- [ ] Reconnect；
- [ ] Client crash；
- [ ] Cable disconnect；
- [ ] STM32 reset；
- [ ] 长时间连接。

## M6：压力与通用化

测试：

- [ ] 数小时持续 UDP；
- [ ] 大量小包；
- [ ] 大包；
- [ ] 双向高负载；
- [ ] 快速 Link Up/Down；
- [ ] FreeRTOS stack high-water mark；
- [ ] LwIP memory pool；
- [ ] CPU load；
- [ ] RX/TX drop；
- [ ] DMA error；
- [ ] 更换同类 STM32H7 板时主要修改 BSP / 配置。

## 测试记录要求

每个测试最终需要记录：

- 前置条件；
- 固件 commit；
- Cube/HAL 版本；
- PC 端工具；
- 操作步骤；
- 预期结果；
- 实际结果；
- 错误计数；
- 是否通过；
- 必要的示波器/逻辑分析仪测量。
