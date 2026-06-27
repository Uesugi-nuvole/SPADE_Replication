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

