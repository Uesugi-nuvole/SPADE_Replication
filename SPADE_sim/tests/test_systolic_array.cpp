#include <iostream>
#include <vector>
#include <cmath>

#include "systolic_array.h"
#include "fifo.h"
#include "scheduler.h"

bool almost_equal(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

int main() {
    constexpr int N = SystolicArray::N;

    SystolicArray array;

    FIFO fifo_q_left;
    FIFO fifo_k_top;
    FIFO fifo_v_top;

    std::cout << "==============================" << std::endl;
    std::cout << "Testing SystolicArray" << std::endl;
    std::cout << "==============================" << std::endl;

    // ============================================================
    // Test 1: SDDMM
    //
    // Q 全 1，K 全 1。
    // 每个 PE 应该累计 16 次：
    // score = 16
    //
    // 输入需要做 systolic skew：
    // 第 i 行 Q 延迟 i 拍；
    // 第 j 列 K 延迟 j 拍。
    // ============================================================

    const int sddmm_total_cycles = 3 * N - 2;

    fifo_q_left.clear();
    fifo_k_top.clear();

    for (int t = 0; t < sddmm_total_cycles; ++t) {
        for (int i = 0; i < N; ++i) {
            float q_value = 0.0f;

            if (t >= i && t < i + N) {
                q_value = 1.0f;
            }

            fifo_q_left.push(q_value);
        }

        for (int j = 0; j < N; ++j) {
            float k_value = 0.0f;

            if (t >= j && t < j + N) {
                k_value = 1.0f;
            }

            fifo_k_top.push(k_value);
        }
    }

    CtrlSignal ctrl_sddmm;
    ctrl_sddmm.array_enable = true;
    ctrl_sddmm.array_clear = false;
    ctrl_sddmm.array_clear_datapath = false;
    ctrl_sddmm.array_mode = Mode::SDDMM;

    std::cout << "Running SDDMM..." << std::endl;

    for (int cycle = 0; cycle < sddmm_total_cycles; ++cycle) {
        array.evaluate(ctrl_sddmm, &fifo_q_left, &fifo_k_top, nullptr);
        array.update();
    }

    auto score_matrix = array.get_score_matrix();

    bool sddmm_pass = true;

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (!almost_equal(score_matrix[i][j], 16.0f)) {
                sddmm_pass = false;
            }
        }
    }

    std::cout << "SDDMM check: "
              << (sddmm_pass ? "[PASS]" : "[FAIL]")
              << std::endl;

    std::cout << "Sample scores:" << std::endl;
    std::cout << "score[0][0]   = " << score_matrix[0][0] << std::endl;
    std::cout << "score[0][15]  = " << score_matrix[0][15] << std::endl;
    std::cout << "score[15][0]  = " << score_matrix[15][0] << std::endl;
    std::cout << "score[15][15] = " << score_matrix[15][15] << std::endl;

    // ============================================================
    // Test 2: SPMM
    //
    // 已知每个 PE 的 score = 16。
    // V 全 1。
    //
    // 每一行最终输出：
    // 16 * 1 累加 16 次 = 256。
    //
    // 由于每行结果产生时间不同，
    // SystolicArray 内部 output buffer 会把结果保存起来，
    // 最后统一读取。
    // ============================================================

    array.clear_datapath_keep_score();

    fifo_v_top.clear();

    const int spmm_total_cycles = 2 * N - 1;

    for (int t = 0; t < spmm_total_cycles; ++t) {
        for (int j = 0; j < N; ++j) {
            float v_value = 0.0f;

            if (t == j) {
                v_value = 1.0f;
            }

            fifo_v_top.push(v_value);
        }
    }

    CtrlSignal ctrl_spmm;
    ctrl_spmm.array_enable = true;
    ctrl_spmm.array_clear = false;
    ctrl_spmm.array_clear_datapath = false;
    ctrl_spmm.array_mode = Mode::SPMM;

    std::cout << std::endl;
    std::cout << "Running SPMM..." << std::endl;

    for (int cycle = 0; cycle < spmm_total_cycles; ++cycle) {
        array.evaluate(ctrl_spmm, nullptr, nullptr, &fifo_v_top);
        array.update();
    }

    auto spmm_out = array.get_spmm_output_vector();

    bool spmm_pass = true;

    for (int i = 0; i < N; ++i) {
        if (!almost_equal(spmm_out[i], 256.0f)) {
            spmm_pass = false;
        }
    }

    std::cout << "SPMM check: "
              << (spmm_pass ? "[PASS]" : "[FAIL]")
              << std::endl;

    std::cout << "SPMM output vector:" << std::endl;
    for (int i = 0; i < N; ++i) {
        std::cout << spmm_out[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "==============================" << std::endl;
    std::cout << "SystolicArray test finished." << std::endl;
    std::cout << "==============================" << std::endl;

    return 0;
}
