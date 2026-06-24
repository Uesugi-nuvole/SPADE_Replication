//fifo.h
#pragma once

#include <cstddef>
#include <queue>
#include "../config/config.h"

class FIFO {
private:
    std::queue<float> current_q;
    mutable std::queue<float> eval_read_q;
    mutable std::queue<float> next_q;
    mutable bool eval_prepared;

    void prepare_evaluate() const;
    static void clear_queue(std::queue<float>& q);

public:
    FIFO();

    void push(float value);
    bool empty() const;
    std::size_t size() const;
    float front() const;
    void pop();
    void clear();
    void update();
};


//pe.h
#pragma once

#include "scheduler.h"
#include "../config/config.h"

class RePE {
private:
    float reg_in_a;
    float reg_in_b;
    float reg_in_spatial;

    float reg_score;
    float reg_out;

    float next_score;
    float next_out;

    Mode mode;

public:
    RePE();

    void set_mode(Mode new_mode);
    void load_inputs(float in_a, float in_b, float in_spatial = 0.0f);

    void evaluate(float wire_a, float wire_b, float wire_spatial = 0.0f);
    void update();

    void clear_score();
    void clear_out();
    void clear_all();

    float get_score() const;
    float get_out() const;
    float get_reg_in_a() const;
    float get_reg_in_b() const;
    float get_reg_in_spatial() const;
};


//sanger_simulaltor.h

#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

#include "fifo.h"
#include "scheduler.h"
#include "sram_buffer.h"
#include "systolic_array.h"
#include "../config/config.h"

class SangerSimulator {
private:
    Scheduler scheduler;
    SRAM_Buffer sram;
    SystolicArray array;

    FIFO fifo_q_left;
    FIFO fifo_k_top;
    FIFO fifo_v_top;

    std::uint64_t global_cycles;

public:
    SangerSimulator()
        : scheduler(), sram(), array(),
          fifo_q_left(), fifo_k_top(), fifo_v_top(),
          global_cycles(0) {}

    void config_cycles(int sddmm_comp,
                       int sddmm_drain,
                       int spmm_comp,
                       int spmm_drain) {
        scheduler.config_cycles(sddmm_comp, sddmm_drain, spmm_comp, spmm_drain);
    }

    
    // 【修改这里！】让外面的 main.cpp 能把 vector 传进来
    void load_real_data(const std::vector<float>& q_data, 
                        const std::vector<float>& k_data, 
                        const std::vector<float>& v_data) 
    {
        sram.load_real_data(q_data, k_data, v_data);
    }

    void load_workload(int total_tiles,
                       int tile_m   =  config::DEFAULT_TILE_M,
                       int tile_n   =  config::DEFAULT_TILE_N,
                       int head_dim =  config::DEFAULT_HEAD_DIM) {
        global_cycles = 0;
        scheduler.set_workload(total_tiles);
        sram.config_tile(tile_m, tile_n, head_dim);
        sram.set_workload(total_tiles);


        array.clear_all();
        fifo_q_left.clear();
        fifo_k_top.clear();
        fifo_v_top.clear();

        fifo_q_left.update();
        fifo_k_top.update();
        fifo_v_top.update();
    }

    void tick() {
        const bool sram_ready = sram.has_ready_tiles();

        scheduler.evaluate(sram_ready);

        const CtrlSignal ctrl = scheduler.get_ctrl_signals();
        const SchedState sched_state = scheduler.get_state();

        if (ctrl.array_clear && ctrl.array_enable && ctrl.array_mode == Mode::SDDMM) {
            sram.consume_tile();
        }

        sram.evaluate(ctrl, sched_state, fifo_q_left, fifo_k_top, fifo_v_top);
        array.evaluate(ctrl, &fifo_q_left, &fifo_k_top, &fifo_v_top);

        scheduler.update();
        sram.update();
        array.update();
        fifo_q_left.update();
        fifo_k_top.update();
        fifo_v_top.update();

        ++global_cycles;
    }

    bool is_finished() const {
        return scheduler.is_done();
    }

    std::uint64_t get_global_cycles() const {
        return global_cycles;
    }

    const SystolicArray& get_array() const {
        return array;
    }

    SystolicArray& get_array() {
        return array;
    }

    std::vector<std::vector<float>> get_score_matrix() const {
        return array.get_score_matrix();
    }

    std::vector<float> get_spmm_output_vector() const {
        return array.get_spmm_output_vector();
    }

    std::vector<bool> get_spmm_output_valid_vector() const {
        return array.get_spmm_output_valid_vector();
    }

    void print_stats() const {
        std::cout << "\n============================================\n";
        std::cout << "        Sanger Simulator Report\n";
        std::cout << "============================================\n";
        std::cout << "Total cycles:     " << global_cycles << "\n";
        std::cout << "Active PE slots:  " << array.get_active_pe_count() << "\n";
        std::cout << "Total PE slots:   " << array.get_total_pe_slots() << "\n";
        std::cout << "PE utilization:   " << array.get_utilization() * 100.0 << "%\n";
        std::cout << "============================================\n";
        sram.print_report();
    }
};

//scheduler.h
#pragma once

#include <algorithm>

#include "../config/config.h"

// Control mode of the systolic array.
enum class Mode {
    IDLE,
    SDDMM,
    SPMM
};

struct CtrlSignal {
    bool array_enable = false;
    Mode array_mode = Mode::IDLE;
    bool array_clear = false;
    bool array_clear_datapath = false;
};

enum class SchedState {
    IDLE,
    SDDMM_COMPUTE,
    SDDMM_DRAIN,
    SPMM_COMPUTE,
    SPMM_DRAIN
};

class Scheduler {
private:
    SchedState current_state;
    SchedState next_state;

    int state_timer;
    int next_state_timer;

    int remaining_tiles;
    int next_remaining_tiles;

    CtrlSignal current_ctrl;
    CtrlSignal next_ctrl;

    int cycles_sddmm_comp;
    int cycles_sddmm_drain;
    int cycles_spmm_comp;
    int cycles_spmm_drain;

public:
    Scheduler(int s_comp  =  config::DEFAULT_SDDMM_COMP_CYCLES, 
              int s_drain =  config::DEFAULT_SDDMM_DRAIN_CYCLES,
              int p_comp  =  config::DEFAULT_SPMM_COMP_CYCLES, 
              int p_drain =  config::DEFAULT_SPMM_DRAIN_CYCLES);

    void config_cycles(int s_comp, int s_drain, int p_comp, int p_drain);
    void set_workload(int total_tiles);

    void evaluate(bool sram_data_ready);
    void update();

    // Returns the combinational control signal generated in evaluate().
    CtrlSignal get_ctrl_signals() const;
    SchedState get_state() const;
    bool is_done() const;
};

//sram_buffer.h

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

//sysytolic_array.h
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "fifo.h"
#include "pe.h"
#include "scheduler.h"
#include "../config/config.h"

class SystolicArray {
public:
    static constexpr int ROWS = config::ARRAY_ROWS;
    static constexpr int COLS = config::ARRAY_COLS;
    static constexpr int N = config::DEFAULT_TILE_N;

private:
    std::array<std::array<RePE, COLS>, ROWS> pes;

    float reg_a_pipe[ROWS][COLS];
    float reg_b_pipe[ROWS][COLS];
    float reg_spatial_pipe[ROWS][COLS];

    float next_a_pipe[ROWS][COLS];
    float next_b_pipe[ROWS][COLS];
    float next_spatial_pipe[ROWS][COLS];

    float reg_final_out[ROWS];
    float next_final_out[ROWS];

    bool reg_final_valid[ROWS];
    bool next_final_valid[ROWS];

    int spmm_cycle;
    int next_spmm_cycle;

    Mode current_mode;
    Mode next_mode;

    std::uint64_t active_pe_count;
    std::uint64_t total_pe_slots;

public:
    SystolicArray();

    void clear_all();
    void clear_datapath_keep_score();

    void set_mode(Mode mode);
    Mode get_mode() const;

    void evaluate(const CtrlSignal& ctrl,
                  FIFO* fifo_q_left,
                  FIFO* fifo_k_top,
                  FIFO* fifo_v_top);

    void evaluate_direct(Mode mode,
                         FIFO* fifo_q_left,
                         FIFO* fifo_k_top,
                         FIFO* fifo_v_top);

    void update();

    std::vector<std::vector<float>> get_score_matrix() const;
    std::vector<float> get_spmm_output_vector() const;
    std::vector<bool> get_spmm_output_valid_vector() const;
    bool all_spmm_outputs_valid() const;

    float get_pe_score(int row, int col) const;
    float get_pe_out(int row, int col) const;

    std::uint64_t get_active_pe_count() const;
    std::uint64_t get_total_pe_slots() const;
    double get_utilization() const;

private:
    void preserve_output_buffer_next();

    void evaluate_idle();
    void evaluate_clear_all();
    void evaluate_clear_datapath_keep_score();
    void evaluate_sddmm(FIFO* fifo_q_left, FIFO* fifo_k_top);
    void evaluate_spmm(FIFO* fifo_v_top);

    float read_fifo_or_zero(FIFO* fifo);
    void check_index(int row, int col) const;
};
