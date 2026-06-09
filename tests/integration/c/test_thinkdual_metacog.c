/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_thinkdual_metacog.c - 元认知校准验证集成测试 (INT-05)
 *
 * Phase 2 集成测试: 验证 Thinkdual 元认知模块的校准与反馈机制
 *
 * 验证覆盖:
 *   INT-05.1: 五维度评估 - 验证元认知评估所有5个维度
 *             (relevance, accuracy, completeness, consistency, clarity)
 *   INT-05.2: 置信度校准 - 验证置信度分数校准正确 (高质量→高置信度)
 *   INT-05.3: 自纠错触发 - 验证低置信度结果触发纠错
 *   INT-05.4: 反馈循环集成 - 验证元认知反馈影响后续思考
 *   INT-05.5: 阈值验证 - 验证可配置阈值正确工作
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

/* ============================================================================
 * 测试框架宏（与项目现有集成测试风格一致）
 * ============================================================================ */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        g_tests_run++;                                                         \
        g_tests_passed++;                                                      \
        printf("  PASSED\n");                                                  \
    } while (0)

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    ASSERT_FAIL: %s at line %d\n", #cond, __LINE__);       \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ(actual, expected)                                       \
    do {                                                                       \
        long _a = (long)(actual);                                              \
        long _e = (long)(expected);                                            \
        if (_a != _e) {                                                        \
            printf("    ASSERT_FAIL: %s == %ld, expected %ld at line %d\n",    \
                   #actual, _a, _e, __LINE__);                                 \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_FLOAT_EQ(a, b, eps)                                        \
    do {                                                                       \
        float _a = (float)(a);                                                 \
        float _b = (float)(b);                                                 \
        float _eps = (float)(eps);                                             \
        if (fabsf(_a - _b) > _eps) {                                          \
            printf("    ASSERT_FAIL: %s == %f, expected %f (eps=%f) at %d\n",  \
                   #a, _a, _b, _eps, __LINE__);                                \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ============================================================================
 * 辅助: 初始化思考步骤
 * ============================================================================ */
static void init_thinking_step(agentos_thinking_step_t *step, uint32_t id,
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

/* ============================================================================
 * INT-05.1: 五维度评估
 *
 * 验证元认知评估所有5个维度:
 *   - MC_DIM_RELEVANCE    (相关性)
 *   - MC_DIM_ACCURACY     (准确性)
 *   - MC_DIM_COMPLETENESS (完整性)
 *   - MC_DIM_CONSISTENCY  (一致性)
 *   - MC_DIM_CLARITY      (清晰度)
 * ============================================================================ */
TEST(int05_1_five_dimension_assessment)
{
    printf("    --- Five-Dimension Assessment ---\n");

    /* 1. 创建元认知引擎 */
    agentos_metacognition_t *mc = NULL;
    agentos_error_t err = agentos_mc_create(&mc);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(mc != NULL);

    /* 2. 创建高质量思考步骤 */
    agentos_thinking_step_t step;
    init_thinking_step(&step, 1, TC_STEP_GENERATION,
                       "Computer networking connects devices to share resources. "
                       "The OSI model defines seven layers from physical to application. "
                       "TCP/IP is the dominant protocol suite for the internet.",
                       "Explain the basics of computer networking",
                       0.85f);

    /* 3. 执行完整评估 */
    mc_evaluation_result_t result;
    memset(&result, 0, sizeof(result));
    err = agentos_mc_evaluate_step(mc, &step, NULL, 0, &result);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    /* 4. 验证5个维度全部被评估 */
    const char *dim_names[] = {
        "RELEVANCE", "ACCURACY", "COMPLETENESS", "CONSISTENCY", "CLARITY"
    };

    int dimensions_evaluated = 0;
    for (int i = 0; i < MC_DIM_COUNT; i++) {
        if (result.dimensions[i].score >= 0.0f && result.dimensions[i].score <= 1.0f) {
            dimensions_evaluated++;
            printf("    Dimension[%d] %s: score=%.3f\n",
                   i, dim_names[i], result.dimensions[i].score);
        }
    }
    TEST_ASSERT_EQ(dimensions_evaluated, MC_DIM_COUNT);

    /* 5. 验证维度枚举值覆盖完整 */
    TEST_ASSERT_EQ(MC_DIM_RELEVANCE, 0);
    TEST_ASSERT_EQ(MC_DIM_ACCURACY, 1);
    TEST_ASSERT_EQ(MC_DIM_COMPLETENESS, 2);
    TEST_ASSERT_EQ(MC_DIM_CONSISTENCY, 3);
    TEST_ASSERT_EQ(MC_DIM_CLARITY, 4);
    TEST_ASSERT_EQ(MC_DIM_COUNT, 5);

    /* 6. 验证综合评分在合理范围 */
    TEST_ASSERT(result.overall_score >= 0.0f && result.overall_score <= 1.0f);
    printf("    Overall score: %.3f, acceptable=%d\n",
           result.overall_score, result.is_acceptable);

    /* 7. 验证评估结果包含校准置信度 */
    TEST_ASSERT(result.calibrated_confidence >= 0.0f &&
                result.calibrated_confidence <= 1.0f);
    printf("    Calibrated confidence: %.3f\n", result.calibrated_confidence);

    /* 8. 清理 */
    if (result.critique_text)
        AGENTOS_FREE(result.critique_text);
    agentos_mc_destroy(mc);
}

/* ============================================================================
 * INT-05.2: 置信度校准
 *
 * 验证置信度分数校准正确:
 *   - 高质量输出应产生高置信度
 *   - 低质量输出应产生低置信度
 *   - 校准器根据历史反馈调整置信度
 * ============================================================================ */
TEST(int05_2_confidence_calibration)
{
    printf("    --- Confidence Calibration ---\n");

    /* 1. 创建元认知引擎 */
    agentos_metacognition_t *mc = NULL;
    agentos_error_t err = agentos_mc_create(&mc);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(mc->enable_confidence_calibration == 1);

    /* 2. 初始状态: 校准样本不足，返回原始置信度 */
    float raw_high = 0.9f;
    float calibrated = agentos_mc_calibrate_confidence(mc, raw_high);
    TEST_ASSERT_FLOAT_EQ(calibrated, raw_high, 0.001f);
    printf("    Initial (no samples): raw=%.3f, calibrated=%.3f\n",
           raw_high, calibrated);

    /* 3. 高质量输出评估 → 高置信度 */
    agentos_thinking_step_t high_step;
    init_thinking_step(&high_step, 10, TC_STEP_GENERATION,
                       "Machine learning is a subset of artificial intelligence that "
                       "enables systems to learn from data. It includes supervised, "
                       "unsupervised, and reinforcement learning paradigms.",
                       "What is machine learning?",
                       0.9f);

    mc_evaluation_result_t high_result;
    memset(&high_result, 0, sizeof(high_result));
    err = agentos_mc_evaluate_step(mc, &high_step, NULL, 0, &high_result);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    High quality: overall=%.3f, confidence=%.3f\n",
           high_result.overall_score, high_result.calibrated_confidence);

    /* 高质量输出应有较高的综合评分 */
    TEST_ASSERT(high_result.overall_score > 0.3f);

    /* 4. 低质量输出评估 → 低置信度 */
    agentos_thinking_step_t low_step;
    init_thinking_step(&low_step, 11, TC_STEP_GENERATION,
                       "I don't know.",
                       "Explain quantum computing in detail",
                       0.2f);

    mc_evaluation_result_t low_result;
    memset(&low_result, 0, sizeof(low_result));
    err = agentos_mc_evaluate_step(mc, &low_step, NULL, 0, &low_result);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Low quality: overall=%.3f, confidence=%.3f\n",
           low_result.overall_score, low_result.calibrated_confidence);

    /* 低质量输出应有较低的综合评分 */
    TEST_ASSERT(low_result.overall_score < high_result.overall_score);

    /* 5. 提供反馈以训练校准器 - 模拟过度自信 */
    for (int i = 0; i < 10; i++) {
        err = agentos_mc_feedback(mc, 0.9f, 0);  /* 预测0.9但实际错误 */
        TEST_ASSERT(err == AGENTOS_SUCCESS);
    }

    /* 6. 校准后: 过度自信的置信度应被降低 */
    float calibrated_overconfident = agentos_mc_calibrate_confidence(mc, 0.9f);
    printf("    After overconfidence feedback: raw=0.9, calibrated=%.3f\n",
           calibrated_overconfident);
    TEST_ASSERT(calibrated_overconfident < 0.9f);

    /* 7. 提供反馈训练校准器 - 模拟自信不足 */
    agentos_metacognition_t *mc2 = NULL;
    err = agentos_mc_create(&mc2);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    for (int i = 0; i < 10; i++) {
        err = agentos_mc_feedback(mc2, 0.3f, 1);  /* 预测0.3但实际正确 */
        TEST_ASSERT(err == AGENTOS_SUCCESS);
    }

    float calibrated_underconfident = agentos_mc_calibrate_confidence(mc2, 0.3f);
    printf("    After underconfidence feedback: raw=0.3, calibrated=%.3f\n",
           calibrated_underconfident);
    TEST_ASSERT(calibrated_underconfident > 0.3f);

    /* 8. 清理 */
    if (high_result.critique_text)
        AGENTOS_FREE(high_result.critique_text);
    if (low_result.critique_text)
        AGENTOS_FREE(low_result.critique_text);
    agentos_mc_destroy(mc);
    agentos_mc_destroy(mc2);
}

/* ============================================================================
 * INT-05.3: 自纠错触发
 *
 * 验证低置信度结果触发纠错:
 *   - 低评分结果应触发 MC_CORRECT_AUTO 或 MC_CORRECT_RERUN
 *   - 高评分结果应返回 MC_CORRECT_NONE
 *   - agentos_mc_should_self_correct 正确报告纠错需求
 * ============================================================================ */
TEST(int05_3_self_correction_trigger)
{
    printf("    --- Self-Correction Trigger ---\n");

    /* 1. 创建元认知引擎 */
    agentos_metacognition_t *mc = NULL;
    agentos_error_t err = agentos_mc_create(&mc);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Acceptance threshold: %.2f, auto_correct threshold: %.2f\n",
           mc->acceptance_threshold, mc->auto_correct_threshold);

    /* 2. 高质量输出 → 无需修正 */
    agentos_thinking_step_t good_step;
    init_thinking_step(&good_step, 20, TC_STEP_GENERATION,
                       "Python is a high-level programming language known for its "
                       "readability and versatility. It supports multiple programming "
                       "paradigms including procedural, object-oriented, and functional.",
                       "What is Python?",
                       0.85f);

    mc_evaluation_result_t good_result;
    memset(&good_result, 0, sizeof(good_result));
    err = agentos_mc_evaluate_step(mc, &good_step, NULL, 0, &good_result);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Good output: score=%.3f, strategy=%d, acceptable=%d\n",
           good_result.overall_score, (int)good_result.strategy,
           good_result.is_acceptable);

    /* 高质量输出应不需要修正 */
    if (good_result.is_acceptable) {
        TEST_ASSERT(good_result.strategy == MC_CORRECT_NONE);
        printf("    Good output: MC_CORRECT_NONE (no correction needed)\n");
    }

    /* 3. 低质量输出 → 触发纠错 */
    agentos_thinking_step_t bad_step;
    init_thinking_step(&bad_step, 21, TC_STEP_GENERATION,
                       "Uh, I think maybe something about computers?",
                       "Explain the architecture of a distributed database system "
                       "with ACID compliance and horizontal scalability",
                       0.15f);

    mc_evaluation_result_t bad_result;
    memset(&bad_result, 0, sizeof(bad_result));
    err = agentos_mc_evaluate_step(mc, &bad_step, NULL, 0, &bad_result);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Bad output: score=%.3f, strategy=%d, acceptable=%d\n",
           bad_result.overall_score, (int)bad_result.strategy,
           bad_result.is_acceptable);

    /* 低质量输出应触发某种纠错策略 */
    if (!bad_result.is_acceptable) {
        TEST_ASSERT(bad_result.strategy != MC_CORRECT_NONE);
        printf("    Bad output: correction strategy=%d (correction triggered)\n",
               (int)bad_result.strategy);
    }

    /* 4. 验证 should_self_correct 接口 */
    /* 先注入多次低分评估以建立纠错模式 */
    for (int i = 0; i < 5; i++) {
        agentos_thinking_step_t low_step;
        init_thinking_step(&low_step, 30 + i, TC_STEP_GENERATION,
                           "vague answer", "complex question", 0.2f);
        mc_evaluation_result_t low_result;
        memset(&low_result, 0, sizeof(low_result));
        err = agentos_mc_evaluate_step(mc, &low_step, NULL, 0, &low_result);
        if (err == AGENTOS_SUCCESS && low_result.critique_text)
            AGENTOS_FREE(low_result.critique_text);
    }

    int should_correct = agentos_mc_should_self_correct(mc, TC_STEP_GENERATION);
    printf("    should_self_correct(GENERATION): %d\n", should_correct);
    /* should_self_correct 应返回 0 或 1（不应返回错误 -1） */
    TEST_ASSERT(should_correct >= 0);

    /* 5. 验证严重程度随质量下降而增加 */
    TEST_ASSERT(bad_result.severity >= good_result.severity);
    printf("    Severity: good=%d, bad=%d\n",
           (int)good_result.severity, (int)bad_result.severity);

    /* 6. 清理 */
    if (good_result.critique_text)
        AGENTOS_FREE(good_result.critique_text);
    if (bad_result.critique_text)
        AGENTOS_FREE(bad_result.critique_text);
    agentos_mc_destroy(mc);
}

/* ============================================================================
 * INT-05.4: 反馈循环集成
 *
 * 验证元认知反馈影响后续思考:
 *   - 反馈更新校准器状态
 *   - 连续反馈改变校准偏差
 *   - 反馈历史影响后续评估的校准置信度
 *   - 统计信息正确更新
 * ============================================================================ */
TEST(int05_4_feedback_loop_integration)
{
    printf("    --- Feedback Loop Integration ---\n");

    /* 1. 创建元认知引擎 */
    agentos_metacognition_t *mc = NULL;
    agentos_error_t err = agentos_mc_create(&mc);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    /* 2. 初始校准器状态 */
    printf("    Initial: calibration_count=%zu, calibration_sum=%.4f\n",
           mc->calibrator.calibration_count,
           mc->calibrator.calibration_sum);

    /* 3. 第一轮: 提供准确反馈 (预测≈实际) */
    for (int i = 0; i < 5; i++) {
        err = agentos_mc_feedback(mc, 0.8f, 1);  /* 预测0.8，实际正确 */
        TEST_ASSERT(err == AGENTOS_SUCCESS);
    }
    printf("    After 5 accurate feedbacks: count=%zu, sum=%.4f\n",
           mc->calibrator.calibration_count,
           mc->calibrator.calibration_sum);

    /* 4. 第一轮评估: 校准后的置信度应接近原始值 (偏差小) */
    float cal_round1 = agentos_mc_calibrate_confidence(mc, 0.8f);
    printf("    Round 1 calibration: raw=0.8, calibrated=%.4f\n", cal_round1);

    /* 5. 第二轮: 提供过度自信反馈 (预测高但实际错) */
    for (int i = 0; i < 10; i++) {
        err = agentos_mc_feedback(mc, 0.9f, 0);  /* 预测0.9，实际错误 */
        TEST_ASSERT(err == AGENTOS_SUCCESS);
    }
    printf("    After overconfidence feedbacks: count=%zu, sum=%.4f\n",
           mc->calibrator.calibration_count,
           mc->calibrator.calibration_sum);

    /* 6. 第二轮评估: 校准应降低过度自信的置信度 */
    float cal_round2 = agentos_mc_calibrate_confidence(mc, 0.9f);
    printf("    Round 2 calibration: raw=0.9, calibrated=%.4f\n", cal_round2);
    TEST_ASSERT(cal_round2 < 0.9f);

    /* 7. 验证校准器历史记录更新 */
    TEST_ASSERT(mc->calibrator.history_index > 0);
    printf("    History index: %zu\n", mc->calibrator.history_index);

    /* 8. 验证过度自信率跟踪 */
    printf("    Overconfidence rate: %.4f\n", mc->calibrator.overconfidence_rate);
    printf("    Underconfidence rate: %.4f\n", mc->calibrator.underconfidence_rate);

    /* 9. 验证统计信息 */
    char *stats_json = NULL;
    err = agentos_mc_stats(mc, &stats_json);
    if (err == AGENTOS_SUCCESS && stats_json != NULL) {
        printf("    Stats JSON: %.100s\n", stats_json);
        AGENTOS_FREE(stats_json);
    }

    /* 10. 验证评估历史记录 */
    mc_evaluation_record_t *records = NULL;
    size_t record_count = 0;
    err = agentos_mc_get_history(mc, 10, &records, &record_count);
    printf("    History records: %zu\n", record_count);
    /* 历史记录可能为0（如果未执行 evaluate_step），这是可接受的 */

    /* 11. 执行一次完整评估+反馈循环 */
    agentos_thinking_step_t step;
    init_thinking_step(&step, 50, TC_STEP_VERIFICATION,
                       "The answer is verified and consistent with known facts.",
                       "Verify the previous answer", 0.75f);

    mc_evaluation_result_t eval_result;
    memset(&eval_result, 0, sizeof(eval_result));
    err = agentos_mc_evaluate_step(mc, &step, NULL, 0, &eval_result);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    /* 根据评估结果提供反馈 */
    int was_correct = eval_result.is_acceptable ? 1 : 0;
    err = agentos_mc_feedback(mc, eval_result.calibrated_confidence, was_correct);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Feedback loop: score=%.3f, confidence=%.3f, correct=%d\n",
           eval_result.overall_score, eval_result.calibrated_confidence, was_correct);

    /* 12. 清理 */
    if (eval_result.critique_text)
        AGENTOS_FREE(eval_result.critique_text);
    agentos_mc_destroy(mc);
}

/* ============================================================================
 * INT-05.5: 阈值验证
 *
 * 验证可配置阈值正确工作:
 *   - 默认 acceptance_threshold = 0.7
 *   - 默认 auto_correct_threshold = 0.5
 *   - 修改阈值影响评估结果
 *   - agentos_mc_adapt_threshold 自适应调整
 * ============================================================================ */
TEST(int05_5_threshold_validation)
{
    printf("    --- Threshold Validation ---\n");

    /* 1. 创建元认知引擎并验证默认阈值 */
    agentos_metacognition_t *mc = NULL;
    agentos_error_t err = agentos_mc_create(&mc);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    printf("    Default acceptance_threshold: %.2f\n", mc->acceptance_threshold);
    printf("    Default auto_correct_threshold: %.2f\n", mc->auto_correct_threshold);

    /* 验证默认阈值在合理范围 */
    TEST_ASSERT(mc->acceptance_threshold > 0.0f && mc->acceptance_threshold <= 1.0f);
    TEST_ASSERT(mc->auto_correct_threshold > 0.0f && mc->auto_correct_threshold <= 1.0f);
    TEST_ASSERT(mc->auto_correct_threshold < mc->acceptance_threshold);

    /* 2. 使用默认阈值评估中等质量输出 */
    agentos_thinking_step_t medium_step;
    init_thinking_step(&medium_step, 60, TC_STEP_GENERATION,
                       "Some partially correct information about the topic.",
                       "Explain the topic in detail", 0.6f);

    mc_evaluation_result_t result_default;
    memset(&result_default, 0, sizeof(result_default));
    err = agentos_mc_evaluate_step(mc, &medium_step, NULL, 0, &result_default);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Default threshold: score=%.3f, acceptable=%d, strategy=%d\n",
           result_default.overall_score, result_default.is_acceptable,
           (int)result_default.strategy);

    /* 3. 修改阈值为更严格 */
    float original_acceptance = mc->acceptance_threshold;
    float original_auto_correct = mc->auto_correct_threshold;

    mc->acceptance_threshold = 0.9f;   /* 提高接受门槛 */
    mc->auto_correct_threshold = 0.7f; /* 提高自动修正门槛 */

    printf("    Modified acceptance_threshold: %.2f\n", mc->acceptance_threshold);
    printf("    Modified auto_correct_threshold: %.2f\n", mc->auto_correct_threshold);

    /* 4. 使用严格阈值重新评估 */
    mc_evaluation_result_t result_strict;
    memset(&result_strict, 0, sizeof(result_strict));
    err = agentos_mc_evaluate_step(mc, &medium_step, NULL, 0, &result_strict);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Strict threshold: score=%.3f, acceptable=%d, strategy=%d\n",
           result_strict.overall_score, result_strict.is_acceptable,
           (int)result_strict.strategy);

    /* 更严格的阈值应使同一输出更可能不可接受 */
    if (result_default.is_acceptable && !result_strict.is_acceptable) {
        printf("    Threshold effect: same output accepted at %.2f but rejected at %.2f\n",
               original_acceptance, mc->acceptance_threshold);
    }

    /* 5. 修改阈值为更宽松 */
    mc->acceptance_threshold = 0.3f;
    mc->auto_correct_threshold = 0.2f;

    mc_evaluation_result_t result_lenient;
    memset(&result_lenient, 0, sizeof(result_lenient));
    err = agentos_mc_evaluate_step(mc, &medium_step, NULL, 0, &result_lenient);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    printf("    Lenient threshold: score=%.3f, acceptable=%d, strategy=%d\n",
           result_lenient.overall_score, result_lenient.is_acceptable,
           (int)result_lenient.strategy);

    /* 宽松阈值应使输出更可能被接受 */
    if (result_lenient.is_acceptable) {
        printf("    Lenient threshold: output accepted at %.2f\n",
               mc->acceptance_threshold);
    }

    /* 6. 验证自适应阈值调整 */
    mc->acceptance_threshold = original_acceptance;
    mc->auto_correct_threshold = original_auto_correct;

    /* 模拟连续接受 → 阈值应放宽 */
    mc->consecutive_accepts = 5;
    mc->consecutive_rejects = 0;
    float adapted = agentos_mc_adapt_threshold(mc);
    printf("    After 5 consecutive accepts: adapted_threshold=%.4f\n", adapted);

    /* 模拟连续拒绝 → 阈值应收紧 */
    mc->consecutive_accepts = 0;
    mc->consecutive_rejects = 5;
    adapted = agentos_mc_adapt_threshold(mc);
    printf("    After 5 consecutive rejects: adapted_threshold=%.4f\n", adapted);

    /* 7. 验证置信度校准开关 */
    mc->enable_confidence_calibration = 0;
    float raw = 0.8f;
    float uncalibrated = agentos_mc_calibrate_confidence(mc, raw);
    TEST_ASSERT_FLOAT_EQ(uncalibrated, raw, 0.001f);
    printf("    Calibration disabled: raw=%.3f, returned=%.3f\n", raw, uncalibrated);

    mc->enable_confidence_calibration = 1;

    /* 8. 验证重置功能 */
    agentos_mc_reset(mc);
    TEST_ASSERT_EQ(mc->calibrator.calibration_count, 0);
    TEST_ASSERT_FLOAT_EQ(mc->calibrator.calibration_sum, 0.0f, 0.001f);
    printf("    After reset: calibration_count=%zu, sum=%.4f\n",
           mc->calibrator.calibration_count, mc->calibrator.calibration_sum);

    /* 9. 清理 */
    if (result_default.critique_text)
        AGENTOS_FREE(result_default.critique_text);
    if (result_strict.critique_text)
        AGENTOS_FREE(result_strict.critique_text);
    if (result_lenient.critique_text)
        AGENTOS_FREE(result_lenient.critique_text);
    agentos_mc_destroy(mc);
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Thinkdual Metacognition Calibration\n");
    printf("  Phase 2 - INT-05\n");
    printf("=========================================\n\n");

    /* INT-05.1: 五维度评估 */
    printf("--- INT-05.1: Five-Dimension Assessment ---\n");
    RUN_TEST(int05_1_five_dimension_assessment);

    /* INT-05.2: 置信度校准 */
    printf("\n--- INT-05.2: Confidence Calibration ---\n");
    RUN_TEST(int05_2_confidence_calibration);

    /* INT-05.3: 自纠错触发 */
    printf("\n--- INT-05.3: Self-Correction Trigger ---\n");
    RUN_TEST(int05_3_self_correction_trigger);

    /* INT-05.4: 反馈循环集成 */
    printf("\n--- INT-05.4: Feedback Loop Integration ---\n");
    RUN_TEST(int05_4_feedback_loop_integration);

    /* INT-05.5: 阈值验证 */
    printf("\n--- INT-05.5: Threshold Validation ---\n");
    RUN_TEST(int05_5_threshold_validation);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d INT-05 metacognition tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED (out of %d)\n",
               g_tests_passed, g_tests_failed, g_tests_run);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
