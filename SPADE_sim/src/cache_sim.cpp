#include "../inc/cache_sim.h"
#include <iostream>
#include <cstdlib>
#include "../configs/config.h" 

CacheSim::CacheSim() : current_l2_usage(0), current_victim_usage(0) {}

int CacheSim::get_memory_latency(int bytes_needed, bool bypass, bool use_victim) {

    if (use_victim) {
        // 简化的概率命中模型：Victim Cache 专用于 R-Matrix 小工作集，命中率极高
        if (rand() % 100 < config::VICTIM_CACHE_HIT_RATE) { 
            stats.victim_hits++;
            return config::VICTIM_CACHE_LATENCY;
        } else {
            stats.dram_accesses++;
            return config::DRAM_LATENCY;
        }
    }
    
    if (bypass) {
        stats.dram_accesses++;
        return config::DRAM_LATENCY;
    }


    //  L2 Cache 访问
    if (rand() % 100 < config::L2_CACHE_HIT_RATE) { // 如果不使用 bypass/victim，普通 L2 命中率设为 config::L2_CACHE_HIT_RATE
        stats.l2_hits++;
        return config::L2_CACHE_LATENCY;
    } else {
        stats.l2_misses++;
        stats.dram_accesses++;
        return config::DRAM_LATENCY;
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