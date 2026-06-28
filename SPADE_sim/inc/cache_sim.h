#pragma once
#include <cstdint>

struct CacheStats {
    long long l2_hits = 0;
    long long l2_misses = 0;
    long long victim_hits = 0;
    long long dram_accesses = 0; // 由于 bypass 或 miss 导致的直接访存
};

class CacheSim {
public:
    CacheSim();
    ~CacheSim() = default;

    // 模拟针对不同矩阵块的访存请求
    // @param bytes_needed: 需要读取的字节数
    // @param bypass: 该请求是否绕过所有 Cache (SPADE 优化策略)
    // @param use_victim: 是否去 Victim Cache 中查找 
    void access_memory(int bytes_needed, bool bypass, bool use_victim);

    void print_stats() const;
    
    // 获取当前的内存延迟模拟 (用于拖慢 PE 的状态机)
    int get_memory_latency(int bytes_needed, bool bypass, bool use_victim);

    CacheStats stats;
private:

    // 简化的容量与 LRU 状态追踪（后续完善）
    int current_l2_usage;
    int current_victim_usage;
};