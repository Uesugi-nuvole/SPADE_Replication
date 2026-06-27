// cache_sim.cpp
#include "../inc/cache_sim.h"
#include <iostream>

CacheSim::CacheSim() : current_l2_usage(0), current_victim_usage(0) {}

int CacheSim::get_memory_latency(int bytes_needed, bool bypass, bool use_victim) {
    // 假设硬件参数 (时钟周期)
    const int L2_LATENCY = 10;
    const int VICTIM_LATENCY = 3;   // Victim Cache 离执行单元极近
    const int DRAM_LATENCY = 150;   // 昂贵的主存访问

    if (bypass) {
        stats.dram_accesses++;
        return DRAM_LATENCY;
    }

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

//main.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "../inc/spade_simulator.h"
#include "../configs/config.h"

// 从文件读取 Python 生成的 Metadata 和 COO 坐标点
std::vector<SpadeTile> load_spade_workload(const std::string& metadata_path, const std::string& tile_dir) {
    std::vector<SpadeTile> tiles;
    std::ifstream meta_file(metadata_path);
    if (!meta_file.is_open()) {
        std::cerr << "Error: Cannot open metadata.txt\n";
        return tiles;
    }

    std::string header;
    std::getline(meta_file, header); // 跳过表头

    int id, rs, rn, cs, cn, nnz;
    bool bcl, bcr, vtc;

    while (meta_file >> id >> rs >> rn >> cs >> cn >> nnz >> bcl >> bcr >> vtc) {
        SpadeTile tile{id, rs, rn, cs, cn, nnz, bcr, bcl, vtc};

        // 读取具体的 COO 数据 (这里虽然算力仿真没直接算值，但保持工程完整)
        std::ifstream data_file(tile_dir + "/tile_" + std::to_string(id) + ".txt");
        if (data_file.is_open()) {
            int r, c;
            float v;
            while(data_file >> r >> c >> v) {
                tile.row_indices.push_back(r);
                tile.col_indices.push_back(c);
                tile.values.push_back(v);
            }
        }
        else {
            // 加一行报错！看看是不是这儿找不着文件
            std::cerr << "[Warning] Cannot open file: " << tile_dir + "/tile_" + std::to_string(id) + ".txt" << "\n";
        }
        tiles.push_back(tile);
    }
    return tiles;
}

int main() {
    std::cout << "[Mini-SPADE] Initializing Hardware Accelerator...\n";
    SpadeSimulator sim;

    std::cout << "[Mini-SPADE] Loading Cora Graph Tiles Metadata...\n";
    std::vector<SpadeTile> workload = load_spade_workload(
    "C:/Users/Lenovo/Desktop/SOFTMAX-PE/SPADE_RE/data/spade_tiles/metadata.txt", 
    "C:/Users/Lenovo/Desktop/SOFTMAX-PE/SPADE_RE/data/spade_tiles"
);

    if (workload.empty()) {
        std::cerr << "Failed to load workload. Check data generate script.\n";
        return -1;
    }

    std::cout << "Loaded " << workload.size() << " valid Sparse Tiles. Starting Simulation...\n";
    
    sim.initialize_workload(workload);

    // 主循环引擎前进 (Tick!)
    while (!sim.is_finished()) {
        sim.tick();
    }

    sim.print_final_report();
    return 0;
}

//spadd_pe.cpp
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

//spade_simulator.cpp
#include "../inc/spade_simulator.h"
#include <iostream>

SpadeSimulator::SpadeSimulator() : global_cycles(0) {
    // 根据 config 初始化 PE 阵列，并统筹连入 Cache
    for (int i = 0; i < config::NUM_PES; ++i) {
        pes.emplace_back(i, &cache);
    }
    scheduler = new TileScheduler(pes);
}

SpadeSimulator::~SpadeSimulator() {
    delete scheduler;
}

void SpadeSimulator::initialize_workload(const std::vector<SpadeTile>& workload) {
    scheduler->load_tiles(workload);
}

void SpadeSimulator::tick() {
    // 派发任务与锁管理
    scheduler->tick();
    
    // 驱动所有计算单元并行演进时钟
    for (auto& pe : pes) {
        pe.tick();
    }
    
    global_cycles++;
}

bool SpadeSimulator::is_finished() const {
    return scheduler->all_done();
}

void SpadeSimulator::print_final_report() const {
    std::cout << "\n========= SPADE Simulation Complete =========\n";
    std::cout << "  Configuration      : " << config::NUM_PES << " PEs" << std::endl;
    std::cout << "  Total Clock Cycles : " << global_cycles << std::endl;
    cache.print_stats();
    std::cout << "=============================================\n";
}

//tile_simulator.cpp
#include "../inc/tile_scheduler.h"
#include <iostream>
#include <algorithm>

TileScheduler::TileScheduler(std::vector<SpadePE>& pes) : pe_array(pes) {
    // 假设输出矩阵最多 10000 行，初始化锁
    row_locks.resize(100000, false); 
}

void TileScheduler::load_tiles(const std::vector<SpadeTile>& tiles) {
    for (const auto& t : tiles) {
        pending_tiles.push(t);
    }
}

void TileScheduler::tick() {
    // 1. 解锁已经完成工作的 PE 所占用的行 (由于我们用简单模型，我们遍历所有 PE 的归属即可，不需要精准回调)
    // 重新校准所有 active 的行，确保 idle 的 PE 释放掉锁
    std::fill(row_locks.begin(), row_locks.end(), false);
    for (auto& pe : pe_array) {
        if (!pe.is_idle()) {
            int r_start = pe.get_current_tile().row_start;
            int r_num = pe.get_current_tile().row_num;
            for(int i = r_start; i < r_start + r_num; ++i) {
                row_locks[i] = true;
            }
        }
    }

    // 2. 尝试给空闲的 PE 派发新任务
    for (auto& pe : pe_array) {
        if (pe.is_idle() && !pending_tiles.empty()) {
            SpadeTile next_tile = pending_tiles.front();
            
            // 检查 Barrier (即将要写的行是否被别的 PE 占用)
            bool barrier_hit = false;
            for(int i = next_tile.row_start; i < next_tile.row_start + next_tile.row_num; ++i) {
                if (row_locks[i]) { barrier_hit = true; break; }
            }

            if (!barrier_hit) {
                pe.assign_tile(next_tile);
                pending_tiles.pop(); // 只有没撞锁，才真正派发并弹出
                
                // 上锁
                for(int i = next_tile.row_start; i < next_tile.row_start + next_tile.row_num; ++i) {
                    row_locks[i] = true;
                }
            } 
            // 如果撞锁，该 PE 闲着，等下个周期 (或者真实实现中可以在 pending 队列往后找，为了初版简化我们采用 stall 模型)
        }
    }
}

bool TileScheduler::all_done() const {
    if (!pending_tiles.empty()) return false;
    for (const auto& pe : pe_array) {
        if (!pe.is_idle()) return false;
    }
    return true;
}