#include "../inc/spade_pe.h"
#include "../inc/cache_sim.h"
#include "../configs/config.h"
#include <iostream>

SpadePE::SpadePE(int id, CacheSim* cache) 
    : pe_id(id), shared_cache(cache), idle(true), compute_cycles_left(0), state(PEState::IDLE) {}

void SpadePE::assign_tile(const SpadeTile& tile) {
    current_tile = tile;
    idle = false;
    
    // 1. 计算访存延迟： 左矩阵(Spade) + 右矩阵(Dense)
    int lat_l = shared_cache->get_memory_latency(current_tile.nnz * 12, current_tile.bypass_cache_c, false);
    int lat_r = shared_cache->get_memory_latency(current_tile.col_num * 4, current_tile.bypass_cache_r, current_tile.use_victim_cache);
    
    // 取最大访存延迟作为 Fetch 阶段准备时间 (简化模型)
    compute_cycles_left = std::max(lat_l, lat_r);
    state = PEState::FETCHING_DATA;
}

void SpadePE::tick() {
    if (state == PEState::IDLE) return;

    if (state == PEState::FETCHING_DATA) {
        compute_cycles_left--;
        if (compute_cycles_left <= 0) {
            // 2. Fetch 完成，切入计算。一个 PE 每个周期可以执行 MACS_PER_PE 次乘加
            compute_cycles_left = (current_tile.nnz / config::MACS_PER_PE) + 1;
            state = PEState::COMPUTING;
        }
    } 
    else if (state == PEState::COMPUTING) {
        compute_cycles_left--;
        if (compute_cycles_left <= 0) {
            // SPADE 中处理完毕后即可进入 IDLE（写入主存冲突已由 Scheduler Barrier 挡住）
            state = PEState::IDLE;
            idle = true; 
        }
    }
}

bool SpadePE::is_idle() const {
    return idle;
}

const SpadeTile& SpadePE::get_current_tile() const {
    return current_tile;
}