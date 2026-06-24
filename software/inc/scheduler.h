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
