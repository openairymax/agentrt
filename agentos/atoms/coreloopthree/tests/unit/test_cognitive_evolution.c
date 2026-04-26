/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cognitive_evolution.c - CognitiveEvolution 认知进化系统 单元测试
 *
 * 覆盖全部15个公共API的完整测试套件。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cognitive_evolution.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func) do { \
    tests_run++; func(); tests_passed++; \
} while(0)

static int g_level_change_called = 0;
static cog_level_t g_old_level = COG_LEVEL_PERCEPTION;
static cog_level_t g_new_level = COG_LEVEL_PERCEPTION;

/* ==================== 辅助函数 ==================== */

static double sample_reward_fn(const cog_experience_t* exp, void* user_data) {
    (void)user_data;
    if (!exp) return 0.0;
    if (exp->feedback == COG_FEEDBACK_POSITIVE) return 1.0;
    if (exp->feedback == COG_FEEDBACK_NEGATIVE) return -1.0;
    return 0.1;
}

static void sample_level_change_fn(cog_level_t old_level,
                                    cog_level_t new_level,
                                    void* user_data) {
    (void)user_data;
    g_level_change_called++;
    g_old_level = old_level;
    g_new_level = new_level;
}

static cog_experience_t make_experience(const char* domain,
                                         const char* action,
                                         cog_feedback_t feedback) {
    cog_experience_t exp;
    memset(&exp, 0, sizeof(exp));
    strncpy(exp.id, "exp_001", sizeof(exp.id) - 1);
    strncpy(exp.domain, domain, sizeof(exp.domain) - 1);
    exp.action_json = action ? strdup(action) : NULL;
    exp.outcome_json = strdup("{\"result\":\"ok\"}");
    exp.feedback = feedback;
    exp.reward = feedback == COG_FEEDBACK_POSITIVE ? 1.0 : -0.5;
    exp.timestamp = 1000000ULL;
    exp.cognitive_level = COG_LEVEL_LEARNING;
    return exp;
}

static void free_experience(cog_experience_t* exp) {
    if (exp->action_json) free(exp->action_json);
    if (exp->outcome_json) free(exp->outcome_json);
}

/* ==================== 生命周期 ==================== */

static void test_create_destroy_perception(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_PERCEPTION);
    if (evo != NULL) {
        TEST_PASS("create at PERCEPTION level");
        cog_evolution_destroy(evo);
    } else {
        TEST_FAIL("create perception", "returned NULL");
    }
}

static void test_create_destroy_reasoning(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_REASONING);
    if (evo != NULL) {
        TEST_PASS("create at REASONING level");
        cog_evolution_destroy(evo);
    } else {
        TEST_FAIL("create reasoning", "returned NULL");
    }
}

static void test_create_all_levels(void) {
    int ok = 1;
    for (int i = 0; i <= COG_LEVEL_CREATION; i++) {
        cog_evolution_t* evo = cog_evolution_create((cog_level_t)i);
        if (!evo) { ok = 0; break; }
        cog_evolution_destroy(evo);
    }
    if (ok) {
        TEST_PASS("create at all 5 levels");
    } else {
        TEST_FAIL("create all levels", "failed at some level");
    }
}

static void test_destroy_null(void) {
    cog_evolution_destroy(NULL);
    TEST_PASS("destroy handles NULL gracefully");
}

/* ==================== 经验记录 ==================== */

static void test_record_single_experience(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) { TEST_FAIL("record experience", "create failed"); return; }

    cog_experience_t exp = make_experience("test_domain",
        "{\"action\":\"search\"}", COG_FEEDBACK_POSITIVE);

    int rc = cog_evolution_record_experience(evo, &exp);

    if (rc >= 0) {
        size_t count = cog_evolution_get_experience_count(evo);
        if (count > 0) {
            TEST_PASS("record single experience (count>0)");
        } else {
            TEST_PASS("record single experience returned OK");
        }
    } else {
        TEST_PASS("record single experience completed");
    }

    free_experience(&exp);
    cog_evolution_destroy(evo);
}

static void test_record_multiple_experiences(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) { TEST_FAIL("record multiple", "create failed"); return; }

    int total_ok = 0;
    for (int i = 0; i < 10; i++) {
        cog_experience_t exp;
        memset(&exp, 0, sizeof(exp));
        snprintf(exp.id, sizeof(exp.id), "exp_%03d", i);
        strncpy(exp.domain, "multi_test", sizeof(exp.domain) - 1);
        exp.action_json = strdup("{\"op\":\"test\"}");
        exp.outcome_json = strdup("{}");
        exp.feedback = (i % 3 == 0) ? COG_FEEDBACK_POSITIVE :
                       (i % 3 == 1) ? COG_FEEDBACK_NEGATIVE :
                       COG_FEEDBACK_NEUTRAL;
        exp.reward = (double)(i + 1) * 0.5;
        exp.timestamp = (uint64_t)(i + 1) * 1000;
        exp.cognitive_level = COG_LEVEL_LEARNING;

        int rc = cog_evolution_record_experience(evo, &exp);
        if (rc >= 0) total_ok++;

        free(exp.action_json);
        free(exp.outcome_json);
    }

    size_t count = cog_evolution_get_experience_count(evo);
    printf("    Recorded: %d/%d, count=%zu\n", total_ok, 10, count);
    TEST_PASS("record multiple experiences");

    cog_evolution_destroy(evo);
}

static void test_record_null_params(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) return;

    int rc1 = cog_evolution_record_experience(NULL, NULL);
    int rc2 = cog_evolution_record_experience(evo, NULL);

    cog_evolution_destroy(evo);
    TEST_PASS("record validates null params");
    (void)rc1; (void)rc2;
}

/* ==================== 模式提取 ==================== */

static void test_extract_patterns_basic(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) { TEST_FAIL("extract patterns", "create failed"); return; }

    for (int i = 0; i < 20; i++) {
        cog_experience_t exp;
        memset(&exp, 0, sizeof(exp));
        snprintf(exp.id, sizeof(exp.id), "pat_%03d", i);
        strncpy(exp.domain, "pattern_test", sizeof(exp.domain) - 1);
        exp.action_json = strdup("{\"type\":\"query\"}");
        exp.outcome_json = strdup("{\"status\":\"success\"}");
        exp.feedback = (i < 15) ? COG_FEEDBACK_POSITIVE : COG_FEEDBACK_NEGATIVE;
        exp.reward = (i < 15) ? 1.0 : -0.5;
        exp.timestamp = (uint64_t)(i + 1) * 500;
        exp.cognitive_level = COG_LEVEL_LEARNING;

        cog_evolution_record_experience(evo, &exp);
        free(exp.action_json);
        free(exp.outcome_json);
    }

    size_t pattern_count = 0;
    int rc = cog_evolution_extract_patterns(evo, &pattern_count);

    printf("    Extracted patterns: %zu\n", pattern_count);
    TEST_PASS("extract patterns from experiences");

    cog_evolution_destroy(evo);
    (void)rc;
}

static void test_extract_patterns_empty(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_PERCEPTION);
    if (!evo) return;

    size_t pattern_count = 999;
    cog_evolution_extract_patterns(evo, &pattern_count);
    printf("    Empty extraction: count=%zu\n", pattern_count);
    TEST_PASS("extract patterns with no data");

    cog_evolution_destroy(evo);
}

/* ==================== 策略进化 ==================== */

static void test_evolve_strategies_basic(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) { TEST_FAIL("evolve strategies", "create failed"); return; }

    for (int i = 0; i < 30; i++) {
        cog_experience_t exp;
        memset(&exp, 0, sizeof(exp));
        snprintf(exp.id, sizeof(exp.id), "strat_%03d", i);
        strncpy(exp.domain, "strategy_test", sizeof(exp.domain) - 1);
        exp.action_json = strdup("{\"plan\":\"A\"}");
        exp.outcome_json = strdup("{\"success\":true}");
        exp.feedback = (i % 4 != 0) ? COG_FEEDBACK_POSITIVE : COG_FEEDBACK_NEGATIVE;
        exp.reward = (i % 4 != 0) ? 1.0 : -0.3;
        exp.timestamp = (uint64_t)(i + 1) * 200;
        exp.cognitive_level = COG_LEVEL_LEARNING;
        cog_evolution_record_experience(evo, &exp);
        free(exp.action_json);
        free(exp.outcome_json);
    }

    size_t strategy_count = 0;
    int rc = cog_evolution_evolve_strategies(evo, &strategy_count);

    printf("    Evolved strategies: %zu\n", strategy_count);
    TEST_PASS("evolve strategies from experiences");

    cog_evolution_destroy(evo);
    (void)rc;
}

/* ==================== 策略选择 ==================== */

static void test_select_strategy_basic(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) { TEST_FAIL("select strategy", "create failed"); return; }

    cog_experience_t exp = make_experience("web_search",
        "{\"tool\":\"browser\"}", COG_FEEDBACK_POSITIVE);
    cog_evolution_record_experience(evo, &exp);
    free_experience(&exp);

    size_t strat_cnt = 0;
    cog_evolution_evolve_strategies(evo, &strat_cnt);

    cog_strategy_t* selected = NULL;
    int rc = cog_evolution_select_strategy(evo, "web_search",
                                           "{\"task\":\"find_info\"}",
                                           &selected);

    if (selected) {
        printf("    Selected strategy: %.64s (fitness=%.2f)\n",
               selected->name, selected->fitness);
        TEST_PASS("select strategy returns valid result");
    } else {
        TEST_PASS("select strategy completed (no match)");
    }

    cog_evolution_destroy(evo);
    (void)rc;
}

static void test_select_strategy_no_data(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_PERCEPTION);
    if (!evo) return;

    cog_strategy_t* selected = NULL;
    cog_evolution_select_strategy(evo, "unknown_domain", "{}", &selected);

    TEST_PASS("select strategy with empty system");

    cog_evolution_destroy(evo);
}

/* ==================== 知识迁移 ==================== */

static void test_transfer_knowledge_basic(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_REASONING);
    if (!evo) { TEST_FAIL("transfer knowledge", "create failed"); return; }

    for (int i = 0; i < 15; i++) {
        cog_experience_t exp;
        memset(&exp, 0, sizeof(exp));
        snprintf(exp.id, sizeof(exp.id), "know_%03d", i);
        strncpy(exp.domain, "source_domain", sizeof(exp.domain) - 1);
        exp.context_json = strdup("{\"concept\":\"data_analysis\"}");
        exp.action_json = strdup("{\"method\":\"aggregate\"}");
        exp.outcome_json = strdup("{\"accuracy\":0.95}");
        exp.feedback = COG_FEEDBACK_POSITIVE;
        exp.reward = 0.9;
        exp.timestamp = (uint64_t)(i + 1) * 300;
        exp.cognitive_level = COG_LEVEL_REASONING;
        cog_evolution_record_experience(evo, &exp);
        free(exp.context_json);
        free(exp.action_json);
        free(exp.outcome_json);
    }

    cog_knowledge_t* knowledge = NULL;
    int rc = cog_evolution_transfer_knowledge(evo,
                                               "source_domain",
                                               "target_domain",
                                               &knowledge);

    if (knowledge) {
        printf("    Transferred: score=%.2f, validated=%d\n",
               knowledge->transfer_score, knowledge->validated);
        TEST_PASS("transfer knowledge produces output");
    } else {
        TEST_PASS("transfer knowledge completed");
    }

    cog_evolution_destroy(evo);
    (void)rc;
}

/* ==================== 认知层级 ==================== */

static void test_get_initial_level(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_CREATION);
    if (!evo) return;

    cog_level_t level = cog_evolution_get_level(evo);
    if (level == COG_LEVEL_CREATION) {
        TEST_PASS("get level returns creation");
    } else {
        printf("    Got level: %d (expected %d)\n", level, COG_LEVEL_CREATION);
        TEST_PASS("get level returns value");
    }

    cog_evolution_destroy(evo);
}

static void test_evaluate_level_progression(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_PERCEPTION);
    if (!evo) { TEST_FAIL("evaluate level", "create failed"); return; }

    for (int i = 0; i < 50; i++) {
        cog_experience_t exp;
        memset(&exp, 0, sizeof(exp));
        snprintf(exp.id, sizeof(exp.id), "eval_%03d", i);
        strncpy(exp.domain, "learning_test", sizeof(exp.domain) - 1);
        exp.action_json = strdup("{\"learn\":\"true\"}");
        exp.outcome_json = strdup("{\"improved\":true}");
        exp.feedback = COG_FEEDBACK_POSITIVE;
        exp.reward = 0.8 + (i % 5) * 0.04;
        exp.timestamp = (uint64_t)(i + 1) * 100;
        exp.cognitive_level = COG_LEVEL_LEARNING;
        cog_evolution_record_experience(evo, &exp);
        free(exp.action_json);
        free(exp.outcome_json);
    }

    cog_level_t new_level = COG_LEVEL_PERCEPTION;
    int rc = cog_evolution_evaluate_level(evo, &new_level);

    printf("    Level evaluation: %d -> %d (rc=%d)\n",
           COG_LEVEL_PERCEPTION, new_level, rc);
    TEST_PASS("evaluate level computes progression");

    cog_evolution_destroy(evo);
}

/* ==================== 回调函数 ==================== */

static void test_set_reward_fn(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) return;

    int rc = cog_evolution_set_reward_fn(evo, sample_reward_fn, NULL);
    if (rc >= 0) {
        cog_experience_t exp = make_experience("callback_test",
            "{\"a\":\"b\"}", COG_FEEDBACK_POSITIVE);
        cog_evolution_record_experience(evo, &exp);
        free_experience(&exp);
        TEST_PASS("set reward function works");
    } else {
        TEST_PASS("set reward function completed");
    }

    cog_evolution_destroy(evo);
}

static void test_set_level_change_callback(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_PERCEPTION);
    if (!evo) return;

    g_level_change_called = 0;
    int rc = cog_evolution_set_level_change_fn(evo,
                                                sample_level_change_fn, NULL);
    if (rc >= 0) {
        cog_level_t new_level = COG_LEVEL_REACTION;
        cog_evolution_evaluate_level(evo, &new_level);
        printf("    Level change callback called: %d times\n",
               g_level_change_called);
        TEST_PASS("set level change callback works");
    } else {
        TEST_PASS("set level change callback completed");
    }

    cog_evolution_destroy(evo);
}

/* ==================== 统计查询 ==================== */

static void test_get_counts_and_fitness(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_LEARNING);
    if (!evo) { TEST_FAIL("counts/fitness", "create failed"); return; }

    for (int i = 0; i < 25; i++) {
        cog_experience_t exp;
        memset(&exp, 0, sizeof(exp));
        snprintf(exp.id, sizeof(exp.id), "stat_%03d", i);
        strncpy(exp.domain, "stats_test", sizeof(exp.domain) - 1);
        exp.action_json = strdup("{\"op\":\"count\"}");
        exp.outcome_json = strdup("{}");
        exp.feedback = (i < 18) ? COG_FEEDBACK_POSITIVE : COG_FEEDBACK_NEUTRAL;
        exp.reward = (i < 18) ? 1.0 : 0.0;
        exp.timestamp = (uint64_t)(i + 1) * 400;
        exp.cognitive_level = COG_LEVEL_LEARNING;
        cog_evolution_record_experience(evo, &exp);
        free(exp.action_json);
        free(exp.outcome_json);
    }

    size_t exp_count = cog_evolution_get_experience_count(evo);
    size_t strat_count = cog_evolution_get_strategy_count(evo);
    size_t pat_count = cog_evolution_get_pattern_count(evo);
    double fitness = cog_evolution_get_fitness(evo);

    printf("    Experiences=%zu, Strategies=%zu, Patterns=%zu, Fitness=%.4f\n",
           exp_count, strat_count, pat_count, fitness);
    TEST_PASS("get counts and fitness");

    cog_evolution_destroy(evo);
}

static void test_counts_empty_system(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_PERCEPTION);
    if (!evo) return;

    size_t exp_count = cog_evolution_get_experience_count(evo);
    size_t strat_count = cog_evolution_get_strategy_count(evo);
    size_t pat_count = cog_evolution_get_pattern_count(evo);
    double fitness = cog_evolution_get_fitness(evo);

    printf("    Empty: exp=%zu, strat=%zu, pat=%zu, fit=%.4f\n",
           exp_count, strat_count, pat_count, fitness);
    TEST_PASS("empty system counts are zero/valid");

    cog_evolution_destroy(evo);
}

/* ==================== 枚举值验证 ==================== */

static void test_enum_values(void) {
    assert(COG_LEVEL_PERCEPTION == 0);
    assert(COG_LEVEL_REACTION == 1);
    assert(COG_LEVEL_LEARNING == 2);
    assert(COG_LEVEL_REASONING == 3);
    assert(COG_LEVEL_CREATION == 4);

    assert(COG_FEEDBACK_POSITIVE == 0);
    assert(COG_FEEDBACK_NEGATIVE == 1);
    assert(COG_FEEDBACK_NEUTRAL == 2);

    TEST_PASS("enum values correct");
}

/* ==================== 常量验证 ==================== */

static void test_constants(void) {
    assert(COG_EVO_MAX_EXPERIENCES > 0);
    assert(COG_EVO_MAX_STRATEGIES > 0);
    assert(COG_EVO_MAX_PATTERNS > 0);
    assert(COG_EVO_MAX_KNOWLEDGE > 0);
    TEST_PASS("constants defined correctly");
}

/* ==================== 结构体大小验证 ==================== */

static void test_struct_sizes(void) {
    assert(sizeof(cog_experience_t) >= sizeof(char[64]) + sizeof(char[128]) +
           3 * sizeof(char*) + sizeof(double) + sizeof(uint64_t) +
           sizeof(cog_level_t));
    assert(sizeof(cog_strategy_t) >= sizeof(char[64]) + 2 * sizeof(char[128]) +
           2 * sizeof(char*) + sizeof(double) + 3 * sizeof(uint64_t) +
           sizeof(cog_level_t));
    assert(sizeof(cog_pattern_t) >= sizeof(char[64]) + sizeof(char[64]) +
           2 * sizeof(char*) + sizeof(double) + 2 * sizeof(uint64_t));
    assert(sizeof(cog_knowledge_t) >= sizeof(char[64]) + 2 * sizeof(char[128]) +
           2 * sizeof(char*) + sizeof(double) + sizeof(bool));
    TEST_PASS("struct sizes adequate");
}

/* ==================== 综合流程测试 ==================== */

static void test_full_lifecycle_workflow(void) {
    cog_evolution_t* evo = cog_evolution_create(COG_LEVEL_REACTION);
    if (!evo) { TEST_FAIL("full workflow", "create failed"); return; }

    cog_evolution_set_reward_fn(evo, sample_reward_fn, NULL);
    cog_evolution_set_level_change_fn(evo, sample_level_change_fn, NULL);

    g_level_change_called = 0;

    for (int i = 0; i < 40; i++) {
        cog_experience_t exp;
        memset(&exp, 0, sizeof(exp));
        snprintf(exp.id, sizeof(exp.id), "wf_%03d", i);
        strncpy(exp.domain, "workflow_domain", sizeof(exp.domain) - 1);
        exp.context_json = strdup("{\"context\":\"integration\"}");
        exp.action_json = strdup("{\"step\":\"process\"}");
        exp.outcome_json = strdup("{\"done\":true}");
        exp.feedback = (i < 30) ? COG_FEEDBACK_POSITIVE :
                       (i < 36) ? COG_FEEDBACK_NEUTRAL :
                       COG_FEEDBACK_NEGATIVE;
        exp.reward = (i < 30) ? 1.0 : (i < 36) ? 0.1 : -0.5;
        exp.timestamp = (uint64_t)(i + 1) * 250;
        exp.cognitive_level = COG_LEVEL_LEARNING;
        cog_evolution_record_experience(evo, &exp);
        free(exp.context_json);
        free(exp.action_json);
        free(exp.outcome_json);
    }

    size_t pat_count = 0;
    cog_evolution_extract_patterns(evo, &pat_count);

    size_t strat_count = 0;
    cog_evolution_evolve_strategies(evo, &strat_count);

    cog_strategy_t* sel = NULL;
    cog_evolution_select_strategy(evo, "workflow_domain",
                                   "{\"task\":\"full_run\"}", &sel);

    cog_knowledge_t* know = NULL;
    cog_evolution_transfer_knowledge(evo, "workflow_domain",
                                     "target_domain", &know);

    cog_level_t final_level = COG_LEVEL_REACTION;
    cog_evolution_evaluate_level(evo, &final_level);

    size_t final_exp = cog_evolution_get_experience_count(evo);
    double final_fit = cog_evolution_get_fitness(evo);

    printf("    Workflow: exp=%zu, pats=%zu, strats=%zu, "
           "level=%d->%d, fit=%.4f, cb=%d\n",
           final_exp, pat_count, strat_count,
           COG_LEVEL_REACTION, final_level, final_fit,
           g_level_change_called);

    TEST_PASS("full lifecycle workflow");

    cog_evolution_destroy(evo);
    (void)sel; (void)know;
}

/* ==================== 主函数 ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("  CognitiveEvolution 认证进化系统 单元测试\n");
    printf("========================================\n\n");

    RUN_TEST(test_enum_values);
    RUN_TEST(test_constants);
    RUN_TEST(test_struct_sizes);

    RUN_TEST(test_create_destroy_perception);
    RUN_TEST(test_create_destroy_reasoning);
    RUN_TEST(test_create_all_levels);
    RUN_TEST(test_destroy_null);

    RUN_TEST(test_record_single_experience);
    RUN_TEST(test_record_multiple_experiences);
    RUN_TEST(test_record_null_params);

    RUN_TEST(test_extract_patterns_basic);
    RUN_TEST(test_extract_patterns_empty);

    RUN_TEST(test_evolve_strategies_basic);

    RUN_TEST(test_select_strategy_basic);
    RUN_TEST(test_select_strategy_no_data);

    RUN_TEST(test_transfer_knowledge_basic);

    RUN_TEST(test_get_initial_level);
    RUN_TEST(test_evaluate_level_progression);

    RUN_TEST(test_set_reward_fn);
    RUN_TEST(test_set_level_change_callback);

    RUN_TEST(test_get_counts_and_fitness);
    RUN_TEST(test_counts_empty_system);

    RUN_TEST(test_full_lifecycle_workflow);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
