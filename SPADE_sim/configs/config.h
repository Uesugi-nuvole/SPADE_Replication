/*
#pragma once
#include <cstddef>

namespace config {
    // ====================== 硬件规格参数 (Hardware Specs) ======================
    // 系统主频 (MHz) 和 DRAM 带宽 (GB/s)
    constexpr int    SYS_FREQ_MHZ               = 500;
    constexpr double DRAM_BANDWIDTH_GBPS        = 128.0;

    // ====================== 脉动阵列维度 ======================
    // 原本写死在 systolic_array.h 的 16x16，在此统一配置
    constexpr int    ARRAY_ROWS                 = 16;
    constexpr int    ARRAY_COLS                 = 16;

    // ====================== SRAM 容量配置 ======================
    // (单位: Bytes, 默认 128KB)
    constexpr size_t SRAM_CAPACITY_QUERY        = 128 * 1024;
    constexpr size_t SRAM_CAPACITY_KEY          = 128 * 1024;
    constexpr size_t SRAM_CAPACITY_VALUE        = 128 * 1024;
    constexpr size_t SRAM_CAPACITY_OUTPUT       = 128 * 1024;

    // ====================== FIFO 参数 ======================
    constexpr int    FIFO_DEFAULT_DEPTH         = 64;

    // ====================== 算法与切块 (Tiling) ======================
    constexpr int    DEFAULT_TILE_M             = 16;
    constexpr int    DEFAULT_TILE_N             = 16;
    constexpr int    DEFAULT_HEAD_DIM           = 64;

    // ====================== 调度器流水线周期 ======================
    // 对应 16x16 阵列的参考值 (Compute 一般为 Head_Dim, Drain 为 Rows+Cols-1)
    constexpr int    DEFAULT_SDDMM_COMP_CYCLES  = 32;
    constexpr int    DEFAULT_SDDMM_DRAIN_CYCLES = 31;
    constexpr int    DEFAULT_SPMM_COMP_CYCLES   = 32;
    constexpr int    DEFAULT_SPMM_DRAIN_CYCLES  = 31;
}
*/

#pragma once

namespace config {
    // 硬件拓扑配置
    constexpr int NUM_PES = 16;                 // 初始测试先放 16 个 PE 
    constexpr int MACS_PER_PE = 16;             // 每个 PE 内部的并行 MAC 数量

    // Cache 相关配置 (参考 Intel Ice Lake 架构及论文 BBF 设定)
    constexpr int L2_CACHE_SIZE_KB = 512;       // 每个 Core 的 L2 Cache
    constexpr int VICTIM_CACHE_SIZE_KB = 16;    // SPADE 特有的 Victim Cache (用于 rMatrix)
    constexpr int CACHE_LINE_BYTES = 64;        // 缓存行大小

    // 默认的 Tile 切分大小 (在运行时可由 metadata 覆盖，这里作默认参考)
    constexpr int DEFAULT_TILE_M = 16;
    constexpr int DEFAULT_TILE_N = 16;
    constexpr int DEFAULT_TILE_K = 64;
}