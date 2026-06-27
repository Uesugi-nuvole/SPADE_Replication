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