#include "../inc/cache_sim.h"
#include <iostream>

CacheSim::CacheSim() : current_l2_usage(0), current_victim_usage(0) {}

int CacheSim::get_memory_latency(int bytes_needed, bool bypass, bool use_victim) {
    // 假设硬件参数 (时钟周期)
    const int L2_LATENCY = 10;
    const int VICTIM_LATENCY = 3;   // Victim Cache 离执行单元极近
    const int DRAM_LATENCY = 150;   // 昂贵的主存访问

    if (use_victim) {
        // 简化的概率命中模型：Victim Cache 专用于 R-Matrix 小工作集，命中率极高
        // 实际硬件中是靠硬件地址映射，这里我们用 90% 命中率近似
        if (rand() % 100 < 90) { 
            stats.victim_hits++;
            return VICTIM_LATENCY;
        } else {
            stats.dram_accesses++;
            return DRAM_LATENCY;
        }
    }
    
    if (bypass) {
        stats.dram_accesses++;
        return DRAM_LATENCY;
    }



    // 默认 L2 Cache 访问
    if (rand() % 100 < 60) { // 如果不使用 bypass/victim，普通 L2 命中率设为 60%
        stats.l2_hits++;
        return L2_LATENCY;
    } else {
        stats.l2_misses++;
        stats.dram_accesses++;
        return DRAM_LATENCY;
    }
}

void CacheSim::access_memory(int bytes_needed, bool bypass, bool use_victim) {
    // 这个接口可以用于精确的带宽追踪，目前主要靠 get_memory_latency 进行周期延迟注入
}

void CacheSim::print_stats() const {
    std::cout << "\n=== SPADE Cache Hierarchy Statistics ===\n";
    std::cout << "  L2 Cache Hits       : " << stats.l2_hits << "\n";
    std::cout << "  L2 Cache Misses     : " << stats.l2_misses << "\n";
    std::cout << "  Victim Cache Hits   : " << stats.victim_hits << " (SPADE Specific)\n";
    std::cout << "  DRAM Accesses       : " << stats.dram_accesses << " (Bypass & Misses)\n";
    std::cout << "========================================\n";
}