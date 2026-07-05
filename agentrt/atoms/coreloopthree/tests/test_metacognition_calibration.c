/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_metacognition_calibration.c - 元认知校准验证测试 (INT-05)
 *
 * 验证覆盖:
 *   INT-05.1: 评分稳定性验证 (score stability)
 *   INT-05.2: 5维度权重校准基线 (weight calibration baseline)
 *   INT-05.3: 自我纠错模式检测 (self-correction mode detection)
 *   INT-05.4: 置信度校准偏差跟踪 (confidence calibration deviation tracking)
 *   INT-05.5: 错误策略选择 (error strategy selection)
 *
 * 该测试自包含，不依赖外部服务（无LLM调用，无网络）。
 */

#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "memory_compat.h"
#include "error.h"
#include "metacognition.h"
#include "thinking_chain.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        printf("  PASSED\n");                                                  \
    } while (0)

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

static int is_valid_json_prefix(const char *str)
{
    if (!str || str[0] == '\0')
        return 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
        str++;
    return (*str == '{' || *str == '[');
}

/* 初始化一个思考步骤 */
static void init_thinking_step(agentrt_thinking_step_t *step, uint32_t id,
                               tc_step_type_t type, const char *content,
                               const char *raw_input, float confidence)
{
    memset(step, 0, sizeof(*step));
    step->step_id         = id;
    step->type            = type;
    step->status          = TC_STATUS_COMPLETED;
    step->content         = (char *)content;
    step->content_len     = content ? strlen(content) : 0;
    step->raw_input       = (char *)raw_input;
    step->raw_input_len   = raw_input ? strlen(raw_input) : 0;
    step->confidence      = confidence;
}

/* 计算标准差 */
static float compute_stddev(const float *values, size_t count, float mean)
{
    if (count <= 1)
        return 0.0f;
    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float diff = values[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrtf(sum_sq / (float)count);
}

/* ============================================================================
 * INT-05.1: 评分稳定性验证
 *
 * 对同一输入重复评估5次，验证评分标准差 < 0.3
 * 断言: 每次处理成功，stats 返回合法 JSON
 * ============================================================================ */
TEST(int05_1_score_stability)
{
    /* 1. 创建元认知引擎 */
    agentrt_metacognition_t *mc = NULL;
    agentrt_error_t        err = agentrt_mc_create(&mc);
    assert(err == AGENTRT_SUCCESS);
    assert(mc != NULL);
    printf("    Metacognition engine created\n");

    /* 2. 准备测试输入/输出 */
    const char *input =
        "Explain the concept of quantum entanglement and its applications "
        "in quantum computing. Provide concrete examples and discuss the "
        "limitations of current technology.";
    const char *output =
        "Quantum entanglement is a physical phenomenon where two or more "
        "particles become correlated in such a way that the quantum state "
        "of each particle cannot be described independently. According to "
        "quantum mechanics, entangled particles remain connected so that "
        "actions performed on one affect the other, even when separated "
        "by large distances. This phenomenon was first described by "
        "Einstein, Podolsky, and Rosen in 1935 as the EPR paradox. "
        "Applications include quantum cryptography, quantum teleportation, "
        "and superdense coding. Current limitations include decoherence "
        "issues and the challenge of maintaining entanglement over long "
        "distances. First, quantum computing uses qubits instead of bits. "
        "Second, entanglement allows for parallel computation. Finally, "
        "in conclusion, quantum entanglement remains a cornerstone of "
        "quantum information science and has promising applications.";

    /* 3. 重复评估5次，收集评分 */
    float overall_scores[5];
    float dim_scores[5][MC_DIM_COUNT];

    for (int iter = 0; iter < 5; iter++) {
        agentrt_thinking_step_t step;
        init_thinking_step(&step, (uint32_t)(iter + 1), TC_STEP_GENERATION,
                           output, input, 0.85f);

        mc_evaluation_result_t result;
        err = agentrt_mc_evaluate_step(mc, &step, NULL, 0, &result);
        assert(err == AGENTRT_SUCCESS);
        printf("    Iteration %d: overall=%.4f, acceptable=%d, strategy=%d\n",
               iter + 1, result.overall_score, result.is_acceptable,
               (int)result.strategy);

        overall_scores[iter] = result.overall_score;
        for (int d = 0; d < MC_DIM_COUNT; d++) {
            dim_scores[iter][d] = result.dimensions[d].score;
        }

        /* 释放批判文本 */
        if (result.critique_text) {
            free(result.critique_text);
        }

        /* 获取 stats 并验证为合法 JSON */
        char *stats_json = NULL;
        err = agentrt_mc_stats(mc, &stats_json);
        assert(err == AGENTRT_SUCCESS);
        assert(stats_json != NULL);
        assert(is_valid_json_prefix(stats_json));
        printf("    Stats[%d]: %.120s\n", iter + 1, stats_json);
        free(stats_json);
    }

    /* 4. 计算综合评分的标准差 */
    float sum = 0.0f;
    for (int i = 0; i < 5; i++)
        sum += overall_scores[i];
    float mean   = sum / 5.0f;
    float stddev = compute_stddev(overall_scores, 5, mean);

    printf("    Overall score: mean=%.4f, stddev=%.4f\n", mean, stddev);

    /* 5. 断言: 标准差 < 0.3 */
    assert(stddev < 0.3f);

    /* 6. 打印各维度稳定性 */
    const char *dim_names[] = {"relevance", "accuracy", "completeness",
                               "consistency", "clarity"};
    for (int d = 0; d < MC_DIM_COUNT; d++) {
        float dsum  = 0.0f;
        float dvals[5];
        for (int i = 0; i < 5; i++) {
            dvals[i] = dim_scores[i][d];
            dsum += dvals[i];
        }
        float dmean = dsum / 5.0f;
        float dstd  = compute_stddev(dvals, 5, dmean);
        printf("    %s: mean=%.4f, stddev=%.4f\n", dim_names[d], dmean, dstd);
        assert(dstd < 0.3f);
    }

    agentrt_mc_destroy(mc);
}

/* ============================================================================
 * INT-05.2: 5维度权重校准基线
 *
 * 处理100+不同输入，收集5维度评分，验证维度非零、综合评分在[0,1]范围
 * 打印校准基线报告
 * ============================================================================ */
TEST(int05_2_weight_calibration_baseline)
{
    /* 1. 创建元认知引擎 */
    agentrt_metacognition_t *mc = NULL;
    agentrt_error_t        err = agentrt_mc_create(&mc);
    assert(err == AGENTRT_SUCCESS);
    assert(mc != NULL);

    /* 2. 准备多样化的输入/输出对（10对，循环使用） */
    static const char *test_inputs[] = {
        "What is the capital of France?",
        "Explain the water cycle in detail",
        "Write a Python function to sort a list",
        "Compare machine learning and deep learning",
        "Define the concept of supply and demand in economics",
        "What are the main causes of climate change?",
        "How does photosynthesis work in plants?",
        "Describe the structure of DNA and its function",
        "What is the Pythagorean theorem and how is it used?",
        "Explain the difference between TCP and UDP protocols",
    };

    static const char *test_outputs[] = {
        "The capital of France is Paris, a major European city known for "
        "its art, culture, and historical landmarks such as the Eiffel Tower. "
        "According to historical records, Paris has been the capital since "
        "the 10th century. First, it is the political center of France. "
        "Second, it is the economic hub. In conclusion, Paris is the capital.",
        "The water cycle, also known as the hydrological cycle, describes "
        "the continuous movement of water on, above, and below the surface "
        "of the Earth. The main processes include evaporation, condensation, "
        "precipitation, and collection. First, water evaporates from oceans "
        "and lakes. Second, water vapor condenses to form clouds. Finally, "
        "precipitation returns water to the surface. In summary, the water "
        "cycle is essential for sustaining life on Earth.",
        "To sort a list in Python, you can use the built-in sorted() "
        "function or the list.sort() method. The sorted() function returns "
        "a new sorted list, while sort() modifies the list in place. "
        "Both methods accept a key parameter for custom sorting logic. "
        "According to Python documentation, the sorting algorithm is "
        "Timsort, which is stable and efficient. First, define your list. "
        "Second, call sorted() or sort(). Finally, verify the result.",
        "Machine learning and deep learning are both subsets of artificial "
        "intelligence. Machine learning uses algorithms to parse data, "
        "learn from it, and make predictions. Deep learning is a subset "
        "of machine learning that uses neural networks with many layers. "
        "First, machine learning requires feature engineering. Second, "
        "deep learning can automatically extract features. In conclusion, "
        "deep learning is more powerful but requires more data and compute.",
        "Supply and demand is a fundamental economic model that determines "
        "price in a market. Supply represents how much the market can offer, "
        "while demand represents how much consumers want. According to "
        "economic theory, the equilibrium price is where supply equals "
        "demand. First, when demand exceeds supply, prices rise. Second, "
        "when supply exceeds demand, prices fall. Finally, market forces "
        "tend to restore equilibrium over time.",
        "The main causes of climate change include greenhouse gas emissions "
        "from burning fossil fuels, deforestation, industrial processes, "
        "and agricultural practices. According to scientific evidence, "
        "carbon dioxide levels have increased significantly since the "
        "industrial revolution. First, CO2 traps heat in the atmosphere. "
        "Second, methane from agriculture contributes significantly. "
        "Finally, deforestation reduces capacity to absorb CO2. "
        "In conclusion, human activities are the primary driver.",
        "Photosynthesis is the process by which plants convert light energy "
        "into chemical energy. It occurs in chloroplasts, where chlorophyll "
        "absorbs sunlight. The process uses carbon dioxide and water to "
        "produce glucose and oxygen. First, light energy splits water "
        "molecules. Second, carbon dioxide is fixed into glucose. Finally, "
        "oxygen is released as a byproduct. According to biologists, this "
        "process is the foundation of most food chains on Earth.",
        "DNA, or deoxyribonucleic acid, is a double-helix molecule that "
        "contains the genetic blueprint for all living organisms. Its "
        "structure consists of two strands of nucleotides twisted around "
        "each other. Each nucleotide contains a sugar, a phosphate group, "
        "and a nitrogenous base. According to Watson and Crick, the four "
        "bases are adenine, thymine, guanine, and cytosine. First, DNA "
        "stores genetic information. Second, it replicates during cell "
        "division. In conclusion, DNA is the molecule of heredity.",
        "The Pythagorean theorem states that in a right triangle, the "
        "square of the hypotenuse equals the sum of the squares of the "
        "other two sides. Mathematically, a^2 + b^2 = c^2. It is used in "
        "geometry, construction, navigation, and computer graphics. "
        "According to historical records, the theorem was known to "
        "Babylonian mathematicians before Pythagoras. First, identify "
        "the right angle. Second, square the two legs. Finally, take "
        "the square root to find the hypotenuse.",
        "TCP and UDP are transport layer protocols. TCP is connection-oriented "
        "and provides reliable, ordered delivery of data. It uses handshaking "
        "and acknowledgments. UDP is connectionless and provides faster but "
        "unreliable delivery. First, TCP is used for web browsing and email. "
        "Second, UDP is used for streaming and gaming. According to network "
        "engineers, TCP guarantees delivery while UDP prioritizes speed. "
        "In conclusion, the choice depends on the application requirements.",
    };

    const int num_tests = 100;
    float     dim_sums[MC_DIM_COUNT] = {0};
    float     dim_mins[MC_DIM_COUNT] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float     dim_maxs[MC_DIM_COUNT] = {0};
    float     overall_sum            = 0.0f;
    float     overall_min            = 1.0f;
    float     overall_max            = 0.0f;
    int       accept_count           = 0;

    for (int i = 0; i < num_tests; i++) {
        /* 循环使用准备好的输入/输出对 */
        int         idx   = i % 10;
        const char *input  = test_inputs[idx];
        const char *output = test_outputs[idx];

        agentrt_thinking_step_t step;
        init_thinking_step(&step, (uint32_t)(i + 1), TC_STEP_GENERATION,
                           output, input, 0.8f);

        mc_evaluation_result_t result;
        agentrt_error_t        e = agentrt_mc_evaluate_step(mc, &step, NULL, 0, &result);
        assert(e == AGENTRT_SUCCESS);

        /* 收集各维度评分 */
        for (int d = 0; d < MC_DIM_COUNT; d++) {
            float score = result.dimensions[d].score;
            dim_sums[d] += score;
            if (score < dim_mins[d]) dim_mins[d] = score;
            if (score > dim_maxs[d]) dim_maxs[d] = score;
        }

        overall_sum += result.overall_score;
        if (result.overall_score < overall_min) overall_min = result.overall_score;
        if (result.overall_score > overall_max) overall_max = result.overall_score;
        if (result.is_acceptable) accept_count++;

        if (result.critique_text)
            free(result.critique_text);
    }

    /* 3. 验证每个维度非零 */
    const char *dim_names[] = {"relevance", "accuracy", "completeness",
                               "consistency", "clarity"};
    for (int d = 0; d < MC_DIM_COUNT; d++) {
        float avg = dim_sums[d] / (float)num_tests;
        assert(avg > 0.0f);
        printf("    %s: avg=%.4f, min=%.4f, max=%.4f\n",
               dim_names[d], avg, dim_mins[d], dim_maxs[d]);
    }

    /* 4. 验证综合评分在 [0.0, 1.0] 范围 */
    float overall_avg = overall_sum / (float)num_tests;
    assert(overall_avg >= 0.0f && overall_avg <= 1.0f);
    assert(overall_min >= 0.0f && overall_min <= 1.0f);
    assert(overall_max >= 0.0f && overall_max <= 1.0f);
    printf("    Overall score: avg=%.4f, min=%.4f, max=%.4f\n",
           overall_avg, overall_min, overall_max);

    /* 5. 验证 accept 率 */
    float accept_rate = (float)accept_count / (float)num_tests;
    printf("    Accept rate: %.1f%% (%d/%d)\n",
           accept_rate * 100.0f, accept_count, num_tests);

    /* 6. 获取 stats 验证 */
    char *stats_json = NULL;
    err = agentrt_mc_stats(mc, &stats_json);
    assert(err == AGENTRT_SUCCESS);
    assert(stats_json != NULL);
    assert(is_valid_json_prefix(stats_json));
    printf("    Stats: %s\n", stats_json);

    /* 7. 打印校准基线报告 */
    printf("\n");
    printf("    ================================================\n");
    printf("      5-Dimension Weight Calibration Baseline Report\n");
    printf("    ================================================\n");
    printf("    Samples processed: %d\n", num_tests);
    printf("    Default weights: 0.25/0.25/0.15/0.20/0.15\n");
    printf("    ------------------------------------------------\n");
    for (int d = 0; d < MC_DIM_COUNT; d++) {
        float avg = dim_sums[d] / (float)num_tests;
        printf("    %-12s: avg=%.4f  range=[%.4f, %.4f]\n",
               dim_names[d], avg, dim_mins[d], dim_maxs[d]);
    }
    printf("    ------------------------------------------------\n");
    printf("    Composite score    : avg=%.4f  range=[%.4f, %.4f]\n",
           overall_avg, overall_min, overall_max);
    printf("    Acceptance rate    : %.1f%%\n", accept_rate * 100.0f);
    printf("    Total evaluations  : %d\n", num_tests);
    printf("    ================================================\n");

    free(stats_json);
    agentrt_mc_destroy(mc);
}

/* ============================================================================
 * INT-05.3: 自我纠错模式检测
 *
 * 通过创建混合评估记录（部分失败），验证 self-correct 模式检测
 * ============================================================================ */
TEST(int05_3_self_correction_mode_detection)
{
    /* 1. 创建元认知引擎 */
    agentrt_metacognition_t *mc  = NULL;
    agentrt_error_t         err = agentrt_mc_create(&mc);
    assert(err == AGENTRT_SUCCESS);

    /* 初始状态: 记录不足，不应触发自我纠错 */
    int should_correct = agentrt_mc_should_self_correct(mc, TC_STEP_GENERATION);
    assert(should_correct == 0);
    printf("    Initial: should_self_correct=%d (expected 0, not enough records)\n",
           should_correct);

    /* 2. 创建一系列评估记录，其中一半是低分 */
    const char *good_input =
        "Explain the concept of gravity";
    const char *good_output =
        "Gravity is a fundamental force of nature that attracts objects "
        "with mass toward each other. According to Newton's law of universal "
        "gravitation, the force is proportional to the product of masses. "
        "First, gravity keeps planets in orbit. Second, it causes objects "
        "to fall toward Earth. Finally, Einstein's theory of general "
        "relativity explains gravity as the curvature of spacetime. "
        "In conclusion, gravity is one of the four fundamental forces.";
    const char *bad_input  = "What is 2+2?";
    const char *bad_output =
        "I think maybe possibly the answer is 5 but I am not sure but "
        "on the other hand I guess it could be 4";

    for (int i = 0; i < 12; i++) {
        agentrt_thinking_step_t step;
        if (i % 2 == 0) {
            init_thinking_step(&step, (uint32_t)(i + 1), TC_STEP_GENERATION,
                               good_output, good_input, 0.9f);
        } else {
            init_thinking_step(&step, (uint32_t)(i + 1), TC_STEP_GENERATION,
                               bad_output, bad_input, 0.3f);
        }

        mc_evaluation_result_t result;
        err = agentrt_mc_evaluate_step(mc, &step, NULL, 0, &result);
        assert(err == AGENTRT_SUCCESS);

        if (result.critique_text)
            free(result.critique_text);
    }

    /* 3. 验证 stats 中的评估计数 */
    char *stats_json = NULL;
    err = agentrt_mc_stats(mc, &stats_json);
    assert(err == AGENTRT_SUCCESS);
    assert(stats_json != NULL);
    printf("    After 12 evaluations: %s\n", stats_json);
    free(stats_json);

    /* 4. 检查自我纠错模式检测：一半失败应触发 */
    should_correct = agentrt_mc_should_self_correct(mc, TC_STEP_GENERATION);
    printf("    After mixed evaluations: should_self_correct=%d\n", should_correct);
    assert(should_correct == 1);

    /* 5. 测试不同步骤类型的自我纠错检测 */
    int sc_decomp      = agentrt_mc_should_self_correct(mc, TC_STEP_DECOMPOSITION);
    int sc_planning    = agentrt_mc_should_self_correct(mc, TC_STEP_PLANNING);
    int sc_verification = agentrt_mc_should_self_correct(mc, TC_STEP_VERIFICATION);
    printf("    Self-correct: decomp=%d, planning=%d, verification=%d\n",
           sc_decomp, sc_planning, sc_verification);

    agentrt_mc_destroy(mc);
}

/* ============================================================================
 * INT-05.4: 置信度校准偏差跟踪
 *
 * 验证 confidence calibration 的偏差累积和校准逻辑
 * ============================================================================ */
TEST(int05_4_confidence_calibration_deviation)
{
    /* 1. 创建元认知引擎 */
    agentrt_metacognition_t *mc  = NULL;
    agentrt_error_t         err = agentrt_mc_create(&mc);
    assert(err == AGENTRT_SUCCESS);
    assert(mc->enable_confidence_calibration == 1);

    /* 2. 初始状态: 校准样本不足，返回原始置信度 */
    float raw_conf   = 0.9f;
    float calibrated = agentrt_mc_calibrate_confidence(mc, raw_conf);
    assert(calibrated == raw_conf); /* 样本不足5，直接返回原始值 */
    printf("    Initial: raw=%.4f, calibrated=%.4f (no calibration yet)\n",
           raw_conf, calibrated);

    /* 3. 提供一系列反馈，模拟过度自信 */
    /* 预测置信度0.9，但实际只有一半正确 → 偏差约0.4 */
    for (int i = 0; i < 10; i++) {
        int was_correct = (i % 2 == 0) ? 1 : 0;
        err = agentrt_mc_feedback(mc, 0.9f, was_correct);
        assert(err == AGENTRT_SUCCESS);
    }

    /* 4. 验证校准器状态 */
    assert(mc->calibrator.calibration_count == 10);
    /* 偏差 = 预测0.9 - 实际(0.5平均) = 0.4 */
    float expected_bias = 0.4f;
    float actual_bias   = mc->calibrator.calibration_sum / 10.0f;
    printf("    Calibration bias: expected=%.4f, actual=%.4f\n",
           expected_bias, actual_bias);
    assert(fabsf(actual_bias - expected_bias) < 0.01f);

    /* 5. 校准后的置信度应该降低 */
    calibrated = agentrt_mc_calibrate_confidence(mc, 0.9f);
    printf("    After feedback: raw=0.9, calibrated=%.4f\n", calibrated);
    assert(calibrated < 0.9f); /* 过度自信被校准降低 */

    /* 6. 验证 stats 包含校准信息 */
    char *stats_json = NULL;
    err = agentrt_mc_stats(mc, &stats_json);
    assert(err == AGENTRT_SUCCESS);
    assert(stats_json != NULL);
    assert(is_valid_json_prefix(stats_json));
    printf("    Calibration stats: %s\n", stats_json);
    assert(strstr(stats_json, "calibration") != NULL);
    assert(strstr(stats_json, "bias") != NULL);
    free(stats_json);

    /* 7. 测试自信不足的情况 */
    agentrt_metacognition_t *mc2 = NULL;
    err = agentrt_mc_create(&mc2);
    assert(err == AGENTRT_SUCCESS);

    for (int i = 0; i < 10; i++) {
        /* 预测低置信度但实际全对 → 自信不足 */
        err = agentrt_mc_feedback(mc2, 0.3f, 1);
        assert(err == AGENTRT_SUCCESS);
    }

    float cal2 = agentrt_mc_calibrate_confidence(mc2, 0.3f);
    printf("    Underconfident: raw=0.3, calibrated=%.4f\n", cal2);
    assert(cal2 > 0.3f); /* 自信不足被校准提升 */

    agentrt_mc_destroy(mc2);
    agentrt_mc_destroy(mc);
}

/* ============================================================================
 * INT-05.5: 错误策略选择验证
 *
 * 验证不同评分水平触发不同的纠正策略
 * ============================================================================ */
TEST(int05_5_error_strategy_selection)
{
    /* 1. 创建元认知引擎 */
    agentrt_metacognition_t *mc  = NULL;
    agentrt_error_t         err = agentrt_mc_create(&mc);
    assert(err == AGENTRT_SUCCESS);
    printf("    Acceptance threshold: %.2f, auto_correct threshold: %.2f\n",
           mc->acceptance_threshold, mc->auto_correct_threshold);

    /* 2. 测试高质量输出 → MC_CORRECT_NONE */
    {
        const char *input =
            "Explain the basics of computer networking";
        const char *output =
            "Computer networking is the practice of connecting computers "
            "and devices to share resources and communicate. According to "
            "the OSI model, networking has seven layers. First, the physical "
            "layer deals with hardware. Second, the data link layer handles "
            "frames. Finally, the application layer provides services to "
            "users. In conclusion, networking is fundamental to modern "
            "computing and the internet as we know it.";
        agentrt_thinking_step_t step;
        init_thinking_step(&step, 1, TC_STEP_GENERATION, output, input, 0.9f);

        mc_evaluation_result_t result;
        err = agentrt_mc_evaluate_step(mc, &step, NULL, 0, &result);
        assert(err == AGENTRT_SUCCESS);

        printf("    High quality: overall=%.4f, strategy=%d, severity=%d\n",
               result.overall_score, (int)result.strategy, (int)result.severity);
        assert(result.strategy == MC_CORRECT_NONE);
        assert(result.is_acceptable == 1);

        if (result.critique_text)
            free(result.critique_text);
    }

    /* 3. 测试中等质量输出 → MC_CORRECT_AUTO 或 MC_CORRECT_RERUN */
    {
        const char *input =
            "What is the weather like?";
        const char *output =
            "I think maybe possibly it is sunny. The weather varies by location.";
        agentrt_thinking_step_t step;
        init_thinking_step(&step, 2, TC_STEP_GENERATION, output, input, 0.5f);

        mc_evaluation_result_t result;
        err = agentrt_mc_evaluate_step(mc, &step, NULL, 0, &result);
        assert(err == AGENTRT_SUCCESS);

        printf("    Medium quality: overall=%.4f, strategy=%d, severity=%d\n",
               result.overall_score, (int)result.strategy, (int)result.severity);
        /* 包含可疑模式，分数应低于可接受阈值 */
        assert(result.overall_score < mc->acceptance_threshold);
        assert(result.strategy != MC_CORRECT_NONE);

        if (result.critique_text)
            free(result.critique_text);
    }

    /* 4. 测试低质量输出 → MC_CORRECT_RERUN 或 MC_CORRECT_ESCALATE */
    {
        const char *input  = "Calculate 2+2";
        const char *output =
            "I think maybe possibly I am not sure but I guess";
        agentrt_thinking_step_t step;
        init_thinking_step(&step, 3, TC_STEP_GENERATION, output, input, 0.2f);

        mc_evaluation_result_t result;
        err = agentrt_mc_evaluate_step(mc, &step, NULL, 0, &result);
        assert(err == AGENTRT_SUCCESS);

        printf("    Low quality: overall=%.4f, strategy=%d, severity=%d\n",
               result.overall_score, (int)result.strategy, (int)result.severity);
        assert(result.overall_score < mc->auto_correct_threshold);
        assert(result.strategy == MC_CORRECT_RERUN ||
               result.strategy == MC_CORRECT_ESCALATE);

        if (result.critique_text)
            free(result.critique_text);
    }

    /* 5. 验证 stats 中的修正统计 */
    char *stats_json = NULL;
    err = agentrt_mc_stats(mc, &stats_json);
    assert(err == AGENTRT_SUCCESS);
    assert(stats_json != NULL);
    printf("    Strategy selection stats: %s\n", stats_json);
    free(stats_json);

    agentrt_mc_destroy(mc);
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Metacognition Calibration Tests\n");
    printf("  Dual-Thinking System (INT-05)\n");
    printf("=========================================\n\n");

    printf("--- INT-05.1: Score Stability Verification ---\n");
    RUN_TEST(int05_1_score_stability);

    printf("\n--- INT-05.2: 5-Dimension Weight Calibration Baseline ---\n");
    RUN_TEST(int05_2_weight_calibration_baseline);

    printf("\n--- INT-05.3: Self-Correction Mode Detection ---\n");
    RUN_TEST(int05_3_self_correction_mode_detection);

    printf("\n--- INT-05.4: Confidence Calibration Deviation Tracking ---\n");
    RUN_TEST(int05_4_confidence_calibration_deviation);

    printf("\n--- INT-05.5: Error Strategy Selection ---\n");
    RUN_TEST(int05_5_error_strategy_selection);

    printf("\n=========================================\n");
    printf("  All metacognition calibration tests PASSED\n");
    printf("=========================================\n");

    return 0;
}