//fifo.cpp
#include "fifo.h"

#include <stdexcept>
#include <utility>

FIFO::FIFO()
    : eval_prepared(false) {}

void FIFO::clear_queue(std::queue<float>& q) {
    std::queue<float> empty;
    std::swap(q, empty);
}

void FIFO::prepare_evaluate() const {
    if (!eval_prepared) {
        eval_read_q = current_q;
        next_q = current_q;
        eval_prepared = true;
    }
}

void FIFO::push(float value) {
    prepare_evaluate();
    next_q.push(value);
}

bool FIFO::empty() const {
    prepare_evaluate();
    return eval_read_q.empty();
}

std::size_t FIFO::size() const {
    prepare_evaluate();
    return eval_read_q.size();
}

float FIFO::front() const {
    prepare_evaluate();
    if (eval_read_q.empty()) {
        throw std::runtime_error("FIFO front() on empty FIFO");
    }
    return eval_read_q.front();
}

void FIFO::pop() {
    prepare_evaluate();
    if (eval_read_q.empty()) {
        throw std::runtime_error("FIFO pop() on empty FIFO");
    }
    eval_read_q.pop();
    next_q.pop();
}

void FIFO::clear() {
    prepare_evaluate();
    clear_queue(eval_read_q);
    clear_queue(next_q);
}

void FIFO::update() {
    prepare_evaluate();
    current_q = next_q;
    clear_queue(eval_read_q);
    clear_queue(next_q);
    eval_prepared = false;
}

//pe.cpp
#include "pe.h"

#include <stdexcept>

RePE::RePE()
    : reg_in_a(0.0f), reg_in_b(0.0f), reg_in_spatial(0.0f),
      reg_score(0.0f), reg_out(0.0f),
      next_score(0.0f), next_out(0.0f),
      mode(Mode::IDLE) {}

void RePE::set_mode(Mode new_mode) {
    mode = new_mode;
}

void RePE::load_inputs(float in_a, float in_b, float in_spatial) {
    reg_in_a = in_a;
    reg_in_b = in_b;
    reg_in_spatial = in_spatial;
}

void RePE::evaluate(float wire_a, float wire_b, float wire_spatial) {
    next_score = reg_score;
    next_out = reg_out;

    switch (mode) {
    case Mode::IDLE:
        break;
    case Mode::SDDMM:
        next_score = reg_score + wire_a * wire_b;
        break;
    case Mode::SPMM:
        next_out = wire_spatial + reg_score * wire_b;
        break;
    default:
        throw std::runtime_error("Unknown RePE mode");
    }
}

void RePE::update() {
    reg_score = next_score;
    reg_out = next_out;
}

void RePE::clear_score() {
    reg_score = 0.0f;
    next_score = 0.0f;
}

void RePE::clear_out() {
    reg_out = 0.0f;
    next_out = 0.0f;
}

void RePE::clear_all() {
    reg_in_a = 0.0f;
    reg_in_b = 0.0f;
    reg_in_spatial = 0.0f;
    reg_score = 0.0f;
    reg_out = 0.0f;
    next_score = 0.0f;
    next_out = 0.0f;
    mode = Mode::IDLE;
}

float RePE::get_score() const { return reg_score; }
float RePE::get_out() const { return reg_out; }
float RePE::get_reg_in_a() const { return reg_in_a; }
float RePE::get_reg_in_b() const { return reg_in_b; }
float RePE::get_reg_in_spatial() const { return reg_in_spatial; }

//scheduler.cpp
#include "scheduler.h"

Scheduler::Scheduler(int s_comp, int s_drain, int p_comp, int p_drain)
    : current_state(SchedState::IDLE), next_state(SchedState::IDLE),
      state_timer(0), next_state_timer(0),
      remaining_tiles(0), next_remaining_tiles(0),
      current_ctrl(), next_ctrl(),
      cycles_sddmm_comp(s_comp), cycles_sddmm_drain(s_drain),
      cycles_spmm_comp(p_comp), cycles_spmm_drain(p_drain) {}

void Scheduler::config_cycles(int s_comp, int s_drain, int p_comp, int p_drain) {
    cycles_sddmm_comp = s_comp;
    cycles_sddmm_drain = s_drain;
    cycles_spmm_comp = p_comp;
    cycles_spmm_drain = p_drain;
}

void Scheduler::set_workload(int total_tiles) {
    remaining_tiles = total_tiles;
    next_remaining_tiles = total_tiles;
    current_state = SchedState::IDLE;
    next_state = SchedState::IDLE;
    state_timer = 0;
    next_state_timer = 0;
    current_ctrl = CtrlSignal{};
    next_ctrl = CtrlSignal{};
}

void Scheduler::evaluate(bool sram_data_ready) {
    next_state = current_state;
    next_state_timer = state_timer + 1;
    next_remaining_tiles = remaining_tiles;

    next_ctrl = current_ctrl;
    next_ctrl.array_clear = false;
    next_ctrl.array_clear_datapath = false;

    switch (current_state) {
    case SchedState::IDLE:
        next_ctrl.array_enable = false;
        next_ctrl.array_mode = Mode::IDLE;
        next_state_timer = 0;

        if (remaining_tiles > 0 && sram_data_ready) {
            next_state = SchedState::SDDMM_COMPUTE;
            next_state_timer = -1;
            next_ctrl.array_enable = true;
            next_ctrl.array_mode = Mode::SDDMM;
            next_ctrl.array_clear = true;
        }
        break;

    case SchedState::SDDMM_COMPUTE:
        next_ctrl.array_enable = true;
        next_ctrl.array_mode = Mode::SDDMM;
        if (state_timer >= cycles_sddmm_comp - 1) {
            next_state = SchedState::SDDMM_DRAIN;
            next_state_timer = 0;
        }
        break;

    case SchedState::SDDMM_DRAIN:
        next_ctrl.array_enable = true;
        next_ctrl.array_mode = Mode::SDDMM;
        if (state_timer >= cycles_sddmm_drain - 1) {
            next_state = SchedState::SPMM_COMPUTE;
            next_state_timer = -1;
            next_ctrl.array_enable = true;
            next_ctrl.array_mode = Mode::SPMM;
            next_ctrl.array_clear_datapath = true;
        }
        break;

    case SchedState::SPMM_COMPUTE:
        next_ctrl.array_enable = true;
        next_ctrl.array_mode = Mode::SPMM;
        if (state_timer >= cycles_spmm_comp - 1) {
            next_state = SchedState::SPMM_DRAIN;
            next_state_timer = 0;
        }
        break;

    case SchedState::SPMM_DRAIN:
        next_ctrl.array_enable = true;
        next_ctrl.array_mode = Mode::SPMM;
        if (state_timer >= cycles_spmm_drain - 1) {
            next_remaining_tiles = std::max(remaining_tiles - 1, 0);
            next_state_timer = 0;

            if (next_remaining_tiles > 0 && sram_data_ready) {
                next_state = SchedState::SDDMM_COMPUTE;
                next_state_timer = -1;
                next_ctrl.array_enable = true;
                next_ctrl.array_mode = Mode::SDDMM;
                next_ctrl.array_clear = true;
            } else {
                next_state = SchedState::IDLE;
                next_ctrl.array_enable = false;
                next_ctrl.array_mode = Mode::IDLE;
            }
        }
        break;
    }
}

void Scheduler::update() {
    current_state = next_state;
    state_timer = next_state_timer;
    remaining_tiles = next_remaining_tiles;
    current_ctrl = next_ctrl;
}

CtrlSignal Scheduler::get_ctrl_signals() const {
    return next_ctrl;
}

SchedState Scheduler::get_state() const {
    return current_state;
}

bool Scheduler::is_done() const {
    return remaining_tiles == 0 && current_state == SchedState::IDLE;
}

//sram_buffer.cpp

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

//systolic_array.cpp
#include "systolic_array.h"

#include <stdexcept>

SystolicArray::SystolicArray()
    : spmm_cycle(0), next_spmm_cycle(0),
      current_mode(Mode::IDLE), next_mode(Mode::IDLE),
      active_pe_count(0), total_pe_slots(0) {
    clear_all();
}

void SystolicArray::clear_all() {
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].clear_all();
            reg_a_pipe[i][j] = 0.0f;
            reg_b_pipe[i][j] = 0.0f;
            reg_spatial_pipe[i][j] = 0.0f;
            next_a_pipe[i][j] = 0.0f;
            next_b_pipe[i][j] = 0.0f;
            next_spatial_pipe[i][j] = 0.0f;
        }
    }

    for (int i = 0; i < ROWS; ++i) {
        reg_final_out[i] = 0.0f;
        next_final_out[i] = 0.0f;
        reg_final_valid[i] = false;
        next_final_valid[i] = false;
    }

    spmm_cycle = 0;
    next_spmm_cycle = 0;
    current_mode = Mode::IDLE;
    next_mode = Mode::IDLE;
    active_pe_count = 0;
    total_pe_slots = 0;
}

void SystolicArray::clear_datapath_keep_score() {
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].clear_out();
            reg_a_pipe[i][j] = 0.0f;
            reg_b_pipe[i][j] = 0.0f;
            reg_spatial_pipe[i][j] = 0.0f;
            next_a_pipe[i][j] = 0.0f;
            next_b_pipe[i][j] = 0.0f;
            next_spatial_pipe[i][j] = 0.0f;
        }
    }

    for (int i = 0; i < ROWS; ++i) {
        reg_final_out[i] = 0.0f;
        next_final_out[i] = 0.0f;
        reg_final_valid[i] = false;
        next_final_valid[i] = false;
    }

    spmm_cycle = 0;
    next_spmm_cycle = 0;
}

void SystolicArray::set_mode(Mode mode) {
    next_mode = mode;
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].set_mode(mode);
        }
    }
}

Mode SystolicArray::get_mode() const {
    return current_mode;
}

void SystolicArray::evaluate(const CtrlSignal& ctrl,
                             FIFO* fifo_q_left,
                             FIFO* fifo_k_top,
                             FIFO* fifo_v_top) {
    if (ctrl.array_clear) {
        evaluate_clear_all();
        return;
    }

    if (ctrl.array_clear_datapath) {
        evaluate_clear_datapath_keep_score();
        return;
    }

    if (!ctrl.array_enable) {
        evaluate_idle();
        return;
    }

    if (ctrl.array_mode == Mode::SDDMM) {
        evaluate_sddmm(fifo_q_left, fifo_k_top);
    } else if (ctrl.array_mode == Mode::SPMM) {
        evaluate_spmm(fifo_v_top);
    } else {
        evaluate_idle();
    }
}

void SystolicArray::evaluate_direct(Mode mode,
                                    FIFO* fifo_q_left,
                                    FIFO* fifo_k_top,
                                    FIFO* fifo_v_top) {
    if (mode == Mode::SDDMM) {
        evaluate_sddmm(fifo_q_left, fifo_k_top);
    } else if (mode == Mode::SPMM) {
        evaluate_spmm(fifo_v_top);
    } else {
        evaluate_idle();
    }
}

void SystolicArray::update() {
    current_mode = next_mode;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].update();
            reg_a_pipe[i][j] = next_a_pipe[i][j];
            reg_b_pipe[i][j] = next_b_pipe[i][j];
            reg_spatial_pipe[i][j] = next_spatial_pipe[i][j];
        }
    }

    for (int i = 0; i < ROWS; ++i) {
        reg_final_out[i] = next_final_out[i];
        reg_final_valid[i] = next_final_valid[i];
    }

    spmm_cycle = next_spmm_cycle;
}

std::vector<std::vector<float>> SystolicArray::get_score_matrix() const {
    std::vector<std::vector<float>> score(ROWS, std::vector<float>(COLS, 0.0f));
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            score[i][j] = pes[i][j].get_score();
        }
    }
    return score;
}

std::vector<float> SystolicArray::get_spmm_output_vector() const {
    std::vector<float> out(ROWS, 0.0f);
    for (int i = 0; i < ROWS; ++i) {
        out[i] = reg_final_out[i];
    }
    return out;
}

std::vector<bool> SystolicArray::get_spmm_output_valid_vector() const {
    std::vector<bool> valid(ROWS, false);
    for (int i = 0; i < ROWS; ++i) {
        valid[i] = reg_final_valid[i];
    }
    return valid;
}

bool SystolicArray::all_spmm_outputs_valid() const {
    for (int i = 0; i < ROWS; ++i) {
        if (!reg_final_valid[i]) {
            return false;
        }
    }
    return true;
}

float SystolicArray::get_pe_score(int row, int col) const {
    check_index(row, col);
    return pes[row][col].get_score();
}

float SystolicArray::get_pe_out(int row, int col) const {
    check_index(row, col);
    return pes[row][col].get_out();
}

std::uint64_t SystolicArray::get_active_pe_count() const {
    return active_pe_count;
}

std::uint64_t SystolicArray::get_total_pe_slots() const {
    return total_pe_slots;
}

double SystolicArray::get_utilization() const {
    if (total_pe_slots == 0) {
        return 0.0;
    }
    return static_cast<double>(active_pe_count) / static_cast<double>(total_pe_slots);
}

void SystolicArray::preserve_output_buffer_next() {
    for (int i = 0; i < ROWS; ++i) {
        next_final_out[i] = reg_final_out[i];
        next_final_valid[i] = reg_final_valid[i];
    }
    next_spmm_cycle = spmm_cycle;
}

void SystolicArray::evaluate_idle() {
    next_mode = Mode::IDLE;
    preserve_output_buffer_next();

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].set_mode(Mode::IDLE);
            pes[i][j].evaluate(0.0f, 0.0f, 0.0f);
            next_a_pipe[i][j] = reg_a_pipe[i][j];
            next_b_pipe[i][j] = reg_b_pipe[i][j];
            next_spatial_pipe[i][j] = reg_spatial_pipe[i][j];
        }
    }
}

void SystolicArray::evaluate_clear_all() {
    next_mode = Mode::IDLE;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].clear_all();
            reg_a_pipe[i][j] = 0.0f;
            reg_b_pipe[i][j] = 0.0f;
            reg_spatial_pipe[i][j] = 0.0f;
            next_a_pipe[i][j] = 0.0f;
            next_b_pipe[i][j] = 0.0f;
            next_spatial_pipe[i][j] = 0.0f;
        }
    }

    for (int i = 0; i < ROWS; ++i) {
        reg_final_out[i] = 0.0f;
        next_final_out[i] = 0.0f;
        reg_final_valid[i] = false;
        next_final_valid[i] = false;
    }

    spmm_cycle = 0;
    next_spmm_cycle = 0;
}

void SystolicArray::evaluate_clear_datapath_keep_score() {
    next_mode = Mode::IDLE;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].clear_out();
            reg_a_pipe[i][j] = 0.0f;
            reg_b_pipe[i][j] = 0.0f;
            reg_spatial_pipe[i][j] = 0.0f;
            next_a_pipe[i][j] = 0.0f;
            next_b_pipe[i][j] = 0.0f;
            next_spatial_pipe[i][j] = 0.0f;
        }
    }

    for (int i = 0; i < ROWS; ++i) {
        reg_final_out[i] = 0.0f;
        next_final_out[i] = 0.0f;
        reg_final_valid[i] = false;
        next_final_valid[i] = false;
    }

    spmm_cycle = 0;
    next_spmm_cycle = 0;
}

void SystolicArray::evaluate_sddmm(FIFO* fifo_q_left, FIFO* fifo_k_top) {
    next_mode = Mode::SDDMM;
    preserve_output_buffer_next();

    total_pe_slots += ROWS * COLS;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].set_mode(Mode::SDDMM);

            float wire_a = 0.0f;
            float wire_b = 0.0f;

            if (j == 0) {
                wire_a = read_fifo_or_zero(fifo_q_left);
            } else {
                wire_a = reg_a_pipe[i][j - 1];
            }

            if (i == 0) {
                wire_b = read_fifo_or_zero(fifo_k_top);
            } else {
                wire_b = reg_b_pipe[i - 1][j];
            }

            pes[i][j].evaluate(wire_a, wire_b, 0.0f);

            next_a_pipe[i][j] = wire_a;
            next_b_pipe[i][j] = wire_b;
            next_spatial_pipe[i][j] = 0.0f;

            if (wire_a != 0.0f && wire_b != 0.0f) {
                ++active_pe_count;
            }
        }
    }
}

void SystolicArray::evaluate_spmm(FIFO* fifo_v_top) {
    next_mode = Mode::SPMM;
    next_spmm_cycle = spmm_cycle + 1;

    for (int r = 0; r < ROWS; ++r) {
        next_final_out[r] = reg_final_out[r];
        next_final_valid[r] = reg_final_valid[r];
    }

    total_pe_slots += ROWS * COLS;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            pes[i][j].set_mode(Mode::SPMM);

            float wire_b = 0.0f;
            float wire_spatial = 0.0f;

            if (i == 0) {
                wire_b = read_fifo_or_zero(fifo_v_top);
            } else {
                wire_b = reg_b_pipe[i - 1][j];
            }

            if (j == 0) {
                wire_spatial = 0.0f;
            } else {
                wire_spatial = reg_spatial_pipe[i][j - 1];
            }

            pes[i][j].evaluate(0.0f, wire_b, wire_spatial);

            const float comb_out = wire_spatial + pes[i][j].get_score() * wire_b;

            next_a_pipe[i][j] = 0.0f;
            next_b_pipe[i][j] = wire_b;
            next_spatial_pipe[i][j] = comb_out;

            if (j == COLS - 1 && spmm_cycle == i + COLS - 1) {
                next_final_out[i] = comb_out;
                next_final_valid[i] = true;
            }

            if (wire_b != 0.0f || wire_spatial != 0.0f) {
                ++active_pe_count;
            }
        }
    }
}

float SystolicArray::read_fifo_or_zero(FIFO* fifo) {
    if (fifo == nullptr || fifo->empty()) {
        return 0.0f;
    }

    const float value = fifo->front();
    fifo->pop();
    return value;
}

void SystolicArray::check_index(int row, int col) const {
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) {
        throw std::out_of_range("SystolicArray PE index out of range");
    }
}
