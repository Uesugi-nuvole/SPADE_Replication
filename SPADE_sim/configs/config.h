#pragma once

namespace config {
    // ==========================================
    // 0. 负载分块尺寸参数 (Workload & Tile Dimensions)
    // Python 预处理脚本依赖这些参数进行图切分
    // ==========================================
    constexpr int DEFAULT_TILE_M = 128;   // 稠密矩阵分块行数 (M)
    constexpr int DEFAULT_TILE_N = 512;   // 稠密矩阵分块列数 (N)
    constexpr int DEFAULT_HEAD_DIM = 32; // 特征向量维度 (D / Head Dim)

    // ==========================================
    // 1. SPADE 计算架构参数 (Architecture Config)
    // ==========================================
    constexpr int NUM_PES = 16;          // 处理单元数量
    constexpr int MACS_PER_PE = 8;       // 每个 PE 每周期可执行的乘加次数 (吞吐量)
    
    // SRAM 容量限制 
    constexpr int SRAM_CAPACITY_QUERY  = 1024 * 1024;
    constexpr int SRAM_CAPACITY_KEY    = 1024 * 1024;
    constexpr int SRAM_CAPACITY_VALUE  = 1024 * 1024;
    constexpr int SRAM_CAPACITY_OUTPUT = 1024 * 1024;

    // ==========================================
    // 2. 存储延迟参数 (Latency in Cycles)
    // 典型的 28nm/45nm 硬件 SRAM 和及 HBM/DRAM 设定
    // ==========================================
    constexpr int VICTIM_CACHE_LATENCY = 3;   // SPADE 特有的近端微型缓存 (L1 级别)
    constexpr int L2_CACHE_LATENCY = 10;      // L2 SRAM 延迟
    constexpr int DRAM_LATENCY = 150;         // 主存访问延迟
    constexpr double DRAM_BANDWIDTH_GBPS = 256.0; 
    constexpr int SYS_FREQ_MHZ = 1000;
    constexpr int BYTES_PER_ELEMENT = 4;      // 单个数据类型的字节数 (FP32 = 4 bytes)
    constexpr int CACHE_LINE_SIZE = 64;       // 缓存行/单次 DRAM 传输粒度 (通常为 32 或 64 Bytes)

    // ==========================================
    // 3. 统计学缓存测算模型参数 (Hit Rates)
    // ==========================================

    constexpr int VICTIM_CACHE_HIT_RATE = 95;      // Victim Cache 极小但利用率高，针对密集侧的重用节点特征
    constexpr int L2_CACHE_HIT_RATE = 60;          // 普通 L2 在不规则图遍历中命中率有限，设定为常规经验值

} 