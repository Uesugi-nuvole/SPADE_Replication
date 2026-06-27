#pragma once
#include <vector>
#include <queue>
#include "spade_pe.h"

class TileScheduler {
public:
    TileScheduler(std::vector<SpadePE>& pes);
    ~TileScheduler() = default;

    // 加载 Python 生成的离线 Tile 元数据
    void load_tiles(const std::vector<SpadeTile>& tiles);
    
    // 每个周期的调度逻辑 (分发 Task, 检查 Barrier)
    void tick();
    
    bool all_done() const;

private:
    std::vector<SpadePE>& pe_array;
    std::queue<SpadeTile> pending_tiles;
    
    // 用于 SpMM 写回冲突的 Barrier 控制 (防止多个 PE 竞争写同一行)
    std::vector<bool> row_locks; 
};

