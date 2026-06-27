#include <iostream>
#include <cmath>
#include "inc/pe.h"

bool almost_equal(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

void print_result(const std::string& test_name, float got, float expected) {
    std::cout << test_name << ": ";
    std::cout << "got = " << got << ", expected = " << expected;

    if (almost_equal(got, expected)) {
        std::cout << "  [PASS]";
    } else {
        std::cout << "  [FAIL]";
    }

    std::cout << std::endl;
}

int main() {
    RePE pe;

    std::cout << "==============================" << std::endl;
    std::cout << "Testing RePE" << std::endl;
    std::cout << "==============================" << std::endl;

    // ============================================================
    // Test 1: 默认 IDLE 状态
    // 期望：
    //   evaluate/update 后 score 和 out 都保持 0
    // ============================================================
    pe.clear_all();
    pe.set_mode(Mode::IDLE);

    pe.evaluate(1.0f, 2.0f, 3.0f);
    pe.update();

    print_result("Test 1A - IDLE score", pe.get_score(), 0.0f);
    print_result("Test 1B - IDLE out", pe.get_out(), 0.0f);

    // ============================================================
    // Test 2: SDDMM 累加
    //
    // 输入：
    //   (1, 2), (3, 4), (5, 6)
    //
    // 期望：
    //   score = 1*2 + 3*4 + 5*6 = 44
    // ============================================================
    pe.clear_all();
    pe.set_mode(Mode::SDDMM);

    float sddmm_inputs[3][2] = {
        {1.0f, 2.0f},
        {3.0f, 4.0f},
        {5.0f, 6.0f}
    };

    for (int i = 0; i < 3; ++i) {
        pe.evaluate(sddmm_inputs[i][0], sddmm_inputs[i][1], 0.0f);
        pe.update();
    }

    print_result("Test 2 - SDDMM accumulated score", pe.get_score(), 44.0f);

    // ============================================================
    // Test 3: SPMM 单次计算
    //
    // 当前 score = 44
    //
    // 输入：
    //   wire_b = 10
    //   wire_spatial = 7
    //
    // 期望：
    //   out = 7 + 44 * 10 = 447
    // ============================================================
    pe.set_mode(Mode::SPMM);
    pe.clear_out();

    pe.evaluate(0.0f, 10.0f, 7.0f);
    pe.update();

    print_result("Test 3 - SPMM single output", pe.get_out(), 447.0f);

    // ============================================================
    // Test 4: SPMM 多次 partial sum 传播
    //
    // 注意：
    //   RePE 的 SPMM 不是 out += score * b
    //   而是：
    //      next_out = wire_spatial + score * wire_b
    //
    // 所以如果想模拟多次累加，需要把上一拍 out
    // 作为下一拍的 wire_spatial。
    //
    // 输入：
    //   score = 44
    //   v = 10, 20, 30
    //
    // 期望：
    //   out = 44*10 + 44*20 + 44*30 = 2640
    // ============================================================
    pe.set_mode(Mode::SPMM);
    pe.clear_out();

    float v_inputs[3] = {10.0f, 20.0f, 30.0f};
    float spatial = 0.0f;

    for (int i = 0; i < 3; ++i) {
        pe.evaluate(0.0f, v_inputs[i], spatial);
        pe.update();

        // 模拟 partial sum 在下一拍传回来
        spatial = pe.get_out();
    }

    print_result("Test 4 - SPMM chained partial sum", pe.get_out(), 2640.0f);

    // ============================================================
    // Test 5: clear_score
    //
    // 期望：
    //   score 被清零
    //   out 不受影响
    // ============================================================
    float out_before_clear_score = pe.get_out();

    pe.clear_score();

    print_result("Test 5A - clear_score score", pe.get_score(), 0.0f);
    print_result("Test 5B - clear_score keeps out", pe.get_out(), out_before_clear_score);

    // ============================================================
    // Test 6: clear_out
    //
    // 期望：
    //   out 被清零
    // ============================================================
    pe.clear_out();

    print_result("Test 6 - clear_out out", pe.get_out(), 0.0f);

    // ============================================================
    // Test 7: clear_all
    //
    // 期望：
    //   score = 0
    //   out = 0
    // ============================================================
    pe.clear_all();

    print_result("Test 7A - clear_all score", pe.get_score(), 0.0f);
    print_result("Test 7B - clear_all out", pe.get_out(), 0.0f);

    std::cout << "==============================" << std::endl;
    std::cout << "RePE test finished." << std::endl;
    std::cout << "==============================" << std::endl;

    return 0;
}
