#include "sram_buffer.h"

#include <algorithm>
#include <cmath>
#include "../config/config.h" // 引入config

SRAM_Buffer::SRAM_Buffer(double bandwidth_gbps, int freq_mhz)
    : dram_bandwidth_gbps(bandwidth_gbps), current_freq_mhz(freq_mhz),
      bytes_per_cycle(0.0),
      dram_reads_bytes(0), sram_reads_bytes(0), sram_writes_bytes(0),
      // 1.\/ 换成 config 里的宏
      query_sram("Query_Buffer", config::SRAM_CAPACITY_QUERY),
      key_sram("Key_Buffer", config::SRAM_CAPACITY_KEY),
      value_sram("Value_Buffer", config::SRAM_CAPACITY_VALUE),
      output_sram("Output_Buffer", config::SRAM_CAPACITY_OUTPUT),
      tile_m(config::DEFAULT_TILE_M), 
      tile_n(config::DEFAULT_TILE_N), 
      head_dim(config::DEFAULT_HEAD_DIM),
      current_dma_busy(false), next_dma_busy(false),
      current_dma_timer(0), next_dma_timer(0),
      current_ready_tiles(0), next_ready_tiles(0),
      current_pending_tiles(0), next_pending_tiles(0) {
    bytes_per_cycle = (dram_bandwidth_gbps * 1024.0 * 1024.0 * 1024.0) /
                      (static_cast<double>(current_freq_mhz) * 1000.0 * 1000.0);
}


// 2. 实现刚才在 .h 里声明的函数  真实数据
void SRAM_Buffer::load_real_data(const std::vector<float>& q, 
                                 const std::vector<float>& k, 
                                 const std::vector<float>& v) {
    real_q_data = q;
    real_k_data = k;
    real_v_data = v;
    q_rd_ptr = 0;
    k_rd_ptr = 0;
    v_rd_ptr = 0;
}

void SRAM_Buffer::config_tile(int m, int n, int d) {
    tile_m = m;
    tile_n = n;
    head_dim = d;
}

void SRAM_Buffer::set_workload(int total_tiles) {
    current_dma_busy = false;
    next_dma_busy = false;
    current_dma_timer = 0;
    next_dma_timer = 0;
    current_ready_tiles = 0;
    next_ready_tiles = 0;
    current_pending_tiles = total_tiles;
    next_pending_tiles = total_tiles;

    query_sram.free_all();
    key_sram.free_all();
    value_sram.free_all();
    output_sram.free_all();

    dram_reads_bytes = 0;
    sram_reads_bytes = 0;
    sram_writes_bytes = 0;
}

int SRAM_Buffer::fetch_tile_latency() {
    const std::size_t bytes_q = static_cast<std::size_t>(tile_m) * head_dim * sizeof(float);
    const std::size_t bytes_k = static_cast<std::size_t>(tile_n) * head_dim * sizeof(float);
    const std::size_t bytes_v = static_cast<std::size_t>(tile_n) * head_dim * sizeof(float);

    if (!query_sram.allocate(bytes_q) ||
        !key_sram.allocate(bytes_k) ||
        !value_sram.allocate(bytes_v)) {
        std::cerr << "[Memory Error] On-chip SRAM overflow. Tile is too large.\n";
    }

    const std::size_t total_bytes = bytes_q + bytes_k + bytes_v;
    dram_reads_bytes += static_cast<long long>(total_bytes);
    sram_writes_bytes += static_cast<long long>(total_bytes);

    const int cycles = static_cast<int>(std::ceil(static_cast<double>(total_bytes) / bytes_per_cycle));
    return std::max(cycles, 1);
}

// 3. 修改 evaluate 里的 push 逻辑  不能只送1.0f了
void SRAM_Buffer::evaluate(const CtrlSignal& ctrl,
                           SchedState sched_state,
                           FIFO& fifo_q,
                           FIFO& fifo_k,
                           FIFO& fifo_v) {
    next_dma_busy = current_dma_busy;
    next_dma_timer = current_dma_timer;
    next_ready_tiles = current_ready_tiles;
    next_pending_tiles = current_pending_tiles;

    if (!current_dma_busy && current_pending_tiles > 0) {
        next_dma_busy = true;
        next_dma_timer = fetch_tile_latency();
        next_pending_tiles = current_pending_tiles - 1;
    }

    if (current_dma_busy) {
        next_dma_timer = current_dma_timer - 1;
        if (next_dma_timer <= 0) {
            next_dma_busy = false;
            next_ready_tiles = current_ready_tiles + 1;
        }
    }

    if (!ctrl.array_enable) {
        return;
    }

    if (sched_state == SchedState::SDDMM_COMPUTE && ctrl.array_mode == Mode::SDDMM) {
         // SDDMM模式：阵列左侧每周期吃 M 个 Q，上侧吃 N 个 K
        for (int r = 0; r < tile_m; ++r) {
            float val = (q_rd_ptr < real_q_data.size()) ? real_q_data[q_rd_ptr++] : 0.0f;
            fifo_q.push(val);
        }
        for (int c = 0; c < tile_n; ++c) {
            float val = (k_rd_ptr < real_k_data.size()) ? real_k_data[k_rd_ptr++] : 0.0f;
            fifo_k.push(val);
        }
        sram_reads_bytes += static_cast<long long>((tile_m + tile_n) * sizeof(float));
    } else if (sched_state == SchedState::SPMM_COMPUTE && ctrl.array_mode == Mode::SPMM) {
        // SPMM模式：阵列上侧吃 V
        for (int c = 0; c < tile_n; ++c) {
            float val = (v_rd_ptr < real_v_data.size()) ? real_v_data[v_rd_ptr++] : 0.0f;
            fifo_v.push(val);
        }
        sram_reads_bytes += static_cast<long long>(tile_n * sizeof(float));
    }
}

void SRAM_Buffer::update() {
    current_dma_busy = next_dma_busy;
    current_dma_timer = next_dma_timer;
    current_ready_tiles = next_ready_tiles;
    current_pending_tiles = next_pending_tiles;
}

bool SRAM_Buffer::has_ready_tiles() const {
    return current_ready_tiles > 0;
}

void SRAM_Buffer::consume_tile() {
    if (current_ready_tiles > 0) {
        --current_ready_tiles;
    }
    next_ready_tiles = current_ready_tiles;

    query_sram.free_all();
    key_sram.free_all();
    value_sram.free_all();
}

void SRAM_Buffer::print_report() const {
    std::cout << "\n============================================\n";
    std::cout << "        Sanger SRAM Memory Report\n";
    std::cout << "============================================\n";
    std::cout << "Tile Config (M,N,D): " << tile_m << ", " << tile_n << ", " << head_dim << "\n";
    std::cout << "Total DRAM Read (MB):  " << static_cast<double>(dram_reads_bytes) / (1024.0 * 1024.0) << "\n";
    std::cout << "Total SRAM Read (MB):  " << static_cast<double>(sram_reads_bytes) / (1024.0 * 1024.0) << "\n";
    std::cout << "Total SRAM Write (MB): " << static_cast<double>(sram_writes_bytes) / (1024.0 * 1024.0) << "\n";
    std::cout << "============================================\n";
}
