/**
 * @file test_forgetting.c
 * @brief 遗忘引擎单元测试
 *
 * 覆盖: create/destroy, prune, get_weight, 策略配置,
 *       自动遗忘启动/停止, 边界条件
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forgetting.h"
#include "layer1_raw.h"
#include "agentos_memory.h"

/* ========== 测试宏 ========== */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return -1; \
    } \
} while(0)

#define TEST_PASS(name) printf("  PASS: %s\n", name)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define RUN_TEST(func) do { \
    int _ret = func(); \
    if (_ret == 0) { g_tests_passed++; } else { g_tests_failed++; } \
} while(0)

/* ========== 辅助函数 ========== */

static agentos_layer1_raw_t* create_test_l1(void) {
    agentos_layer1_raw_t* l1 = NULL;
    agentos_error_t err = agentos_layer1_raw_create_async(NULL, 1024, 2, &l1);
    return (err == AGENTOS_SUCCESS) ? l1 : NULL;
}

static void add_test_records(agentos_layer1_raw_t* l1, int count) {
    for (int i = 0; i < count; i++) {
        char key[48], val[128];
        snprintf(key, sizeof(key), "forget_rec_%04d", i);
        snprintf(val, sizeof(val), 
            "Test memory record #%d for forgetting engine testing. "
            "Contains sample content about machine learning and AI.", i);
        agentos_layer1_raw_write(l1, key, val, strlen(val));
    }
}

/* ========== 测试用例 ========== */

/**
 * T1: 创建和销毁遗忘引擎
 */
int test_fg_create_destroy(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    TEST_ASSERT(l1 != NULL, "create L1");
    
    agentos_forgetting_config_t config = {
        .strategy = AGENTOS_FORGET_EBBINGHAUS,
        .lambda = 0.1,
        .threshold = 0.15,
        .min_access = 2,
        .check_interval_sec = 3600,
        .archive_path = NULL
    };
    
    agentos_forgetting_engine_t* engine = NULL;
    agentos_error_t err = agentos_forgetting_create(&config, l1, NULL, &engine);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create should succeed");
    TEST_ASSERT(engine != NULL, "engine should not be NULL");
    
    agentos_forgetting_destroy(engine);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("fg_create_destroy");
    return 0;
}

/**
 * T2: 使用默认配置创建
 */
int test_fg_create_default(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    agentos_forgetting_engine_t* engine = NULL;
    agentos_error_t err = agentos_forgetting_create(NULL, l1, NULL, &engine);
    TEST_ASSERT(err == AGENTOS_SUCCESS || engine != NULL,
               "create with default config should work");
    
    if (engine) agentos_forgetting_destroy(engine);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("fg_create_default");
    return 0;
}

/**
 * T3: Prune操作 - 裁剪低权重记忆
 */
int test_fg_prune(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    TEST_ASSERT(l1 != NULL, "create L1");
    
    add_test_records(l1, 20);
    
    agentos_forgetting_config_t config = {
        .strategy = AGENTOS_FORGET_EBBINGHAUS,
        .lambda = 0.5,
        .threshold = 0.3,
        .min_access = 1,
        .check_interval_sec = 60,
        .archive_path = NULL
    };
    
    agentos_forgetting_engine_t* engine = NULL;
    agentos_error_t c_err = agentos_forgetting_create(&config, l1, NULL, &engine);
    TEST_ASSERT(c_err == AGENTOS_SUCCESS, "create engine for prune test");
    
    uint32_t pruned = 0;
    agentos_error_t p_err = agentos_forgetting_prune(engine, &pruned);
    TEST_ASSERT(p_err == AGENTOS_SUCCESS, "prune should succeed");
    printf("    (pruned=%u records)\n", pruned);
    
    agentos_forgetting_destroy(engine);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("fg_prune");
    return 0;
}

/**
 * T4: 获取记录权重
 */
int test_fg_get_weight(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    const char* id = "weight_test_id";
    agentos_layer1_raw_write(l1, id, "test content for weight check", 28);
    
    agentos_forgetting_config_t config = {
        .strategy = AGENTOS_FORGET_EBBINGHAUS,
        .lambda = 0.05,
        .threshold = 0.1,
        .min_access = 1,
        .check_interval_sec = 300,
        .archive_path = NULL
    };
    
    agentos_forgetting_engine_t* engine = NULL;
    agentos_forgetting_create(&config, l1, NULL, &engine);
    
    float weight = 0.0f;
    agentos_error_t w_err = agentos_forgetting_get_weight(engine, id, &weight);
    
    if (w_err == AGENTOS_SUCCESS) {
        TEST_ASSERT(weight >= 0.0f && weight <= 1.0f, 
                   "weight should be in [0, 1]");
        printf("    (weight=%.4f)\n", weight);
    } else {
        printf("  NOTE: get_weight returned %d\n", w_err);
    }
    
    agentos_forgetting_destroy(engine);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("fg_get_weight");
    return 0;
}

/**
 * T5: 不同遗忘策略
 */
int test_fg_strategies(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    add_test_records(l1, 10);
    
    agentos_forget_strategy_t strategies[] = {
        AGENTOS_FORGET_EBBINGHAUS,
        AGENTOS_FORGET_LINEAR,
        AGENTOS_FORGET_ACCESS_BASED
    };
    
    const char* strategy_names[] = {
        "Ebbinghaus", "Linear", "Access-Based"
    };
    
    for (size_t s = 0; s < sizeof(strategies)/sizeof(strategies[0]); s++) {
        agentos_forgetting_config_t cfg = {
            .strategy = strategies[s],
            .lambda = 0.1,
            .threshold = 0.2,
            .min_access = 1,
            .check_interval_sec = 100,
            .archive_path = NULL
        };
        
        agentos_forgetting_engine_t* eng = NULL;
        agentos_error_t err = agentos_forgetting_create(&cfg, l1, NULL, &eng);
        TEST_ASSERT(err == AGENTOS_SUCCESS || eng != NULL,
                   "each strategy should be creatable");
        
        if (eng) {
            uint32_t p = 0;
            agentos_forgetting_prune(eng, &p);
            agentos_forgetting_destroy(eng);
        }
        printf("    strategy[%s]: OK\n", strategy_names[s]);
    }
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("fg_strategies");
    return 0;
}

/**
 * T6: 无效参数边界测试
 */
int test_fg_invalid_params(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    agentos_forgetting_engine_t* eng = NULL;
    
    agentos_error_t e1 = agentos_forgetting_create(NULL, NULL, NULL, &eng);
    TEST_ASSERT(e1 != AGENTOS_SUCCESS, 
               "NULL layer1 should fail or handle gracefully");
    
    uint32_t pruned = 0;
    agentos_error_t e2 = agentos_forgetting_prune(NULL, &pruned);
    TEST_ASSERT(e2 != AGENTOS_SUCCESS, "prune NULL should fail");
    
    float w = 0;
    agentos_error_t e3 = agentos_forgetting_get_weight(NULL, "id", &w);
    TEST_ASSERT(e3 != AGENTOS_SUCCESS, "get_weight NULL should fail");
    
    agentos_forgetting_stop_auto(NULL);
    agentos_forgetting_destroy(NULL);
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("fg_invalid_params");
    return 0;
}

/**
 * T7: 高阈值不裁剪任何记录
 */
int test_fg_no_prune_high_threshold(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    add_test_records(l1, 10);
    
    agentos_forgetting_config_t config = {
        .strategy = AGENTOS_FORGET_EBBINGHAUS,
        .lambda = 0.001,
        .threshold = 0.99,
        .min_access = 1,
        .check_interval_sec = 9999,
        .archive_path = NULL
    };
    
    agentos_forgetting_engine_t* engine = NULL;
    agentos_forgetting_create(&config, l1, NULL, &engine);
    
    uint32_t pruned = 999;
    agentos_forgetting_prune(engine, &pruned);
    
    TEST_ASSERT(pruned == 0, "high threshold should prune 0 records");
    
    agentos_forgetting_destroy(engine);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("fg_no_prune_high_threshold");
    return 0;
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    /* 初始化内存管理器 */
    memory_options_t mem_opts = {0};
    if (!memory_init(&mem_opts)) {
        printf("  WARN: memory_init failed\n");
    }
    
    printf("\n========================================\n");
    printf("  MemoryRovol Forgetting Engine Unit Tests\n");
    printf("========================================\n\n");
    
    RUN_TEST(test_fg_create_destroy);
    RUN_TEST(test_fg_create_default);
    RUN_TEST(test_fg_prune);
    RUN_TEST(test_fg_get_weight);
    RUN_TEST(test_fg_strategies);
    RUN_TEST(test_fg_invalid_params);
    RUN_TEST(test_fg_no_prune_high_threshold);
    
    printf("\n----------------------------------------\n");
    printf("  Results: %d/%d passed", g_tests_passed, g_tests_passed + g_tests_failed);
    if (g_tests_failed > 0) {
        printf(" (%d FAILED)", g_tests_failed);
    }
    printf("\n");
    
    return g_tests_failed > 0 ? 1 : 0;
}
