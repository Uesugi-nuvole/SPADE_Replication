//cache_sim.cpp
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
        SpadeTile tile{id, rs, rn, cs, cn, nnz, bcl, bcr, vtc};

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

//spade_silmulator.cpp
#include "../inc/spade_simulator.h"
#include <iostream>
#include <iomanip>

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
        if (pe.is_computing()) {
            active_pe_slots++;
    }
}
    global_cycles++;
}

bool SpadeSimulator::is_finished() const {
    return scheduler->all_done();
}


void SpadeSimulator::print_final_report() const {
    // 1. 利用率与工作情况计算
    // 理论上阵列全负荷能提供的 PE Slot 总数
    uint64_t total_pe_slots = global_cycles * config::NUM_PES; 
    double utilization = (total_pe_slots == 0) ? 0.0 : 
                         (static_cast<double>(active_pe_slots) / total_pe_slots) * 100.0;
                         
    // 2. 带宽体积计算
    // 总搬运体积 = 请求次数 * (每次请求传输的数据块大小)
    double dram_volume_mb = (cache.stats.dram_accesses * config::CACHE_LINE_SIZE) / (1024.0 * 1024.0);
    double l2_volume_mb   = (cache.stats.l2_hits * config::CACHE_LINE_SIZE) / (1024.0 * 1024.0);
    
    // 平均每周期完成的有效 MAC 运算数
    double avg_throughput = (static_cast<double>(active_pe_slots) * config::MACS_PER_PE) / (global_cycles == 0 ? 1 : global_cycles);

    std::cout << "\n===================================================\n";
    std::cout << "        SPADE Simulator Performance Report         \n";
    std::cout << "===================================================\n";
    std::cout << "[Architecture Configuration]\n";
    std::cout << "  PE Array Size       : " << config::NUM_PES << " PEs\n";
    std::cout << "  Physical MAC Width  : " << config::MACS_PER_PE << " MACs/PE (aligned with logic)\n";
    std::cout << "  Cache Line Size     : " << config::CACHE_LINE_SIZE << " Bytes\n\n";

    std::cout << "[Execution & Utilization]\n";
    std::cout << "  Total Clock Cycles  : " << global_cycles << "\n";
    std::cout << "  Active PE slots     : " << active_pe_slots << "\n";
    std::cout << "  Total PE slots      : " << total_pe_slots << "\n";
    std::cout << "  PE Utilization      : " << std::fixed << std::setprecision(2) << utilization << " %\n";
    std::cout << "  Avg Throughput      : " << std::fixed << std::setprecision(2) << avg_throughput << " MACs/clk\n\n";

    std::cout << "[Memory Hierarchy Network Traffic]\n";
    std::cout << "  Victim Cache Hits   : " << cache.stats.victim_hits << " (Bypass intercept)\n";
    std::cout << "  L2 SRAM Hits        : " << cache.stats.l2_hits << " (Traffic: " << std::fixed << std::setprecision(3) << l2_volume_mb << " MB)\n";
    std::cout << "  DRAM Accesses       : " << cache.stats.dram_accesses << " (Traffic: " << std::fixed << std::setprecision(3) << dram_volume_mb << " MB)\n";
    std::cout << "===================================================\n";
}

//tile_schedulor.cpp
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