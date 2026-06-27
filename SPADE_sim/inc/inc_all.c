//  cache_sim.h
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

private:
    CacheStats stats;
    // 简化的容量与 LRU 状态追踪（后续完善）
    int current_l2_usage;
    int current_victim_usage;
};


//spade_pe.h  
#pragma once
#include <vector>
#include <cstdint>

// 1. SPADE 的核心数据包格式 (Tile 指令 + 数据)
struct SpadeTile {
    int tile_id;
    int row_start, row_num;
    int col_start, col_num;
    int nnz; // 非零元素数量
    
    // SPADE 核心贡献：运行时 Cache 策略控制
    bool bypass_cache_r;   // 右矩阵是否 Bypass Cache
    bool bypass_cache_c;   // 左矩阵是否 Bypass Cache
    bool use_victim_cache; // 是否将右矩阵留在 Victim Cache
    
    // 图的稀疏数据 (COO格式)
    std::vector<int> row_indices;
    std::vector<int> col_indices;
    std::vector<float> values;
};

class CacheSim; // 前向声明

// 2. 独立处理单元 (Processing Element)
class SpadePE {
public:
    SpadePE(int id, CacheSim* cache);
    ~SpadePE() = default;

    // 接收调度器派发的新 Tile
    void assign_tile(const SpadeTile& tile);
    
    // 每个时钟周期执行的动作 (Load/Store & Compute)
    void tick();
    
    bool is_idle() const;
    void clear();

     const SpadeTile& get_current_tile() const;

private:
    int pe_id;
    CacheSim* shared_cache; // 指向共享缓存层的指针
    
    bool idle;
    SpadeTile current_tile;
    int compute_cycles_left; // 当前 Tile 剩余的计算周期
    
    // PE 内部的状态机枚举
    enum class PEState { IDLE, FETCHING_DATA, COMPUTING, WAITING_BARRIER };
    PEState state;
};

//spade_simulator.h
#pragma once
#include <vector>
#include "spade_pe.h"
#include "cache_sim.h"
#include "tile_scheduler.h"
#include "../configs/config.h"

class SpadeSimulator {
public:
    SpadeSimulator();
    ~SpadeSimulator();

    // 从外部主循环 (main.cpp) 灌入所有的初始化 Tile
    void initialize_workload(const std::vector<SpadeTile>& workload);

    // 推动整个仿真器前进一个时钟周期
    void tick();
    
    bool is_finished() const;
    void print_final_report() const;

private:
    long long global_cycles;
    
    CacheSim cache;
    std::vector<SpadePE> pes;
    TileScheduler* scheduler;
};

//tile_scheduler.h
#pragma once
#include <vector>
#include <queue>
#include "spade_pe.h"

class TileScheduler {
public:
    TileScheduler(std::vector<SpadePE>& pes);
    ~TileScheduler() = default;

    // 加载 Python 生成的离线 Tile 元数据
    void load_tiles(const std::vector<SpadeTile>& tiles);
    
    // 每个周期的调度逻辑 (分发 Task, 检查 Barrier)
    void tick();
    
    bool all_done() const;

private:
    std::vector<SpadePE>& pe_array;
    std::queue<SpadeTile> pending_tiles;
    
    // 用于 SpMM 写回冲突的 Barrier 控制 (防止多个 PE 竞争写同一行)
    std::vector<bool> row_locks; 
};

