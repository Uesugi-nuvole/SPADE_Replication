#pragma once

namespace config {
    // ==========================================
    // 1. SPADE 计算架构参数 (Architecture Config)
    // ==========================================
    constexpr int NUM_PES = 16;          // 处理单元数量
    constexpr int MACS_PER_PE = 8;       // 每个 PE 每周期可执行的乘加次数 (决定计算吞吐)

    // ==========================================
    // 2. 存储延迟参数 (Latency in Cycles)
    // 根据 SPADE 
    // ==========================================
    constexpr int VICTIM_CACHE_LATENCY = 3;   // SPADE 特有的近端微型缓存 (L1 级别)
    constexpr int L2_CACHE_LATENCY = 10;      // L2 SRAM 延迟
    constexpr int DRAM_LATENCY = 150;         // 主存 (HBM/DDR) 访问延迟

    // ==========================================
    // 3. 统计学缓存测算模型参数 (Hit Rates)
    // 为了简化全系统精确跟踪，使用论文测试的平均经验分布
    // ==========================================
    // Victim Cache 极小但利用率极高，论文中经常达到 90%+ 的拦截率
    constexpr int VICTIM_CACHE_HIT_RATE = 95; 
    
    // 普通 L2 对于不规则图的命中率其实很低，这里先默认 60%，方便对照
    constexpr int L2_CACHE_HIT_RATE = 60;     

} // namespace config