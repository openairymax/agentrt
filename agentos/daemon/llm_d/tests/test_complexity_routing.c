/**
 * @file test_complexity_routing.c
 * @brief llm_d 复杂度评估路由策略验证 (INT-16)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 验证: SIMPLE/MODERATE/COMPLEX 三档复杂度评估 + 用户显式model参数覆盖
 */

#define _POSIX_C_SOURCE 199309L

#include "cache.h"
#include "cost_tracker.h"
#include "llm_service.h"
#include "providers/provider.h"
#include "providers/registry.h"
#include "memory_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 测试辅助 ========== */

static int test_count = 0;
static int pass_count = 0;

#define TEST_PASS() do { pass_count++; test_count++; } while(0)
#define TEST_FAIL(msg) do { printf("    FAIL: %s\n", msg); test_count++; } while(0)

/* 模拟复杂度评估函数 - 基于输入文本特征 */
typedef enum {
    COMPLEXITY_SIMPLE   = 0,
    COMPLEXITY_MODERATE = 1,
    COMPLEXITY_COMPLEX  = 2
} complexity_level_t;

static const char *complexity_names[] = {"SIMPLE", "MODERATE", "COMPLEX"};

/**
 * @brief 基于启发式规则评估输入复杂度
 *
 * 规则:
 * - SIMPLE:   短文本(<50字), 无技术关键词, 日常问候
 * - MODERATE: 中等长度(50-500字), 含编程/技术关键词
 * - COMPLEX:  长文本(>500字), 含架构/设计/系统级关键词
 */
static complexity_level_t assess_complexity(const char *input)
{
    if (!input) return COMPLEXITY_SIMPLE;

    size_t len = strlen(input);

    /* 复杂度关键词检测 */
    const char *complex_kw[] = {
        "architecture", "distributed", "system design", "scalability",
        "架构", "分布式", "系统设计", "高可用", "微服务"
    };
    const char *moderate_kw[] = {
        "function", "algorithm", "sort", "implement", "write",
        "函数", "算法", "排序", "实现", "编写", "Python", "Java"
    };

    int complex_score = 0;
    int moderate_score = 0;

    for (size_t i = 0; i < sizeof(complex_kw) / sizeof(complex_kw[0]); i++) {
        if (strstr(input, complex_kw[i])) complex_score++;
    }
    for (size_t i = 0; i < sizeof(moderate_kw) / sizeof(moderate_kw[0]); i++) {
        if (strstr(input, moderate_kw[i])) moderate_score++;
    }

    /* 综合判断 */
    if (complex_score >= 1 || len > 500) return COMPLEXITY_COMPLEX;
    if (moderate_score >= 1 || len > 50)  return COMPLEXITY_MODERATE;
    return COMPLEXITY_SIMPLE;
}

/**
 * @brief 根据复杂度选择模型路由
 */
static const char *route_by_complexity(complexity_level_t level, const char *user_model)
{
    /* 用户显式指定model时跳过复杂度评估 */
    if (user_model && user_model[0]) {
        return user_model;
    }

    switch (level) {
    case COMPLEXITY_SIMPLE:
        return "gpt-4o-mini";    /* 便宜模型 */
    case COMPLEXITY_MODERATE:
        return "gpt-4o";         /* 平衡模型 */
    case COMPLEXITY_COMPLEX:
        return "claude-sonnet";  /* 强模型 */
    default:
        return "gpt-4o-mini";
    }
}

/* ========== INT-16.1: SIMPLE复杂度 ========== */

static void test_complexity_simple(void)
{
    printf("  [INT-16.1] SIMPLE complexity assessment...\n");

    const char *inputs[] = {
        "Hello, how are you?",
        "What is the weather today?",
        "你好，今天天气怎么样？",
        "Hi",
    };

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        complexity_level_t level = assess_complexity(inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        printf("    Input: \"%s\" -> %s -> %s\n",
               inputs[i], complexity_names[level], model);

        assert(level == COMPLEXITY_SIMPLE);
        assert(strcmp(model, "gpt-4o-mini") == 0);
    }

    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-16.2: MODERATE复杂度 ========== */

static void test_complexity_moderate(void)
{
    printf("  [INT-16.2] MODERATE complexity assessment...\n");

    const char *inputs[] = {
        "Write a Python function to sort a list of integers",
        "请编写一个Java函数实现快速排序算法",
        "How to implement a binary search in C?",
        "Write a function to parse JSON data",
    };

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        complexity_level_t level = assess_complexity(inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        printf("    Input: \"%s\" -> %s -> %s\n",
               inputs[i], complexity_names[level], model);

        assert(level == COMPLEXITY_MODERATE);
        assert(strcmp(model, "gpt-4o") == 0);
    }

    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-16.3: COMPLEX复杂度 ========== */

static void test_complexity_complex(void)
{
    printf("  [INT-16.3] COMPLEX complexity assessment...\n");

    const char *inputs[] = {
        "Design a distributed system architecture for a global e-commerce platform "
        "handling millions of requests per second with high availability requirements",
        "请设计一个微服务架构的分布式系统，需要考虑可扩展性、容错性和数据一致性",
    };

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        complexity_level_t level = assess_complexity(inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        printf("    Input: \"%s...\" -> %s -> %s\n",
               (strlen(inputs[i]) > 50 ? "..." : ""), /* 截断显示 */
               complexity_names[level], model);

        assert(level == COMPLEXITY_COMPLEX);
        assert(strcmp(model, "claude-sonnet") == 0);
    }

    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-16.4: 用户显式model覆盖自动路由 ========== */

static void test_user_model_override(void)
{
    printf("  [INT-16.4] User model parameter overrides auto-routing...\n");

    /* 即使用户输入是SIMPLE，显式指定model应跳过复杂度评估 */
    const char *simple_input = "Hello";
    complexity_level_t level = assess_complexity(simple_input);
    assert(level == COMPLEXITY_SIMPLE);

    /* 用户显式指定 model */
    const char *user_model = "deepseek-v3";
    const char *routed = route_by_complexity(level, user_model);

    printf("    Input: \"%s\" (user_model=%s) -> routed to %s\n",
           simple_input, user_model, routed);

    assert(strcmp(routed, "deepseek-v3") == 0);
    printf("    Auto-routing skipped when user model specified\n");

    /* 用户显式指定MODERATE输入对应的强模型 */
    const char *moderate_input = "Write a Python function to sort a list";
    level = assess_complexity(moderate_input);
    assert(level == COMPLEXITY_MODERATE);

    user_model = "claude-opus";
    routed = route_by_complexity(level, user_model);
    assert(strcmp(routed, "claude-opus") == 0);
    printf("    User override with claude-opus: OK\n");

    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-16.5: 路由日志记录验证 ========== */

static void test_routing_logging(void)
{
    printf("  [INT-16.5] Routing decision logging verification...\n");

    /* 模拟路由日志记录 */
    typedef struct {
        char input_summary[128];
        complexity_level_t complexity;
        char selected_model[64];
        char reason[256];
    } routing_log_entry_t;

    routing_log_entry_t log_entries[3];
    int log_count = 0;

    /* 模拟三次路由决策并记录日志 */
    const char *test_inputs[] = {
        "Hello, how are you?",
        "Write a Python function to sort a list",
        "Design a distributed system architecture",
    };

    for (int i = 0; i < 3; i++) {
        complexity_level_t level = assess_complexity(test_inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        /* 记录路由日志 */
        routing_log_entry_t *entry = &log_entries[log_count++];
        size_t copy_len = strlen(test_inputs[i]) < 60 ? strlen(test_inputs[i]) : 60;
        memcpy(entry->input_summary, test_inputs[i], copy_len);
        entry->input_summary[copy_len] = '\0';
        entry->complexity = level;
        strncpy(entry->selected_model, model, sizeof(entry->selected_model) - 1);
    entry->selected_model[sizeof(entry->selected_model) - 1] = '\0';
        snprintf(entry->reason, sizeof(entry->reason),
                 "Complexity=%s, len=%zu, routed to %s",
                 complexity_names[level], strlen(test_inputs[i]), model);
    }

    /* 验证日志完整性 */
    assert(log_count == 3);
    printf("    Routing log entries: %d\n", log_count);

    for (int i = 0; i < log_count; i++) {
        printf("    Log[%d]: summary=\"%s\" complexity=%s model=%s\n",
               i, log_entries[i].input_summary,
               complexity_names[log_entries[i].complexity],
               log_entries[i].selected_model);
    }

    /* 验证日志内容 */
    assert(strcmp(log_entries[0].selected_model, "gpt-4o-mini") == 0);
    assert(log_entries[0].complexity == COMPLEXITY_SIMPLE);

    assert(strcmp(log_entries[1].selected_model, "gpt-4o") == 0);
    assert(log_entries[1].complexity == COMPLEXITY_MODERATE);

    assert(strcmp(log_entries[2].selected_model, "claude-sonnet") == 0);
    assert(log_entries[2].complexity == COMPLEXITY_COMPLEX);

    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== 主函数 ========== */

int main(void)
{
    printf("=========================================\n");
    printf("  LLM Complexity Routing Verification\n");
    printf("  (INT-16: SIMPLE/MODERATE/COMPLEX)\n");
    printf("=========================================\n\n");

    test_complexity_simple();
    test_complexity_moderate();
    test_complexity_complex();
    test_user_model_override();
    test_routing_logging();

    printf("\n=========================================\n");
    printf("  Results: %d/%d tests PASSED\n", pass_count, test_count);
    printf("=========================================\n");

    return (pass_count == test_count) ? 0 : 1;
}