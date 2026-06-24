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
