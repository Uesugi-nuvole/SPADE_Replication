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