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