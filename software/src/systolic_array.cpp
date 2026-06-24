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
