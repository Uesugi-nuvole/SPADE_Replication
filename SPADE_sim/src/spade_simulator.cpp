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