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
