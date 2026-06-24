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
