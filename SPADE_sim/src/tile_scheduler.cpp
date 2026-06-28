#include "../inc/tile_scheduler.h"
#include <iostream>
#include <algorithm>

TileScheduler::TileScheduler(std::vector<SpadePE>& pes) : pe_array(pes) {
    // 假设输出矩阵最多 10000 行，初始化锁
    row_locks.resize(100000, false); 
}

void TileScheduler::load_tiles(const std::vector<SpadeTile>& tiles) {
    for (const auto& t : tiles) {
        pending_tiles.push_back(t); // 修复 1：list 使用 push_back
    }
}

void TileScheduler::tick() {
    // 1. 解锁已经完成工作的 PE 所占用的行
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
            
            // 遍历未分配的 Tile 列表，寻找第一个不冲突的任务
            auto it = pending_tiles.begin();
            while (it != pending_tiles.end()) {
                bool barrier_hit = false;
                for(int i = it->row_start; i < it->row_start + it->row_num; ++i) {
                    if (row_locks[i]) { barrier_hit = true; break; }
                }

                if (!barrier_hit) {
                    // 找到不冲突的，分配给 PE
                    pe.assign_tile(*it);
                    
                    // 立即上锁
                    for(int i = it->row_start; i < it->row_start + it->row_num; ++i) {
                        row_locks[i] = true;
                    }
                    
                    pending_tiles.erase(it); // 从列表中移除该任务
                    break; // 这个 PE 有活干了，退出 while 去给下一个 PE 找活
                }
                
                ++it; // 如果冲突，往后看看有没有别的不冲突的行
            }
        }
    }
} // 修复 2：这里补上 tick() 函数缺失的右大括号！

bool TileScheduler::all_done() const {
    if (!pending_tiles.empty()) return false;
    for (const auto& pe : pe_array) {
        if (!pe.is_idle()) return false;
    }
    return true;
}