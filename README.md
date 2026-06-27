# SPADE Accelerator Reproduction (SPADE_RE)

## 1. 项目概述 (Project Overview)
本仓库包含用于复现 ISCA 2023 论文 **"SPADE: A Flexible and Scalable Accelerator for SpMM and SDDMM"** 的周期级精确（Cycle-accurate）硬件仿真器。

本项目基于已完成的 Sanger 加速器复现工作演进而来。我们复用了 Sanger 中经过验证的底层 PE 算术逻辑与 FIFO 模块，同时将宏观架构向 SPADE 转移：即构建一个与主机紧耦合（Host-coupled）、基于灵活的 Tile ISA 调度、且共享 Cache 层级的智能软硬件协同架构。

SPADE的主要贡献：
一： 消除 Host-Accelerator 数据传输开销
SPADE 将 PEs 紧密耦合到 CPU 核心（如 Intel Ice Lake），共享：
L2/LLC cache 层级
TLB（虚拟地址空间）
二级缓存目录（STLB）
实现 零拷贝、零地址映射开销，PE 直接访问 CPU 虚拟地址空间。

二： 基于 Tile 的可编程 ISA（Flexible & Scalable）
定义 Tile-based ISA：每个 tile 对应一组 {r_ids, c_ids, vals, metadata}（见 Figure 15）
支持 运行时配置的执行策略：
tile 尺寸可调（任意 M×N）
内存访问路径可选（bypass cache / 使用 victim cache / L2 cache）
调度可依赖 barrier 控制多线程 tile 执行顺序
举例：
对于 rMatrix（dense 行向量），若 tile 间重用距离远 → 启用 victim cache（BBF 中 16KB 2-way）避免污染 L2；
对于 cMatrix（dense 列向量），因 tile 内按行访问 → 强制 cache；
对于输出 SDDMM 的稀疏矩阵 → bypass 所有 cache（避免污染 + VRF 高重用）
---

## 2. 目录结构 (Directory Structure)

```text
SPADE_RE/
|—— data                # 存放 Python 生成的稀疏数组与元数据文本
    |—— spade_tiles
        |——metadata
        |——tile_0(下略)              
├── docs/               # 论文解构笔记、系统设计文档与参考资料
├── evaluations/        # 仿真运行后的自动生成报告与性能分析日志
└── SPADE_sim/           # 核心仿真系统源码
    ├── config/
    │   └── config.h                # 定义 SPADE 的架构参数 (L2_SIZE, VICTIM_CACHE_SIZE, PE_NUM 等)
    ├── scripts/
    │   └── generate_spade_tiles.py # 生成适合 SPADE 格式的带 Metadata 的 Tile 数据
    ├── inc/
    │   ├── cache_sim.h             # 模拟 L1/L2 和 Victim Cache 层级及命中率
    │   ├── spade_pe.h              #  SPADE 的处理单元 (计算 MACs + 寄存器堆 VRF)
    │   ├── tile_scheduler.h        #  负责派发 Tile 和处理 Barrier 锁
    │   └── spade_simulator.h       #  把 Scheduler, Cache 和 PEs 组装起来
    ├── src/
    │   ├── cache_sim.cpp
    │   ├── spade_pe.cpp
    │   ├── tile_scheduler.cpp
    │   ├── spade_simulator.cpp
    │   └── main.cpp                # 评测入口，跑循环 tick()
                       