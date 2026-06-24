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
