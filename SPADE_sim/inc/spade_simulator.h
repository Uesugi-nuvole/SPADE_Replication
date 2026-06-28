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

    uint64_t active_pe_slots = 0;   // 处于 COMPUTING 状态的周期总和
    uint64_t total_macs_done = 0;   // 总共完成的有效乘加运算数
};