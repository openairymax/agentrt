/**
 * @file test_cost_tracker.c
 * @brief 成本跟踪器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cost_tracker.h"
#include <cjson/cJSON.h>

/**
 * @brief 测试成本跟踪器创建和销毁
 */
static void test_cost_tracker_create_destroy(void) {
    printf("  test_cost_tracker_create_destroy...\n");

    cost_tracker_t* tracker = cost_tracker_create(NULL, 0);
    assert(tracker != NULL);

    cost_tracker_destroy(tracker);

    printf("    PASSED\n");
}

/**
 * @brief 测试成本跟踪器带规则创建
 */
static void test_cost_tracker_with_rules(void) {
    printf("  test_cost_tracker_with_rules...\n");

    pricing_rule_t rules[2];
    memset(rules, 0, sizeof(rules));
    
    rules[0].model_pattern = strdup("gpt-4*");
    rules[0].input_price_per_k = 0.03;
    rules[0].output_price_per_k = 0.06;
    
    rules[1].model_pattern = strdup("gpt-3.5-turbo");
    rules[1].input_price_per_k = 0.001;
    rules[1].output_price_per_k = 0.002;

    cost_tracker_t* tracker = cost_tracker_create(rules, 2);
    assert(tracker != NULL);

    cost_tracker_destroy(tracker);
    
    free((void*)rules[0].model_pattern);
    free((void*)rules[1].model_pattern);

    printf("    PASSED\n");
}

/**
 * @brief 测试成本跟踪器添加记录
 */
static void test_cost_tracker_add(void) {
    printf("  test_cost_tracker_add...\n");

    cost_tracker_t* tracker = cost_tracker_create(NULL, 0);
    assert(tracker != NULL);

    cost_tracker_add(tracker, "gpt-4", 1000, 500);
    cost_tracker_add(tracker, "gpt-4", 2000, 1000);
    cost_tracker_add(tracker, "gpt-3.5-turbo", 500, 250);

    cost_tracker_destroy(tracker);

    printf("    PASSED\n");
}

/**
 * @brief 测试成本跟踪器导出JSON
 */
static void test_cost_tracker_export(void) {
    printf("  test_cost_tracker_export...\n");

    pricing_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.model_pattern = strdup("gpt-4");
    rule.input_price_per_k = 0.03;
    rule.output_price_per_k = 0.06;

    cost_tracker_t* tracker = cost_tracker_create(&rule, 1);
    assert(tracker != NULL);

    cost_tracker_add(tracker, "gpt-4", 1000, 500);

    cJSON* json = cost_tracker_export(tracker);
    assert(json != NULL);

    cJSON* models = cJSON_GetObjectItem(json, "models");
    assert(models != NULL);
    assert(cJSON_IsArray(models));

    int count = cJSON_GetArraySize(models);
    assert(count == 1);

    cJSON* model_obj = cJSON_GetArrayItem(models, 0);
    cJSON* model_name = cJSON_GetObjectItem(model_obj, "model");
    assert(model_name != NULL);
    assert(strcmp(model_name->valuestring, "gpt-4") == 0);

    cJSON_Delete(json);
    cost_tracker_destroy(tracker);
    free((void*)rule.model_pattern);

    printf("    PASSED\n");
}

/**
 * @brief 测试成本跟踪器空参数
 */
static void test_cost_tracker_null_param(void) {
    printf("  test_cost_tracker_null_param...\n");

    cost_tracker_add(NULL, "gpt-4", 1000, 500);

    cost_tracker_t* tracker = cost_tracker_create(NULL, 0);
    cost_tracker_add(tracker, NULL, 1000, 500);

    cJSON* json = cost_tracker_export(NULL);
    assert(json != NULL);
    cJSON_Delete(json);

    cost_tracker_destroy(tracker);
    cost_tracker_destroy(NULL);

    printf("    PASSED\n");
}

/**
 * @brief 测试成本跟踪器多模型
 */
static void test_cost_tracker_multiple_models(void) {
    printf("  test_cost_tracker_multiple_models...\n");

    cost_tracker_t* tracker = cost_tracker_create(NULL, 0);
    assert(tracker != NULL);

    cost_tracker_add(tracker, "gpt-4", 1000, 500);
    cost_tracker_add(tracker, "gpt-3.5-turbo", 2000, 1000);
    cost_tracker_add(tracker, "claude-3", 1500, 750);

    cJSON* json = cost_tracker_export(tracker);
    assert(json != NULL);

    cJSON* models = cJSON_GetObjectItem(json, "models");
    int count = cJSON_GetArraySize(models);
    assert(count == 3);

    cJSON_Delete(json);
    cost_tracker_destroy(tracker);

    printf("    PASSED\n");
}

/**
 * @brief 测试成本跟踪器规则匹配
 */
static void test_cost_tracker_rule_matching(void) {
    printf("  test_cost_tracker_rule_matching...\n");

    pricing_rule_t rules[2];
    memset(rules, 0, sizeof(rules));
    
    rules[0].model_pattern = strdup("gpt-4*");
    rules[0].input_price_per_k = 0.03;
    rules[0].output_price_per_k = 0.06;
    
    rules[1].model_pattern = strdup("gpt-3.5*");
    rules[1].input_price_per_k = 0.001;
    rules[1].output_price_per_k = 0.002;

    cost_tracker_t* tracker = cost_tracker_create(rules, 2);
    assert(tracker != NULL);

    cost_tracker_add(tracker, "gpt-4-turbo", 1000, 500);
    cost_tracker_add(tracker, "gpt-3.5-turbo-16k", 1000, 500);

    cJSON* json = cost_tracker_export(tracker);
    assert(json != NULL);

    cJSON* models = cJSON_GetObjectItem(json, "models");
    int count = cJSON_GetArraySize(models);
    assert(count == 2);

    cJSON_Delete(json);
    cost_tracker_destroy(tracker);
    
    free((void*)rules[0].model_pattern);
    free((void*)rules[1].model_pattern);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Cost Tracker Unit Tests\n");
    printf("=========================================\n");

    test_cost_tracker_create_destroy();
    test_cost_tracker_with_rules();
    test_cost_tracker_add();
    test_cost_tracker_export();
    test_cost_tracker_null_param();
    test_cost_tracker_multiple_models();
    test_cost_tracker_rule_matching();

    printf("\n✅ All cost tracker tests PASSED\n");
    return 0;
}
