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
