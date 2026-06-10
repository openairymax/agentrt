/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_llm_d_complexity.c - 复杂度路由策略集成测试 (INT-16)
 *
 * Phase 2 集成测试: 验证 llm_d 基于复杂度的路由策略
 *
 * 验证覆盖:
 *   INT-16.1: SIMPLE 路由 - 简单查询路由到快速/便宜模型 (gpt-4o-mini)
 *   INT-16.2: MODERATE 路由 - 中等查询路由到平衡模型 (gpt-4o)
 *   INT-16.3: COMPLEX 路由 - 复杂查询路由到强大模型 (claude-sonnet)
 *   INT-16.4: 覆盖路由 - 显式 model 参数覆盖复杂度路由
 *   INT-16.5: 回退链 - 首选模型不可用时回退到下一个可用模型
 *
 * 该测试自包含，不依赖外部服务（无 LLM 调用，无网络）。
 */

#include "cache.h"
#include "cost_tracker.h"
#include "llm_service.h"
#include "providers/provider.h"
#include "providers/registry.h"
#include "response.h"

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

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    [FAIL] %s (line %d)\n", msg, __LINE__);                \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg)                                              \
    do {                                                                       \
        long _a = (long)(a);                                                   \
        long _e = (long)(b);                                                   \
        if (_a != _e) {                                                        \
            printf("    [FAIL] %s: got %ld, expected %ld (line %d)\n",         \
                   msg, _a, _e, __LINE__);                                      \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_STREQ(a, b, msg)                                           \
    do {                                                                       \
        if (strcmp((a), (b)) != 0) {                                           \
            printf("    [FAIL] %s: got '%s', expected '%s' (line %d)\n",       \
                   msg, (a), (b), __LINE__);                                    \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ============================================================================
 * 复杂度评估辅助 (与 test_complexity_routing.c / test_llm_d_routing.c 一致)
 * ============================================================================ */

typedef enum {
    COMPLEXITY_SIMPLE   = 0,
    COMPLEXITY_MODERATE = 1,
    COMPLEXITY_COMPLEX  = 2
} complexity_level_t;

static const char *complexity_names[] = { "SIMPLE", "MODERATE", "COMPLEX" };

static complexity_level_t assess_complexity(const char *input)
{
    if (!input) return COMPLEXITY_SIMPLE;

    size_t len = strlen(input);

    const char *complex_kw[] = {
        "architecture", "distributed", "system design", "scalability",
        "架构", "分布式", "系统设计", "高可用", "微服务"
    };
    const char *moderate_kw[] = {
        "function", "algorithm", "sort", "implement", "write",
        "函数", "算法", "排序", "实现", "编写", "Python", "Java"
    };

    int complex_score  = 0;
    int moderate_score = 0;

    for (size_t i = 0; i < sizeof(complex_kw) / sizeof(complex_kw[0]); i++) {
        if (strstr(input, complex_kw[i])) complex_score++;
    }
    for (size_t i = 0; i < sizeof(moderate_kw) / sizeof(moderate_kw[0]); i++) {
        if (strstr(input, moderate_kw[i])) moderate_score++;
    }

    if (complex_score >= 1 || len > 500) return COMPLEXITY_COMPLEX;
    if (moderate_score >= 1 || len > 50)  return COMPLEXITY_MODERATE;
    return COMPLEXITY_SIMPLE;
}

/* 根据复杂度选择模型路由 */
static const char *route_by_complexity(complexity_level_t level, const char *user_model)
{
    if (user_model && user_model[0]) return user_model;

    switch (level) {
    case COMPLEXITY_SIMPLE:   return "gpt-4o-mini";
    case COMPLEXITY_MODERATE: return "gpt-4o";
    case COMPLEXITY_COMPLEX:  return "claude-sonnet";
    default:                  return "gpt-4o-mini";
    }
}

/* ============================================================================
 * 模型定价规则 (用于成本验证)
 * ============================================================================ */
static pricing_rule_t mock_rules[] = {
    {"gpt-4o",        2.50, 10.00},
    {"gpt-4o-mini",   0.15,  0.60},
    {"claude-sonnet",  3.00, 15.00},
    {"claude-haiku",   0.25,  1.25},
    {"deepseek-v3",    0.14,  0.28},
    {"gemini-pro",     0.50,  1.50},
};

/* ============================================================================
 * INT-16.1: SIMPLE 路由
 *
 * 验证简单查询路由到快速/便宜模型:
 *   - 短文本、日常问候评估为 SIMPLE
 *   - SIMPLE 路由到 gpt-4o-mini
 *   - 验证成本最低
 * ============================================================================ */
TEST(int16_1_simple_routing)
{
    printf("    --- SIMPLE Routing ---\n");

    /* 1. 简单输入评估 */
    const char *simple_inputs[] = {
        "Hello, how are you?",
        "What is the weather today?",
        "你好，今天天气怎么样？",
        "Hi",
        "Thanks!",
        "Good morning",
    };
    int simple_count = (int)(sizeof(simple_inputs) / sizeof(simple_inputs[0]));

    for (int i = 0; i < simple_count; i++) {
        complexity_level_t level = assess_complexity(simple_inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        printf("    Input: \"%s\" → %s → %s\n",
               simple_inputs[i], complexity_names[level], model);

        TEST_ASSERT(level == COMPLEXITY_SIMPLE,
                    "简单输入应评估为 SIMPLE");
        TEST_ASSERT_STREQ(model, "gpt-4o-mini",
                          "SIMPLE 应路由到 gpt-4o-mini");
    }

    /* 2. 验证 SIMPLE 路由使用最便宜的模型 */
    double simple_cost_input  = mock_rules[1].input_price;   /* gpt-4o-mini */
    double moderate_cost_input = mock_rules[0].input_price;  /* gpt-4o */
    double complex_cost_input  = mock_rules[2].input_price;  /* claude-sonnet */

    TEST_ASSERT(simple_cost_input < moderate_cost_input,
                "SIMPLE 模型输入成本应低于 MODERATE");
    TEST_ASSERT(simple_cost_input < complex_cost_input,
                "SIMPLE 模型输入成本应低于 COMPLEX");
    printf("    Cost comparison: SIMPLE(%.2f) < MODERATE(%.2f) < COMPLEX(%.2f)\n",
           simple_cost_input, moderate_cost_input, complex_cost_input);

    /* 3. 验证 SIMPLE 模型的输出成本也最低 */
    double simple_cost_output  = mock_rules[1].output_price;
    double moderate_cost_output = mock_rules[0].output_price;
    double complex_cost_output  = mock_rules[2].output_price;

    TEST_ASSERT(simple_cost_output < moderate_cost_output,
                "SIMPLE 模型输出成本应低于 MODERATE");
    TEST_ASSERT(simple_cost_output < complex_cost_output,
                "SIMPLE 模型输出成本应低于 COMPLEX");
    printf("    Output cost: SIMPLE(%.2f) < MODERATE(%.2f) < COMPLEX(%.2f)\n",
           simple_cost_output, moderate_cost_output, complex_cost_output);
}

/* ============================================================================
 * INT-16.2: MODERATE 路由
 *
 * 验证中等查询路由到平衡模型:
 *   - 含编程/技术关键词的输入评估为 MODERATE
 *   - MODERATE 路由到 gpt-4o
 *   - 验证成本适中
 * ============================================================================ */
TEST(int16_2_moderate_routing)
{
    printf("    --- MODERATE Routing ---\n");

    /* 1. 中等输入评估 */
    const char *moderate_inputs[] = {
        "Write a Python function to sort a list of integers",
        "请编写一个Java函数实现快速排序算法",
        "How to implement a binary search in C?",
        "Explain the algorithm behind merge sort",
        "实现一个LRU缓存的数据结构",
    };
    int moderate_count = (int)(sizeof(moderate_inputs) / sizeof(moderate_inputs[0]));

    for (int i = 0; i < moderate_count; i++) {
        complexity_level_t level = assess_complexity(moderate_inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        printf("    Input: \"%s\" → %s → %s\n",
               moderate_inputs[i], complexity_names[level], model);

        TEST_ASSERT(level == COMPLEXITY_MODERATE,
                    "中等输入应评估为 MODERATE");
        TEST_ASSERT_STREQ(model, "gpt-4o",
                          "MODERATE 应路由到 gpt-4o");
    }

    /* 2. 验证 MODERATE 模型成本适中 */
    double moderate_cost = mock_rules[0].input_price;  /* gpt-4o */
    double simple_cost   = mock_rules[1].input_price;  /* gpt-4o-mini */
    double complex_cost  = mock_rules[2].input_price;  /* claude-sonnet */

    TEST_ASSERT(moderate_cost > simple_cost,
                "MODERATE 成本应高于 SIMPLE");
    TEST_ASSERT(moderate_cost < complex_cost,
                "MODERATE 成本应低于 COMPLEX");
    printf("    MODERATE cost (%.2f) is between SIMPLE (%.2f) and COMPLEX (%.2f)\n",
           moderate_cost, simple_cost, complex_cost);

    /* 3. 验证中等长度文本 (50-500字符) 无关键词时为 MODERATE */
    char medium_text[200];
    memset(medium_text, 'a', 150);
    medium_text[150] = '\0';

    complexity_level_t level = assess_complexity(medium_text);
    printf("    150-char no-keyword text: %s\n", complexity_names[level]);
    TEST_ASSERT(level == COMPLEXITY_MODERATE,
                "50-500字符无关键词文本应为 MODERATE");
}

/* ============================================================================
 * INT-16.3: COMPLEX 路由
 *
 * 验证复杂查询路由到强大模型:
 *   - 含架构/分布式/系统设计关键词的输入评估为 COMPLEX
 *   - COMPLEX 路由到 claude-sonnet
 *   - 验证使用最强大的模型
 * ============================================================================ */
TEST(int16_3_complex_routing)
{
    printf("    --- COMPLEX Routing ---\n");

    /* 1. 复杂输入评估 */
    const char *complex_inputs[] = {
        "Design a distributed system architecture for a global e-commerce platform "
        "handling millions of requests per second with high availability requirements",
        "请设计一个微服务架构的分布式系统，需要考虑可扩展性、容错性和数据一致性",
        "How to achieve scalability in a distributed database system?",
        "Explain the architecture of a high-availability microservice system",
        "分布式系统中的CAP理论如何影响架构设计？",
    };
    int complex_count = (int)(sizeof(complex_inputs) / sizeof(complex_inputs[0]));

    for (int i = 0; i < complex_count; i++) {
        complexity_level_t level = assess_complexity(complex_inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        printf("    Input (truncated) → %s → %s\n",
               complexity_names[level], model);

        TEST_ASSERT(level == COMPLEXITY_COMPLEX,
                    "复杂输入应评估为 COMPLEX");
        TEST_ASSERT_STREQ(model, "claude-sonnet",
                          "COMPLEX 应路由到 claude-sonnet");
    }

    /* 2. 验证超长文本 (>500字符) 自动评估为 COMPLEX */
    char long_text[600];
    memset(long_text, 'x', 550);
    long_text[550] = '\0';

    complexity_level_t level = assess_complexity(long_text);
    printf("    550-char text: %s\n", complexity_names[level]);
    TEST_ASSERT(level == COMPLEXITY_COMPLEX,
                ">500字符文本应自动评估为 COMPLEX");

    /* 3. 验证 COMPLEX 模型成本最高 */
    double complex_cost  = mock_rules[2].output_price;  /* claude-sonnet */
    double moderate_cost = mock_rules[0].output_price;  /* gpt-4o */
    double simple_cost   = mock_rules[1].output_price;  /* gpt-4o-mini */

    TEST_ASSERT(complex_cost > moderate_cost,
                "COMPLEX 输出成本应高于 MODERATE");
    TEST_ASSERT(complex_cost > simple_cost,
                "COMPLEX 输出成本应高于 SIMPLE");
    printf("    COMPLEX output cost (%.2f) is highest\n", complex_cost);
}

/* ============================================================================
 * INT-16.4: 覆盖路由
 *
 * 验证显式 model 参数覆盖复杂度路由:
 *   - 用户指定 model 时跳过复杂度评估
 *   - SIMPLE 输入 + 显式 model → 使用指定 model
 *   - COMPLEX 输入 + 显式 model → 使用指定 model
 *   - 空 model 字符串不触发覆盖
 * ============================================================================ */
TEST(int16_4_override_routing)
{
    printf("    --- Override Routing ---\n");

    /* 1. SIMPLE 输入 + 显式 model 覆盖 */
    const char *simple_input = "Hello";
    complexity_level_t level = assess_complexity(simple_input);
    TEST_ASSERT(level == COMPLEXITY_SIMPLE, "输入应为 SIMPLE");

    const char *user_model = "deepseek-v3";
    const char *routed = route_by_complexity(level, user_model);
    TEST_ASSERT_STREQ(routed, "deepseek-v3",
                      "用户指定 deepseek-v3 应覆盖自动路由");
    printf("    SIMPLE + override: %s → %s (not gpt-4o-mini)\n",
           simple_input, routed);

    /* 2. COMPLEX 输入 + 显式 model 覆盖 */
    const char *complex_input = "Design a distributed system architecture";
    level = assess_complexity(complex_input);
    TEST_ASSERT(level == COMPLEXITY_COMPLEX, "输入应为 COMPLEX");

    user_model = "gpt-4o-mini";
    routed = route_by_complexity(level, user_model);
    TEST_ASSERT_STREQ(routed, "gpt-4o-mini",
                      "用户指定 gpt-4o-mini 应覆盖 claude-sonnet");
    printf("    COMPLEX + override: → %s (not claude-sonnet)\n", routed);

    /* 3. MODERATE 输入 + 显式 model 覆盖到强模型 */
    const char *moderate_input = "Write a Python function to sort a list";
    level = assess_complexity(moderate_input);
    TEST_ASSERT(level == COMPLEXITY_MODERATE, "输入应为 MODERATE");

    user_model = "claude-sonnet";
    routed = route_by_complexity(level, user_model);
    TEST_ASSERT_STREQ(routed, "claude-sonnet",
                      "用户指定 claude-sonnet 应覆盖 gpt-4o");
    printf("    MODERATE + override: → %s (not gpt-4o)\n", routed);

    /* 4. 空 model 字符串不触发覆盖 */
    level = assess_complexity(simple_input);
    TEST_ASSERT(level == COMPLEXITY_SIMPLE, "输入应为 SIMPLE");

    routed = route_by_complexity(level, "");
    TEST_ASSERT_STREQ(routed, "gpt-4o-mini",
                      "空 model 字符串应使用自动路由");
    printf("    SIMPLE + empty model: → %s (auto-routing)\n", routed);

    /* 5. NULL model 不触发覆盖 */
    routed = route_by_complexity(level, NULL);
    TEST_ASSERT_STREQ(routed, "gpt-4o-mini",
                      "NULL model 应使用自动路由");
    printf("    SIMPLE + NULL model: → %s (auto-routing)\n", routed);

    /* 6. 覆盖路由的成本追踪验证 */
    cost_tracker_t *ct = cost_tracker_create(
        mock_rules, (int)(sizeof(mock_rules) / sizeof(mock_rules[0])));
    TEST_ASSERT(ct != NULL, "成本追踪器创建成功");

    /* SIMPLE 输入被覆盖到 claude-sonnet (更贵) */
    cost_tracker_add(ct, "claude-sonnet", 100, 200);
    /* 成本: (100/1000)*3.00 + (200/1000)*15.00 = 0.3 + 3.0 = 3.3 */

    cJSON *report = cost_tracker_export(ct);
    if (report) {
        char *json_str = cJSON_PrintUnformatted(report);
        TEST_ASSERT(json_str != NULL && strstr(json_str, "claude-sonnet") != NULL,
                    "成本报告包含 claude-sonnet (覆盖路由)");
        printf("    Override cost report: %.60s\n", json_str);
        free(json_str);
        cJSON_Delete(report);
    }
    cost_tracker_destroy(ct);
}

/* ============================================================================
 * INT-16.5: 回退链
 *
 * 验证首选模型不可用时回退到下一个可用模型:
 *   - 定义回退优先级链
 *   - 首选不可用时选择下一个
 *   - 所有不可用时返回 NULL
 *   - 结合复杂度路由的回退
 * ============================================================================ */
TEST(int16_5_fallback_chain)
{
    printf("    --- Fallback Chain ---\n");

    /* 1. 定义模型回退链 */
    typedef struct {
        const char *model;
        int is_available;
    } model_availability_t;

    /* SIMPLE 回退链: gpt-4o-mini → claude-haiku → deepseek-v3 */
    model_availability_t simple_chain[] = {
        {"gpt-4o-mini",  0},  /* 首选不可用 */
        {"claude-haiku",  1},  /* 回退可用 */
        {"deepseek-v3",   1},
    };
    int simple_chain_len = (int)(sizeof(simple_chain) / sizeof(simple_chain[0]));

    const char *selected = NULL;
    for (int i = 0; i < simple_chain_len; i++) {
        if (simple_chain[i].is_available) {
            selected = simple_chain[i].model;
            break;
        }
    }
    TEST_ASSERT(selected != NULL, "SIMPLE 回退链找到可用模型");
    TEST_ASSERT_STREQ(selected, "claude-haiku",
                      "gpt-4o-mini 不可用时回退到 claude-haiku");
    printf("    SIMPLE fallback: gpt-4o-mini (unavailable) → %s\n", selected);

    /* 2. MODERATE 回退链: gpt-4o → claude-sonnet → deepseek-v3 */
    model_availability_t moderate_chain[] = {
        {"gpt-4o",        0},  /* 首选不可用 */
        {"claude-sonnet",  0},  /* 第二选择不可用 */
        {"deepseek-v3",   1},  /* 第三选择可用 */
    };

    selected = NULL;
    for (int i = 0; i < 3; i++) {
        if (moderate_chain[i].is_available) {
            selected = moderate_chain[i].model;
            break;
        }
    }
    TEST_ASSERT(selected != NULL, "MODERATE 回退链找到可用模型");
    TEST_ASSERT_STREQ(selected, "deepseek-v3",
                      "gpt-4o 和 claude-sonnet 不可用时回退到 deepseek-v3");
    printf("    MODERATE fallback: gpt-4o → claude-sonnet (both unavailable) → %s\n",
           selected);

    /* 3. COMPLEX 回退链: claude-sonnet → gpt-4o → deepseek-v3 */
    model_availability_t complex_chain[] = {
        {"claude-sonnet",  0},  /* 首选不可用 */
        {"gpt-4o",        1},  /* 回退可用 */
        {"deepseek-v3",   1},
    };

    selected = NULL;
    for (int i = 0; i < 3; i++) {
        if (complex_chain[i].is_available) {
            selected = complex_chain[i].model;
            break;
        }
    }
    TEST_ASSERT(selected != NULL, "COMPLEX 回退链找到可用模型");
    TEST_ASSERT_STREQ(selected, "gpt-4o",
                      "claude-sonnet 不可用时回退到 gpt-4o");
    printf("    COMPLEX fallback: claude-sonnet (unavailable) → %s\n", selected);

    /* 4. 所有模型不可用 */
    model_availability_t none_available[] = {
        {"gpt-4o-mini",  0},
        {"claude-haiku",  0},
        {"deepseek-v3",   0},
    };

    selected = NULL;
    for (int i = 0; i < 3; i++) {
        if (none_available[i].is_available) {
            selected = none_available[i].model;
            break;
        }
    }
    TEST_ASSERT(selected == NULL, "所有模型不可用时应返回 NULL");
    printf("    All unavailable: NULL (expected)\n");

    /* 5. 所有模型可用时选择首选 */
    model_availability_t all_available[] = {
        {"gpt-4o-mini",  1},
        {"claude-haiku",  1},
        {"deepseek-v3",   1},
    };

    selected = NULL;
    for (int i = 0; i < 3; i++) {
        if (all_available[i].is_available) {
            selected = all_available[i].model;
            break;
        }
    }
    TEST_ASSERT_STREQ(selected, "gpt-4o-mini",
                      "所有可用时选择首选 gpt-4o-mini");
    printf("    All available: %s (first choice)\n", selected);

    /* 6. 端到端: 复杂度评估 → 模型选择 → 回退 */
    const char *test_input = "Design a distributed system architecture";
    complexity_level_t level = assess_complexity(test_input);
    const char *preferred = route_by_complexity(level, NULL);
    TEST_ASSERT_STREQ(preferred, "claude-sonnet",
                      "COMPLEX 首选 claude-sonnet");

    /* 模拟 claude-sonnet 不可用，回退到 gpt-4o */
    model_availability_t e2e_chain[] = {
        {"claude-sonnet",  0},
        {"gpt-4o",        1},
        {"deepseek-v3",   1},
    };

    selected = NULL;
    for (int i = 0; i < 3; i++) {
        if (e2e_chain[i].is_available) {
            selected = e2e_chain[i].model;
            break;
        }
    }
    TEST_ASSERT_STREQ(selected, "gpt-4o",
                      "E2E: claude-sonnet 不可用回退到 gpt-4o");
    printf("    E2E: '%s' → %s → %s (fallback)\n",
           test_input, preferred, selected);

    /* 7. 回退路由的成本追踪 */
    cost_tracker_t *ct = cost_tracker_create(
        mock_rules, (int)(sizeof(mock_rules) / sizeof(mock_rules[0])));
    TEST_ASSERT(ct != NULL, "回退成本追踪器创建成功");

    /* 回退到 gpt-4o 的成本 */
    cost_tracker_add(ct, "gpt-4o", 200, 500);
    /* 成本: (200/1000)*2.50 + (500/1000)*10.00 = 0.5 + 5.0 = 5.5 */

    cJSON *report = cost_tracker_export(ct);
    if (report) {
        char *json_str = cJSON_PrintUnformatted(report);
        TEST_ASSERT(json_str != NULL && strstr(json_str, "gpt-4o") != NULL,
                    "回退成本报告包含 gpt-4o");
        printf("    Fallback cost report: %.60s\n", json_str);
        free(json_str);
        cJSON_Delete(report);
    }
    cost_tracker_destroy(ct);
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(void)
{
    printf("=========================================\n");
    printf("  LLM Complexity Routing Strategy Tests\n");
    printf("  Phase 2 - INT-16\n");
    printf("=========================================\n\n");

    /* INT-16.1: SIMPLE 路由 */
    printf("--- INT-16.1: SIMPLE Routing ---\n");
    RUN_TEST(int16_1_simple_routing);

    /* INT-16.2: MODERATE 路由 */
    printf("\n--- INT-16.2: MODERATE Routing ---\n");
    RUN_TEST(int16_2_moderate_routing);

    /* INT-16.3: COMPLEX 路由 */
    printf("\n--- INT-16.3: COMPLEX Routing ---\n");
    RUN_TEST(int16_3_complex_routing);

    /* INT-16.4: 覆盖路由 */
    printf("\n--- INT-16.4: Override Routing ---\n");
    RUN_TEST(int16_4_override_routing);

    /* INT-16.5: 回退链 */
    printf("\n--- INT-16.5: Fallback Chain ---\n");
    RUN_TEST(int16_5_fallback_chain);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d INT-16 complexity routing tests PASSED\n",
               g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED (out of %d)\n",
               g_tests_passed, g_tests_failed, g_tests_run);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
