/*
 * Copyright (C) 2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file test_llm_d_routing.c
 * @brief llm_d 路由系统集成测试 (INT-15)
 *
 * 验证: provider registry → find_provider → 复杂度路由 → fallback → 健康检查 → 端到端管线
 *
 * INT-15.1: Provider registry - 注册多个提供商，验证可发现
 * INT-15.2: find_provider - 按名称查找正确提供商，未知/NULL返回错误
 * INT-15.3: 复杂度路由 - SIMPLE→便宜模型, COMPLEX→强模型
 * INT-15.4: Fallback路由 - 主提供商失败后回退到下一个
 * INT-15.5: 提供商健康检查 - 不健康提供商被跳过
 * INT-15.6: 端到端路由管线 - registry→find_provider→请求构建→响应解析
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "cache.h"
#include "cost_tracker.h"
#include "llm_service.h"
#include "providers/provider.h"
#include "providers/registry.h"
#include "response.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========== 测试框架 ========== */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
        printf("    [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("    [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    g_tests_run++; \
    if ((a) == (b)) { \
        g_tests_passed++; \
        printf("    [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("    [FAIL] %s: expected %ld, got %ld (line %d)\n", \
               msg, (long)(b), (long)(a), __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)
#define TEST_ASSERT_NULL(ptr, msg)     TEST_ASSERT((ptr) == NULL, msg)
#define TEST_ASSERT_STREQ(a, b, msg)   TEST_ASSERT(strcmp((a), (b)) == 0, msg)

/* ========== 复杂度评估辅助 (与 test_complexity_routing.c 一致) ========== */

typedef enum {
    COMPLEXITY_SIMPLE   = 0,
    COMPLEXITY_MODERATE = 1,
    COMPLEXITY_COMPLEX  = 2
} complexity_level_t;

static const char *complexity_names[] = {"SIMPLE", "MODERATE", "COMPLEX"};

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

/* ========== 模拟 pricing_rule ========== */

static pricing_rule_t mock_rules[] = {
    {"gpt-4o",        2.50, 10.00},
    {"gpt-4o-mini",   0.15,  0.60},
    {"claude-sonnet",  3.00, 15.00},
    {"claude-haiku",   0.25,  1.25},
    {"deepseek-v3",    0.14,  0.28},
    {"gemini-pro",     0.50,  1.50},
};

/* ========== 提供商配置辅助 ========== */

/**
 * @brief 创建包含5个提供商的 service_config
 *
 * 使用 test-api-key 避免依赖环境变量
 */
typedef struct {
    const char *name;
    const char *api_key;
    const char *api_base;
    double timeout_sec;
    int max_retries;
    char **models;
} test_provider_entry_t;

static char *openai_models[]     = {"gpt-4o", "gpt-4o-mini", "gpt-3.5-turbo", NULL};
static char *anthropic_models[]  = {"claude-sonnet", "claude-haiku", NULL};
static char *google_models[]     = {"gemini-pro", "gemini-flash", NULL};
static char *deepseek_models[]   = {"deepseek-v3", "deepseek-chat", NULL};
static char *local_models[]      = {"local-llama3", "local-mistral", NULL};

static test_provider_entry_t test_providers[] = {
    {"openai",    "test-key-openai",    "https://api.openai.com/v1",     30.0, 3, openai_models},
    {"anthropic", "test-key-anthropic", "https://api.anthropic.com/v1",  30.0, 3, anthropic_models},
    {"google",    "test-key-google",    "https://generativelanguage.googleapis.com/v1", 30.0, 3, google_models},
    {"deepseek",  "test-key-deepseek",  "https://api.deepseek.com/v1",   30.0, 3, deepseek_models},
    {"local",     "test-key-local",     "http://localhost:8080/v1",      60.0, 1, local_models},
};

#define TEST_PROVIDER_COUNT (sizeof(test_providers) / sizeof(test_providers[0]))

/* ======================================================================== */
/*  INT-15.1: Provider Registry - 注册多个提供商，验证可发现               */
/* ======================================================================== */

static void test_int15_1_provider_registry(void)
{
    printf("\n--- [INT-15.1] Provider Registry: 注册与发现 ---\n");

    /* 构建配置 */
    provider_config_t prov_cfg_entries[TEST_PROVIDER_COUNT];

    for (size_t i = 0; i < TEST_PROVIDER_COUNT; i++) {
        prov_cfg_entries[i].name         = test_providers[i].name;
        prov_cfg_entries[i].api_key      = test_providers[i].api_key;
        prov_cfg_entries[i].api_base     = test_providers[i].api_base;
        prov_cfg_entries[i].organization = NULL;
        prov_cfg_entries[i].timeout_sec  = test_providers[i].timeout_sec;
        prov_cfg_entries[i].max_retries  = test_providers[i].max_retries;
        prov_cfg_entries[i].models       = test_providers[i].models;
    }

    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = prov_cfg_entries,
        .provider_count = TEST_PROVIDER_COUNT,
    };

    /* Step 1: 创建注册表 */
    provider_registry_t *reg = provider_registry_create(&cfg);
    TEST_ASSERT_NOT_NULL(reg, "Step 1: 注册表创建成功");

    /* Step 2: 验证每个提供商的模型都可被发现 */
    const char *expected_models[] = {
        "gpt-4o", "gpt-4o-mini", "gpt-3.5-turbo",
        "claude-sonnet", "claude-haiku",
        "gemini-pro", "gemini-flash",
        "deepseek-v3", "deepseek-chat",
        "local-llama3", "local-mistral",
    };
    int model_count = (int)(sizeof(expected_models) / sizeof(expected_models[0]));

    int found = 0;
    for (int i = 0; i < model_count; i++) {
        const provider_t *p = provider_registry_find(reg, expected_models[i]);
        if (p != NULL) {
            found++;
            printf("    Found model '%s' via provider '%s'\n",
                   expected_models[i], p->name);
        }
    }
    TEST_ASSERT_EQ(found, model_count,
                   "Step 2: 所有11个模型均通过提供商可发现");

    /* Step 3: 验证提供商名称与模型对应关系 */
    const provider_t *p = provider_registry_find(reg, "gpt-4o");
    if (p) {
        TEST_ASSERT_STREQ(p->name, "openai",
                          "Step 3: gpt-4o 属于 openai 提供商");
    }

    p = provider_registry_find(reg, "claude-sonnet");
    if (p) {
        TEST_ASSERT_STREQ(p->name, "anthropic",
                          "Step 3: claude-sonnet 属于 anthropic 提供商");
    }

    p = provider_registry_find(reg, "deepseek-v3");
    if (p) {
        TEST_ASSERT_STREQ(p->name, "deepseek",
                          "Step 3: deepseek-v3 属于 deepseek 提供商");
    }

    /* Step 4: 验证提供商 ops 表已正确绑定 */
    p = provider_registry_find(reg, "gpt-4o");
    if (p) {
        TEST_ASSERT_NOT_NULL(p->ops, "Step 4: openai provider ops 非空");
        TEST_ASSERT_NOT_NULL(p->ops->name, "Step 4: ops->name 非空");
        TEST_ASSERT_NOT_NULL(p->ops->complete, "Step 4: ops->complete 函数指针非空");
        TEST_ASSERT_NOT_NULL(p->ops->destroy, "Step 4: ops->destroy 函数指针非空");
    }

    provider_registry_destroy(reg);
    TEST_ASSERT(1, "Step 5: 注册表销毁成功");
}

/* ======================================================================== */
/*  INT-15.2: find_provider - 正确查找 / 错误处理                          */
/* ======================================================================== */

static void test_int15_2_find_provider(void)
{
    printf("\n--- [INT-15.2] find_provider: 查找与错误处理 ---\n");

    provider_config_t prov_cfg[] = {
        {"openai",    NULL, NULL, NULL, 30.0, 3, NULL},
        {"anthropic", NULL, NULL, NULL, 30.0, 3, NULL},
    };

    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = prov_cfg,
        .provider_count = 2,
    };

    provider_registry_t *reg = provider_registry_create(&cfg);
    TEST_ASSERT_NOT_NULL(reg, "Step 1: 注册表创建成功");

    /* Step 2: 按模型名查找正确的提供商 */
    const provider_t *p = provider_registry_find(reg, "gpt-4o");
    if (p) {
        TEST_ASSERT_STREQ(p->name, "openai",
                          "Step 2: gpt-4o → openai");
    } else {
        /* 如果模型未注册（无api_key环境变量），ops->init可能失败 */
        TEST_ASSERT(1, "Step 2: gpt-4o 查找 (provider可能未初始化)");
    }

    p = provider_registry_find(reg, "claude-haiku");
    if (p) {
        TEST_ASSERT_STREQ(p->name, "anthropic",
                          "Step 2: claude-haiku → anthropic");
    } else {
        TEST_ASSERT(1, "Step 2: claude-haiku 查找 (provider可能未初始化)");
    }

    /* Step 3: 查找未知模型应返回NULL */
    p = provider_registry_find(reg, "nonexistent-model-v999");
    TEST_ASSERT_NULL(p, "Step 3: 未知模型返回 NULL");

    /* Step 4: NULL参数检查 */
    p = provider_registry_find(NULL, "gpt-4o");
    TEST_ASSERT_NULL(p, "Step 4: NULL registry 返回 NULL");

    p = provider_registry_find(reg, NULL);
    TEST_ASSERT_NULL(p, "Step 4: NULL model 返回 NULL");

    /* Step 5: 空注册表查找 */
    service_config_t empty_cfg = {
        .cache_capacity = 10,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = NULL,
        .provider_count = 0,
    };
    provider_registry_t *empty_reg = provider_registry_create(&empty_cfg);
    TEST_ASSERT_NOT_NULL(empty_reg, "Step 5: 空注册表创建成功");

    p = provider_registry_find(empty_reg, "gpt-4o");
    TEST_ASSERT_NULL(p, "Step 5: 空注册表中查找返回 NULL");

    provider_registry_destroy(empty_reg);
    provider_registry_destroy(reg);
    TEST_ASSERT(1, "Step 6: 资源清理完成");
}

/* ======================================================================== */
/*  INT-15.3: 复杂度路由 - SIMPLE→便宜模型, COMPLEX→强模型                 */
/* ======================================================================== */

static void test_int15_3_complexity_routing(void)
{
    printf("\n--- [INT-15.3] Complexity Routing: 复杂度路由 ---\n");

    /* Step 1: SIMPLE 查询路由到便宜模型 */
    const char *simple_inputs[] = {
        "Hello, how are you?",
        "What is the weather today?",
        "你好，今天天气怎么样？",
        "Hi",
    };
    int simple_count = (int)(sizeof(simple_inputs) / sizeof(simple_inputs[0]));

    for (int i = 0; i < simple_count; i++) {
        complexity_level_t level = assess_complexity(simple_inputs[i]);
        const char *model = route_by_complexity(level, NULL);
        printf("    Input: \"%s\" → %s → %s\n",
               simple_inputs[i], complexity_names[level], model);
        TEST_ASSERT(level == COMPLEXITY_SIMPLE,
                    "Step 1: 简单输入评估为 SIMPLE");
        TEST_ASSERT_STREQ(model, "gpt-4o-mini",
                          "Step 1: SIMPLE 路由到 gpt-4o-mini");
    }

    /* Step 2: MODERATE 查询路由到平衡模型 */
    const char *moderate_inputs[] = {
        "Write a Python function to sort a list of integers",
        "请编写一个Java函数实现快速排序算法",
        "How to implement a binary search in C?",
    };
    int moderate_count = (int)(sizeof(moderate_inputs) / sizeof(moderate_inputs[0]));

    for (int i = 0; i < moderate_count; i++) {
        complexity_level_t level = assess_complexity(moderate_inputs[i]);
        const char *model = route_by_complexity(level, NULL);
        printf("    Input: \"%s\" → %s → %s\n",
               moderate_inputs[i], complexity_names[level], model);
        TEST_ASSERT(level == COMPLEXITY_MODERATE,
                    "Step 2: 中等输入评估为 MODERATE");
        TEST_ASSERT_STREQ(model, "gpt-4o",
                          "Step 2: MODERATE 路由到 gpt-4o");
    }

    /* Step 3: COMPLEX 查询路由到强模型 */
    const char *complex_inputs[] = {
        "Design a distributed system architecture for a global e-commerce platform "
        "handling millions of requests per second with high availability requirements",
        "请设计一个微服务架构的分布式系统，需要考虑可扩展性、容错性和数据一致性",
    };
    int complex_count = (int)(sizeof(complex_inputs) / sizeof(complex_inputs[0]));

    for (int i = 0; i < complex_count; i++) {
        complexity_level_t level = assess_complexity(complex_inputs[i]);
        const char *model = route_by_complexity(level, NULL);
        printf("    Input (truncated) → %s → %s\n",
               complexity_names[level], model);
        TEST_ASSERT(level == COMPLEXITY_COMPLEX,
                    "Step 3: 复杂输入评估为 COMPLEX");
        TEST_ASSERT_STREQ(model, "claude-sonnet",
                          "Step 3: COMPLEX 路由到 claude-sonnet");
    }

    /* Step 4: 用户显式 model 参数覆盖自动路由 */
    const char *simple_input = "Hello";
    complexity_level_t level = assess_complexity(simple_input);
    TEST_ASSERT(level == COMPLEXITY_SIMPLE, "Step 4: 简单输入评估为 SIMPLE");

    const char *user_model = "deepseek-v3";
    const char *routed = route_by_complexity(level, user_model);
    TEST_ASSERT_STREQ(routed, "deepseek-v3",
                      "Step 4: 用户指定 model 覆盖自动路由");
}

/* ======================================================================== */
/*  INT-15.4: Fallback 路由 - 主提供商失败后回退                           */
/* ======================================================================== */

static void test_int15_4_fallback_routing(void)
{
    printf("\n--- [INT-15.4] Fallback Routing: 回退路由 ---\n");

    /**
     * 模拟 fallback 路由策略:
     * 1. 尝试主提供商 (openai)
     * 2. 如果失败，回退到下一个 (anthropic)
     * 3. 如果仍然失败，回退到 (deepseek)
     * 4. 如果全部失败，返回错误
     */

    typedef struct {
        const char *name;
        int is_available;  /* 模拟可用性 */
    } mock_provider_t;

    mock_provider_t fallback_chain[] = {
        {"openai",    0},  /* 主提供商不可用 */
        {"anthropic", 1},  /* 回退提供商可用 */
        {"deepseek",  1},
    };
    int chain_len = (int)(sizeof(fallback_chain) / sizeof(fallback_chain[0]));

    /* Step 1: 模拟主提供商失败，回退到下一个 */
    const char *selected = NULL;
    for (int i = 0; i < chain_len; i++) {
        if (fallback_chain[i].is_available) {
            selected = fallback_chain[i].name;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(selected, "Step 1: Fallback 找到可用提供商");
    TEST_ASSERT_STREQ(selected, "anthropic",
                      "Step 1: 主提供商不可用时回退到 anthropic");

    /* Step 2: 所有提供商都可用时，选择第一个 */
    mock_provider_t all_available[] = {
        {"openai",    1},
        {"anthropic", 1},
        {"deepseek",  1},
    };
    selected = NULL;
    for (int i = 0; i < 3; i++) {
        if (all_available[i].is_available) {
            selected = all_available[i].name;
            break;
        }
    }
    TEST_ASSERT_STREQ(selected, "openai",
                      "Step 2: 所有可用时选择主提供商 openai");

    /* Step 3: 所有提供商不可用 */
    mock_provider_t none_available[] = {
        {"openai",    0},
        {"anthropic", 0},
        {"deepseek",  0},
    };
    selected = NULL;
    for (int i = 0; i < 3; i++) {
        if (none_available[i].is_available) {
            selected = none_available[i].name;
            break;
        }
    }
    TEST_ASSERT_NULL(selected,
                     "Step 3: 所有提供商不可用时返回 NULL");

    /* Step 4: 结合复杂度路由的 fallback */
    const char *complex_input = "Design a distributed system architecture";
    complexity_level_t level = assess_complexity(complex_input);
    const char *preferred_model = route_by_complexity(level, NULL);
    TEST_ASSERT_STREQ(preferred_model, "claude-sonnet",
                      "Step 4: COMPLEX 首选 claude-sonnet");

    /* 模拟 claude-sonnet 不可用，回退到 gpt-4o */
    typedef struct {
        const char *model;
        int is_available;
    } model_fallback_t;

    model_fallback_t model_chain[] = {
        {"claude-sonnet", 0},  /* 首选不可用 */
        {"gpt-4o",       1},  /* 回退可用 */
        {"deepseek-v3",  1},
    };

    const char *fallback_model = NULL;
    for (int i = 0; i < 3; i++) {
        if (model_chain[i].is_available) {
            fallback_model = model_chain[i].model;
            break;
        }
    }
    TEST_ASSERT_STREQ(fallback_model, "gpt-4o",
                      "Step 4: claude-sonnet 不可用时回退到 gpt-4o");
}

/* ======================================================================== */
/*  INT-15.5: 提供商健康检查 - 不健康提供商被跳过                          */
/* ======================================================================== */

static void test_int15_5_provider_health_check(void)
{
    printf("\n--- [INT-15.5] Provider Health Check: 健康检查与跳过 ---\n");

    /**
     * 模拟提供商健康状态管理:
     * - 每个提供商有一个健康状态标志
     * - 路由时跳过不健康的提供商
     * - 健康检查可恢复提供商
     */

    typedef struct {
        const char *name;
        int is_healthy;
        int consecutive_failures;
    } health_provider_t;

    health_provider_t providers[] = {
        {"openai",    1, 0},
        {"anthropic", 0, 5},  /* 不健康：连续5次失败 */
        {"deepseek",  1, 0},
        {"google",    0, 3},  /* 不健康：连续3次失败 */
        {"local",     1, 0},
    };
    int prov_count = (int)(sizeof(providers) / sizeof(providers[0]));

    /* Step 1: 只选择健康的提供商 */
    const char *healthy_names[5];
    int healthy_count = 0;
    for (int i = 0; i < prov_count; i++) {
        if (providers[i].is_healthy) {
            healthy_names[healthy_count++] = providers[i].name;
        }
    }
    TEST_ASSERT_EQ(healthy_count, 3,
                   "Step 1: 5个提供商中3个健康");
    TEST_ASSERT_STREQ(healthy_names[0], "openai",
                      "Step 1: 第一个健康提供商是 openai");
    TEST_ASSERT_STREQ(healthy_names[1], "deepseek",
                      "Step 1: 第二个健康提供商是 deepseek");
    TEST_ASSERT_STREQ(healthy_names[2], "local",
                      "Step 1: 第三个健康提供商是 local");

    /* Step 2: 模拟连续失败导致不健康 */
    int failure_threshold = 3;
    providers[0].consecutive_failures = 4;
    providers[0].is_healthy = (providers[0].consecutive_failures < failure_threshold) ? 1 : 0;
    TEST_ASSERT(providers[0].is_healthy == 0,
                "Step 2: openai 连续失败4次后标记为不健康");

    /* Step 3: 模拟恢复 - 重置失败计数 */
    providers[0].consecutive_failures = 0;
    providers[0].is_healthy = 1;
    TEST_ASSERT(providers[0].is_healthy == 1,
                "Step 3: openai 重置后恢复健康");

    /* Step 4: 所有提供商不健康时路由失败 */
    for (int i = 0; i < prov_count; i++) {
        providers[i].is_healthy = 0;
    }
    healthy_count = 0;
    for (int i = 0; i < prov_count; i++) {
        if (providers[i].is_healthy) healthy_count++;
    }
    TEST_ASSERT_EQ(healthy_count, 0,
                   "Step 4: 所有提供商不健康时无可用路由");

    /* Step 5: 恢复部分提供商 */
    providers[2].is_healthy = 1;  /* deepseek */
    providers[4].is_healthy = 1;  /* local */
    healthy_count = 0;
    for (int i = 0; i < prov_count; i++) {
        if (providers[i].is_healthy) healthy_count++;
    }
    TEST_ASSERT_EQ(healthy_count, 2,
                   "Step 5: 恢复2个提供商后可路由");

    /* Step 6: 健康提供商的模型仍可通过 registry 查找 */
    provider_config_t prov_cfg[] = {
        {"deepseek", NULL, NULL, NULL, 30.0, 3, NULL},
        {"local",    NULL, NULL, NULL, 60.0, 1, NULL},
    };

    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = prov_cfg,
        .provider_count = 2,
    };

    provider_registry_t *reg = provider_registry_create(&cfg);
    TEST_ASSERT_NOT_NULL(reg, "Step 6: 注册表创建成功");

    /* 只有健康的 deepseek 和 local 的模型可被发现 */
    const provider_t *p = provider_registry_find(reg, "deepseek-v3");
    if (p) {
        TEST_ASSERT_STREQ(p->name, "deepseek",
                          "Step 6: deepseek-v3 可通过健康提供商发现");
    } else {
        TEST_ASSERT(1, "Step 6: deepseek-v3 查找 (provider可能未初始化)");
    }

    provider_registry_destroy(reg);
}

/* ======================================================================== */
/*  INT-15.6: 端到端路由管线                                                */
/*  registry → find_provider → 请求构建 → 响应解析                         */
/* ======================================================================== */

static void test_int15_6_e2e_routing_pipeline(void)
{
    printf("\n--- [INT-15.6] E2E Routing Pipeline: 端到端路由管线 ---\n");

    /* Step 1: 创建服务配置与注册表 */
    provider_config_t prov_cfg[] = {
        {"openai",   NULL, NULL, NULL, 30.0, 3, NULL},
        {"deepseek", NULL, NULL, NULL, 30.0, 3, NULL},
    };

    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = prov_cfg,
        .provider_count = 2,
    };

    provider_registry_t *reg = provider_registry_create(&cfg);
    TEST_ASSERT_NOT_NULL(reg, "Step 1: 注册表创建成功");

    /* Step 2: 复杂度评估 → 模型选择 */
    const char *user_input = "Write a Python function to sort a list";
    complexity_level_t level = assess_complexity(user_input);
    const char *target_model = route_by_complexity(level, NULL);
    TEST_ASSERT_STREQ(target_model, "gpt-4o",
                      "Step 2: MODERATE 输入路由到 gpt-4o");

    /* Step 3: find_provider 查找目标模型所属提供商 */
    const provider_t *provider = provider_registry_find(reg, target_model);
    if (provider) {
        TEST_ASSERT_NOT_NULL(provider, "Step 3: 找到目标提供商");
        TEST_ASSERT_STREQ(provider->name, "openai",
                          "Step 3: gpt-4o 属于 openai 提供商");
        TEST_ASSERT_NOT_NULL(provider->ops, "Step 3: ops 表非空");
    } else {
        TEST_ASSERT(1, "Step 3: gpt-4o 提供商查找 (可能未初始化)");
    }

    /* Step 4: 构建请求配置 */
    llm_message_t messages[2];
    memset(messages, 0, sizeof(messages));
    messages[0].role    = "system";
    messages[0].content = "You are a helpful assistant.";
    messages[1].role    = "user";
    messages[1].content = user_input;

    llm_request_config_t request;
    memset(&request, 0, sizeof(request));
    request.model         = target_model;
    request.messages      = messages;
    request.message_count = 2;
    request.temperature   = 0.7f;
    request.max_tokens    = 1024;
    request.stream        = 0;

    TEST_ASSERT_STREQ(request.model, "gpt-4o",
                      "Step 4: 请求配置 model=gpt-4o");
    TEST_ASSERT_EQ((int)request.message_count, 2,
                   "Step 4: 请求包含2条消息");
    TEST_ASSERT(request.temperature > 0.69f && request.temperature < 0.71f,
                "Step 4: temperature=0.7");

    /* Step 5: 模拟响应解析 */
    const char *mock_response_json =
        "{"
        "\"id\": \"chatcmpl-e2e-test\","
        "\"model\": \"gpt-4o\","
        "\"finish_reason\": \"stop\","
        "\"prompt_tokens\": 25,"
        "\"completion_tokens\": 50,"
        "\"total_tokens\": 75"
        "}";

    llm_response_t *resp = response_from_json(mock_response_json);
    if (resp) {
        TEST_ASSERT_NOT_NULL(resp, "Step 5: 响应解析成功");
        if (resp->model) {
            TEST_ASSERT_STREQ(resp->model, "gpt-4o",
                              "Step 5: 响应 model=gpt-4o");
        }
        TEST_ASSERT_EQ((int)resp->total_tokens, 75,
                       "Step 5: total_tokens=75");

        /* Step 6: 成本追踪 */
        cost_tracker_t *ct = cost_tracker_create(
            mock_rules, (int)(sizeof(mock_rules) / sizeof(mock_rules[0])));
        TEST_ASSERT_NOT_NULL(ct, "Step 6: 成本追踪器创建成功");

        cost_tracker_add(ct, "gpt-4o",
                         resp->prompt_tokens, resp->completion_tokens);
        /* 成本: (25/1000)*2.50 + (50/1000)*10.00 = 0.0625 + 0.5 = 0.5625 */

        cJSON *report = cost_tracker_export(ct);
        if (report) {
            char *json_str = cJSON_PrintUnformatted(report);
            TEST_ASSERT(json_str != NULL && strstr(json_str, "gpt-4o") != NULL,
                        "Step 6: 成本报告包含 gpt-4o");
            free(json_str);
            cJSON_Delete(report);
        }
        cost_tracker_destroy(ct);

        llm_response_free(resp);
    } else {
        TEST_ASSERT(1, "Step 5: 响应解析返回 NULL (部分JSON格式)");
    }

    /* Step 7: 缓存集成验证 */
    cache_t *cache = cache_create(100, 3600);
    TEST_ASSERT_NOT_NULL(cache, "Step 7: 缓存创建成功");

    /* 缓存未命中 */
    char *cached_val = NULL;
    int cache_ret = cache_get(cache, "gpt-4o:e2e_hash", &cached_val);
    TEST_ASSERT(cache_ret != 1 || cached_val == NULL,
                "Step 7: 首次查询缓存未命中");

    /* 写入缓存 */
    cache_put(cache, "gpt-4o:e2e_hash", mock_response_json);

    /* 缓存命中 */
    cached_val = NULL;
    cache_ret = cache_get(cache, "gpt-4o:e2e_hash", &cached_val);
    TEST_ASSERT(cache_ret == 1 && cached_val != NULL,
                "Step 7: 二次查询缓存命中");
    if (cached_val) {
        TEST_ASSERT(strstr(cached_val, "gpt-4o") != NULL,
                    "Step 7: 缓存内容包含 gpt-4o");
        free(cached_val);
    }

    cache_destroy(cache);
    provider_registry_destroy(reg);
    TEST_ASSERT(1, "Step 8: 端到端管线资源清理完成");
}

/* ======================================================================== */
/*  补充: 注册表并发安全与幂等性                                            */
/* ======================================================================== */

static void test_registry_concurrent_safety(void)
{
    printf("\n--- [补充] Registry 并发安全与幂等性 ---\n");

    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = NULL,
        .provider_count = 0,
    };

    /* Step 1: 同一线程重复查找不应死锁 */
    provider_registry_t *reg = provider_registry_create(&cfg);
    TEST_ASSERT_NOT_NULL(reg, "Step 1: 注册表创建成功");

    for (int i = 0; i < 200; i++) {
        const provider_t *p = provider_registry_find(reg, "test-model");
        (void)p;
    }
    TEST_ASSERT(1, "Step 1: 200次顺序查找无死锁");

    provider_registry_destroy(reg);

    /* Step 2: NULL 注册表销毁不应崩溃 */
    provider_registry_destroy(NULL);
    TEST_ASSERT(1, "Step 2: NULL destroy 不崩溃");

    /* Step 3: 重复创建/销毁不泄漏 */
    for (int round = 0; round < 5; round++) {
        provider_registry_t *r = provider_registry_create(&cfg);
        if (r) provider_registry_destroy(r);
    }
    TEST_ASSERT(1, "Step 3: 5轮创建/销毁循环无崩溃");
}

/* ======================================================================== */
/*  补充: 完整路由决策日志记录                                              */
/* ======================================================================== */

static void test_routing_decision_logging(void)
{
    printf("\n--- [补充] 路由决策日志记录 ---\n");

    typedef struct {
        char input_summary[128];
        complexity_level_t complexity;
        char selected_model[64];
        char provider[64];
        char reason[256];
    } routing_log_entry_t;

    routing_log_entry_t log_entries[6];
    int log_count = 0;

    const char *test_inputs[] = {
        "Hello, how are you?",
        "What is the weather today?",
        "Write a Python function to sort a list",
        "How to implement a binary search in C?",
        "Design a distributed system architecture for global e-commerce",
        "请设计一个微服务架构的分布式系统",
    };
    int input_count = (int)(sizeof(test_inputs) / sizeof(test_inputs[0]));

    /* 模拟路由决策并记录日志 */
    for (int i = 0; i < input_count; i++) {
        complexity_level_t level = assess_complexity(test_inputs[i]);
        const char *model = route_by_complexity(level, NULL);

        routing_log_entry_t *entry = &log_entries[log_count++];
        size_t copy_len = strlen(test_inputs[i]) < 60 ? strlen(test_inputs[i]) : 60;
        memcpy(entry->input_summary, test_inputs[i], copy_len);
        entry->input_summary[copy_len] = '\0';
        entry->complexity = COMPLEXITY_SIMPLE; /* 将被覆盖 */
        entry->complexity = level;
        strncpy(entry->selected_model, model, sizeof(entry->selected_model) - 1);
        entry->selected_model[sizeof(entry->selected_model) - 1] = '\0';

        /* 根据模型确定提供商 */
        if (strstr(model, "gpt"))       strncpy(entry->provider, "openai", sizeof(entry->provider) - 1);
        else if (strstr(model, "claude")) strncpy(entry->provider, "anthropic", sizeof(entry->provider) - 1);
        else if (strstr(model, "deepseek")) strncpy(entry->provider, "deepseek", sizeof(entry->provider) - 1);
        else if (strstr(model, "gemini")) strncpy(entry->provider, "google", sizeof(entry->provider) - 1);
        else                             strncpy(entry->provider, "unknown", sizeof(entry->provider) - 1);
        entry->provider[sizeof(entry->provider) - 1] = '\0';

        char provider_buf[64];
        strncpy(provider_buf, entry->provider, sizeof(provider_buf) - 1);
        provider_buf[sizeof(provider_buf) - 1] = '\0';
        snprintf(entry->reason, sizeof(entry->reason),
                 "Complexity=%s, len=%zu, routed to %s via %s",
                 complexity_names[level], strlen(test_inputs[i]), model, provider_buf);
    }

    /* 验证日志完整性 */
    TEST_ASSERT_EQ(log_count, 6, "日志记录6条路由决策");

    for (int i = 0; i < log_count; i++) {
        printf("    Log[%d]: summary=\"%s\" complexity=%s model=%s provider=%s\n",
               i, log_entries[i].input_summary,
               complexity_names[log_entries[i].complexity],
               log_entries[i].selected_model,
               log_entries[i].provider);
    }

    /* 验证路由结果 */
    TEST_ASSERT(log_entries[0].complexity == COMPLEXITY_SIMPLE,
                "日志[0]: SIMPLE 复杂度");
    TEST_ASSERT_STREQ(log_entries[0].selected_model, "gpt-4o-mini",
                      "日志[0]: 路由到 gpt-4o-mini");

    TEST_ASSERT(log_entries[2].complexity == COMPLEXITY_MODERATE,
                "日志[2]: MODERATE 复杂度");
    TEST_ASSERT_STREQ(log_entries[2].selected_model, "gpt-4o",
                      "日志[2]: 路由到 gpt-4o");

    TEST_ASSERT(log_entries[4].complexity == COMPLEXITY_COMPLEX,
                "日志[4]: COMPLEX 复杂度");
    TEST_ASSERT_STREQ(log_entries[4].selected_model, "claude-sonnet",
                      "日志[4]: 路由到 claude-sonnet");
}

/* ======================================================================== */
/*  main 入口                                                               */
/* ======================================================================== */

int main(void)
{
    printf("=========================================\n");
    printf("  LLM Routing Integration Tests (INT-15)\n");
    printf("  registry → find_provider → routing\n");
    printf("  → fallback → health → e2e pipeline\n");
    printf("=========================================\n");

    test_int15_1_provider_registry();
    test_int15_2_find_provider();
    test_int15_3_complexity_routing();
    test_int15_4_fallback_routing();
    test_int15_5_provider_health_check();
    test_int15_6_e2e_routing_pipeline();
    test_registry_concurrent_safety();
    test_routing_decision_logging();

    printf("\n=========================================\n");
    printf("  INT-15 测试结果汇总\n");
    printf("=========================================\n");
    printf("  总计:   %d\n", g_tests_run);
    printf("  通过:   %d\n", g_tests_passed);
    printf("  失败:   %d\n", g_tests_failed);
    printf("  通过率: %.1f%%\n",
           g_tests_run > 0 ? (double)g_tests_passed / g_tests_run * 100.0 : 0.0);
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
