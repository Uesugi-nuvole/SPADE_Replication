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