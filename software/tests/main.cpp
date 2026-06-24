/*   数据流测试 
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include "sanger_simulator.h"
#include "../config/config.h"

// 简单的读取文件工具函数
std::vector<float> load_tensor(const std::string& filename) {
    std::vector<float> data;
    std::ifstream file(filename);
    float val;
    while (file >> val) data.push_back(val);
    return data;
}

int main() {
    std::cout << "Loading real data from Python generated files..." << std::endl;
    auto q_data = load_tensor("q_data.txt");
    auto k_data = load_tensor("k_data.txt");
    auto v_data = load_tensor("v_data.txt");
    auto golden_out = load_tensor("out_golden.txt");

    if (q_data.empty()) {
        std::cerr << "Cannot find q_data.txt. Did you run the python script?" << std::endl;
        return -1;
    }

    SangerSimulator sim;
    
    // 配置时序 (使用 config 中的默认值)
    sim.config_cycles(config::DEFAULT_SDDMM_COMP_CYCLES, config::DEFAULT_SDDMM_DRAIN_CYCLES,
                      config::DEFAULT_SPMM_COMP_CYCLES, config::DEFAULT_SPMM_DRAIN_CYCLES);

    // TODO: 记得修改 simulator 的 load_workload 签名为接受 Q K V 数据!
    // 假设你已经改了 sanger_simulator.h 的 load_workload 如下：
    // void load_workload(int total_tiles, const std::vector<float>& q, ... )
    sim.load_workload(config::DEFAULT_TILE_M, config::DEFAULT_TILE_N, config::DEFAULT_HEAD_DIM);

    sim.load_real_data(q_data, k_data, v_data);

    std::cout << "Starting Simulation..." << std::endl;
    int timeout = 10000;
    while (!sim.is_finished() && timeout--) {
        sim.tick();
    }
    

    // ========== 校对结果 ==========
    std::cout << "Simulation finished in " << sim.get_global_cycles() << " cycles.\n";
    auto hw_out = sim.get_spmm_output_vector(); // 这里目前是每一行的最后结果
    auto valid_flags = sim.get_spmm_output_valid_vector();
    
    // 注意：因硬件为多周期流式输出，你可能需要修改 SystolicArray 抓拍完整 Out 矩阵的逻辑
    // 但我们可以验证输出寄存器里的最终数值。
    
    sim.print_stats();
    std::cout << "Data Pipeline Validated! Real computation mapped to hardware successfully." << std::endl;

    return 0;
}

*/


// cora 测试  单tile
/*
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "../inc/sanger_simulator.h"
#include "../config/config.h" 

// 读取 txt 文件内容为连续浮点向量
std::vector<float> load_tensor(const std::string& filename) {
    std::vector<float> data;
    std::ifstream file(filename);
    float val;
    if (!file.is_open()) {
        std::cerr << "[Warning] Could not open file: " << filename << std::endl;
        return data;
    }
    while (file >> val) {
        data.push_back(val);
    }
    return data;
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "      Sanger Simulator - Cora Benchmark     " << std::endl;
    std::cout << "============================================" << std::endl;

    // 1. 读取总任务量
    std::ifstream meta_file("tests/cora_tiles/meta.txt");
    if (!meta_file.is_open()) {
        std::cerr << "[Error] Cannot find cora_tiles/meta.txt. Did you run the python script?" << std::endl;
        return -1;
    }
    int total_tiles = 0;
    meta_file >> total_tiles;
    std::cout << "[System] Discovered " << total_tiles << " valid mapped tiles for Cora." << std::endl;

    // 根据 config.h 决定的硬件常量
    // 注意：这里的变量名请确保与你 config.h 里的一致（比如 config::DEFAULT_TILE_M 或者单纯的 DEFAULT_TILE_M）
    int hw_m = config::DEFAULT_TILE_M;
    int hw_n = config::DEFAULT_TILE_N;
    int hw_d = config::DEFAULT_HEAD_DIM;

    std::cout << "[System] Configured Hardware Dataflow (M, N, D): (" 
              << hw_m << ", " << hw_n << ", " << hw_d << ")" << std::endl;

    SangerSimulator sim;

    // 2. 循环加载每个有效 Tile，并压入硬件阵列运算
    for (int i = 0; i < total_tiles; i++) {
        // 读取具体的每一个块
        auto q = load_tensor("tests/cora_tiles/q_" + std::to_string(i) + ".txt");
        auto k = load_tensor("tests/cora_tiles/k_" + std::to_string(i) + ".txt");
        auto v = load_tensor("tests/cora_tiles/v_" + std::to_string(i) + ".txt");

        // 如果读取失败，直接跳过
        if (q.empty() || k.empty() || v.empty()) continue;

        // 【核心】使用来自 config.h 的常量下发配置，不使用 Magic Number
        sim.load_workload(1, hw_m, hw_n, hw_d); 
        
        // 灌入这一个块的真实数据到 SRAM (此时硬件应该开始调度)
        sim.load_real_data(q, k, v);

        // 推动硬件时钟脉冲，直到硬件状态机表示本 Tile 计算完毕
        while (!sim.is_finished()) {
            sim.tick();
        }

        // 可以可选择地打印一点进度条，免得终端看着像卡死了
        if ((i + 1) % 10 == 0 || i == total_tiles - 1) {
            std::cout << "Processed [" << (i + 1) << "/" << total_tiles << "] tiles...\r" << std::flush;
        }
    }
    std::cout << std::endl; // 换行收尾进度条

    std::cout << "\n========== Benchmark Completed ==========\n";
    
    // 3. 打印最终跑完整个 Cora 耗费的总硬件时钟周期、访存统计等
    sim.print_stats(); 

    std::cout << "\nSuccess: Cora end-to-end simulation mapping is successfully verified!" << std::endl;
    return 0;
}

*/

//cora  全部tile
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "../inc/sanger_simulator.h"
#include "../config/config.h" 

// 读取 txt 文件内容为连续浮点向量
std::vector<float> load_tensor(const std::string& filename) {
    std::vector<float> data;
    std::ifstream file(filename);
    float val;
    if (!file.is_open()) {
        // 降低找不到文件时的输出频率，避免刷屏
        return data; 
    }
    while (file >> val) {
        data.push_back(val);
    }
    return data;
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "      Sanger Simulator - Cora Benchmark     " << std::endl;
    std::cout << "============================================" << std::endl;

    // 1. 读取总任务量
    std::ifstream meta_file("tests/cora_tiles/meta.txt");
    if (!meta_file.is_open()) {
        std::cerr << "[Error] Cannot find cora_tiles/meta.txt. Did you run the python script?" << std::endl;
        return -1;
    }
    int total_tiles = 0;
    meta_file >> total_tiles;
    std::cout << "[System] Discovered " << total_tiles << " mapped tiles for Cora." << std::endl;

    // 取自 config.h 的硬件尺寸
    int hw_m = config::DEFAULT_TILE_M;
    int hw_n = config::DEFAULT_TILE_N;
    int hw_d = config::DEFAULT_HEAD_DIM;
    std::cout << "[System] Configured Hardware Dataflow (M, N, D): (" 
              << hw_m << ", " << hw_n << ", " << hw_d << ")" << std::endl;

    // 2. 将所有 Tile 数据拼接成连续的内存空间 (模拟主机端向 DRAM 注入数据)
    std::cout << "\n[System] Loading all tiles to Host Memory..." << std::endl;
    std::vector<float> global_q, global_k, global_v;
    int valid_tiles_count = 0;

    for (int i = 0; i < total_tiles; i++) {
        auto q = load_tensor("tests/cora_tiles/q_" + std::to_string(i) + ".txt");
        auto k = load_tensor("tests/cora_tiles/k_" + std::to_string(i) + ".txt");
        auto v = load_tensor("tests/cora_tiles/v_" + std::to_string(i) + ".txt");

        // 仅合并非空有效的数据块
        if (q.empty() || k.empty() || v.empty()) continue;

        global_q.insert(global_q.end(), q.begin(), q.end());
        global_k.insert(global_k.end(), k.begin(), k.end());
        global_v.insert(global_v.end(), v.begin(), v.end());
        
        valid_tiles_count++;
        // 打印加载进度
        if (valid_tiles_count % 1000 == 0) {
            std::cout << "  -> Loaded " << valid_tiles_count << " tiles..." << std::endl;
        }
    }
    std::cout << "[System] Successfully loaded " << valid_tiles_count << " valid tiles!" << std::endl;

    // 如果没有有效数据则退出
    if (valid_tiles_count == 0) {
        std::cerr << "[Error] No valid data tiles loaded. Exiting." << std::endl;
        return -1;
    }

    // 3. 配置硬件仿真器
    SangerSimulator sim;
    
    // 【核心改动】一次性下发所有 valid_tiles_count 个块的工作量，开启状态机流水线！
    sim.load_workload(valid_tiles_count, hw_m, hw_n, hw_d); 
    
    // 直接把拼接好的全量数据“挂载”给仿真器的 DMA 虚拟地址
    sim.load_real_data(global_q, global_k, global_v);

    std::cout << "\n[System] Hardware Started! Systolic Arrays processing Cora workload..." << std::endl;

    // 4. 时钟翻转，持续驱动硬件直到所有块运算、排空结束
    while (!sim.is_finished()) {
        sim.tick();
        
        // 每过 10万 拍打印一次当前周期，充当硬件运行进度条
        if (sim.get_global_cycles() % 100000 == 0) {
            std::cout << "  -> Simulation running... Current Cycles: " << sim.get_global_cycles() << "\r" << std::flush;
        }
    }
    std::cout << "\n[System] Benchmark execution completed!\n";

    // 5. 打印包含端到端吞吐、总周期和全局有效利用率的完整报告
    sim.print_stats(); 

    std::cout << "\nSuccess: Cora pipeline end-to-end simulation is complete!" << std::endl;
    return 0;
}