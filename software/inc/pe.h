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
