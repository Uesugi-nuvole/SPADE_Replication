#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "fifo.h"
#include "scheduler.h"
#include "../config/config.h"

struct VirtualSRAM {
    std::string name;
    std::size_t capacity;
    std::size_t used;

    VirtualSRAM(const std::string& n, std::size_t c)
        : name(n), capacity(c), used(0) {}

    bool allocate(std::size_t size) {
        if (used + size > capacity) {
            return false;
        }
        used += size;
        return true;
    }

    void free_all() {
        used = 0;
    }
};

class SRAM_Buffer {
private:
    double dram_bandwidth_gbps;
    int current_freq_mhz;
    double bytes_per_cycle;

    long long dram_reads_bytes;
    long long sram_reads_bytes;
    long long sram_writes_bytes;

    VirtualSRAM query_sram;
    VirtualSRAM key_sram;
    VirtualSRAM value_sram;
    VirtualSRAM output_sram;

    int tile_m;
    int tile_n;
    int head_dim;

    bool current_dma_busy;
    bool next_dma_busy;
    int current_dma_timer;
    int next_dma_timer;
    int current_ready_tiles;
    int next_ready_tiles;
    int current_pending_tiles;
    int next_pending_tiles;

    int fetch_tile_latency();

    // 真实数据
    std::vector<float> real_q_data;
    std::vector<float> real_k_data;
    std::vector<float> real_v_data;
    
    std::size_t q_rd_ptr;
    std::size_t k_rd_ptr;
    std::size_t v_rd_ptr;

public:
    SRAM_Buffer(double bandwidth_gbps = config::DRAM_BANDWIDTH_GBPS, 
                int freq_mhz = config::SYS_FREQ_MHZ);

    void config_tile(int m, int n, int d);
    void set_workload(int total_tiles);

    void evaluate(const CtrlSignal& ctrl,
                  SchedState sched_state,
                  FIFO& fifo_q,
                  FIFO& fifo_k,
                  FIFO& fifo_v);

    void update();

    bool has_ready_tiles() const;
    void consume_tile();
    void print_report() const;

    // 在 public: 里面新增一个载入真实数据的接口
    void load_real_data(const std::vector<float>& q, 
                        const std::vector<float>& k, 
                        const std::vector<float>& v);
};
