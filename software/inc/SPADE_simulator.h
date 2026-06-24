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
