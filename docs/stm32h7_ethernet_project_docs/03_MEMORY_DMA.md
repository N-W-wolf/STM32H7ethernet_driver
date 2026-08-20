# Memory / DMA / Cache Design

- 状态：Pending M2
- 当前用途：占位并声明该专题的冻结边界。

## 当前已知约束

- STM32H7 Ethernet DMA 需要显式确认可访问内存；
- Ethernet Descriptor / Buffer 不允许随意放置；
- D-Cache 一致性必须显式处理；
- Cache Line 对齐需要纳入 Buffer 设计；
- 计划建立 `.eth_dma` 专用段；
- 第一版优先正确性与可验证性。

## 在 M2 前需要确定

- STM32H743 实际 SRAM 分区；
- ETH DMA Master 的可达矩阵；
- RX Descriptor 区域；
- TX Descriptor 区域；
- RX Buffer 区域；
- TX Buffer 区域；
- 对齐；
- MPU Region；
- Cacheability；
- Shareability；
- Clean / Invalidate 策略；
- Linker Script；
- Buffer ownership；
- Descriptor / Buffer 数量；
- map 文件检查方法。

在这些内容确认前，本文件不提供虚构的最终地址。
