

/*
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
            int total_macs = current_tile.nnz * config::DEFAULT_HEAD_DIM;
            compute_cycles_left = (total_macs + config::MACS_PER_PE - 1) / config::MACS_PER_PE;
            // 确保即使是非常稀疏的 tile (比如全是0)，也至少消耗 1 周期，避免除零或负数错误
            if (compute_cycles_left <= 0) {
                compute_cycles_left = 1;
            }
            
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

bool SpadePE::is_computing() const {
    return state == PEState::COMPUTING;
}

// 每次 assign_tile 时，顺便把这个块包含的计算量给统计出来
int SpadePE::get_processed_macs() const {
    return current_tile.nnz; 
}

*/

#include "../inc/spade_pe.h"
#include "../inc/cache_sim.h"
#include "../configs/config.h"
#include <iostream>
#include <algorithm>

SpadePE::SpadePE(int id, CacheSim* cache) 
    : pe_id(id), shared_cache(cache), idle(true), compute_cycles_left(0), state(PEState::IDLE) {}

void SpadePE::assign_tile(const SpadeTile& tile) {
    current_tile = tile;
    idle = false;
    
    // =========================================================
    // 1. 精确计算访存需要拖取（Fetch）的总字节数
    // =========================================================
    
    // (a) 左侧稀疏矩阵(Sparse Matrix): 
    // 包括行指针、列索引、非零值 (大致约 12 字节 / non-zero)
    long long sparse_bytes = (long long)current_tile.nnz * 12;

    // (b) 右侧稠密矩阵(Dense Matrix / Features): 
    // SPADE 的 SpMM 需要取对应节点的完整的向量，而不只是索引！
    // 不考虑超级理想的复用(那是 Cache 该干的活)，硬件发出原始请求字节流：
    long long dense_read_bytes = (long long)current_tile.nnz * config::DEFAULT_HEAD_DIM * 4;
    
    // =========================================================
    // 2. 向 CacheSim 发送请求获知真实的 Miss Penalty (惩罚延迟)
    // =========================================================
    int lat_l = shared_cache->get_memory_latency(sparse_bytes, current_tile.bypass_cache_c, false);
    int lat_r = shared_cache->get_memory_latency(dense_read_bytes, current_tile.bypass_cache_r, current_tile.use_victim_cache);
    
    // =========================================================
    // 3. 计算物理传输吞吐瓶颈 (Bandwidth Bound)
    // 即使 100% 命中 SRAM，数据顺着电线排队流进 PE 也需要占据时钟周期 (假设单 PE 宽度能扛 32 Bytes/周期 )
    // =========================================================
    int pipeline_bandwidth_cycles = (sparse_bytes + dense_read_bytes) / 32;

    // 获取数据的时间 ＝ max(内存/Cache返回的等待时间， 吞吐传输时间) + 一些控制状态机闲置切换开销
    compute_cycles_left = std::max({lat_l, lat_r, pipeline_bandwidth_cycles}) + 20;

    // 防止全零空块引发的死心逻辑
    if (compute_cycles_left <= 0) {
        compute_cycles_left = 10;
    }

    state = PEState::FETCHING_DATA;
}

void SpadePE::tick() {
    if (state == PEState::IDLE) return;

    if (state == PEState::FETCHING_DATA) {
        compute_cycles_left--;
        
        if (compute_cycles_left <= 0) {
            // =========================================================
            // 2. Fetch 完成，进入密集的数学乘加阶段 (Compute)
            // =========================================================
            int total_macs = current_tile.nnz * config::DEFAULT_HEAD_DIM;
            
            // 考虑 PE MACs 的并发吞吐度 MACS_PER_PE
            compute_cycles_left = (total_macs + config::MACS_PER_PE - 1) / config::MACS_PER_PE;
            
            if (compute_cycles_left <= 0) {
                compute_cycles_left = 1;
            }
            
            state = PEState::COMPUTING;
        }
    } 
    else if (state == PEState::COMPUTING) {
        compute_cycles_left--;
        
        if (compute_cycles_left <= 0) {
            // =========================================================
            // 3. (可选补足) 仿真输出写回 (Drain) 到内存引发的延迟卡顿
            // 将部分和(Partial Sums)写回，每行生成 HEAD_DIM 个 float (4 字节)
            // =========================================================
            long long write_back_bytes = (long long)current_tile.row_num * config::DEFAULT_HEAD_DIM * 4;
            int write_lat = shared_cache->get_memory_latency(write_back_bytes, false, false); 
            
            // 下周期进入闲置，把锁释放回调度器
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

bool SpadePE::is_computing() const {
    return state == PEState::COMPUTING;
}

// 每次 assign_tile 时，顺便把这个块包含的计算量给统计出来
int SpadePE::get_processed_macs() const {
    return current_tile.nnz * config::DEFAULT_HEAD_DIM; // <- 这里也帮你修了，总算力也要乘以特征维！
}