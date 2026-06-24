# SPADE Accelerator Reproduction (SPADE_RE)

## 1. 项目概述 (Project Overview)
本仓库包含用于复现 ISCA 2023 论文 **"SPADE: A Flexible and Scalable Accelerator for SpMM and SDDMM"** 的周期级精确（Cycle-accurate）硬件仿真器。

本项目基于已完成的 Sanger 加速器复现工作演进而来。我们复用了 Sanger 中经过验证的底层 PE 算术逻辑与 FIFO 模块，同时将宏观架构向 SPADE 转移：即构建一个与主机紧耦合（Host-coupled）、基于灵活的 Tile ISA 调度、且共享 Cache 层级的智能软硬件协同架构。

---

## 2. 目录结构 (Directory Structure)

```text
SPADE_RE/
├── configs/            # 硬件配置参数（如 Cache 容积、Tile 维度界限等）
├── docs/               # 论文解构笔记、系统设计文档与参考资料
├── evaluations/        # 仿真运行后的自动生成报告与性能分析日志
└── software/           # 核心仿真系统源码
    ├── inc/            # C++ 头文件（当前融合了 Sanger 遗留逻辑与 SPADE 新框架）
    ├── src/            # C++ 核心代码实现
    └── tests/          # 测试用例、基准测试数据以及 Python 数据预处理脚本
