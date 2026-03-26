/**
 * @file test_majority.c
 * @brief 多数投票协调器单元测试
 * @copyright (c) 2026 SPHARx. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 包含必要的头文件 */
#include "cognition.h"
#include "coordinator/strategy.h"
#include "agentos.h"

/**
 * @brief 测试多数投票协调器基本功能
 * @return 0表示成功，非0表示失败
 */
int test_majority_basic(void) {
    printf("  测试多数投票协调器基本功能...\n");
    
    agentos_coordinator_base_t* coordinator = NULL;
    agentos_error_t err = agentos_coordinator_majority_create(3, 0.5f, &coordinator);
    if (err != AGENTOS_SUCCESS) {
        printf("    创建协调器失败: %d\n", err);
        return 1;
    }
    
    /* 准备测试数据 */
    const char* inputs[] = {
        "option_a",
        "option_b",
        "option_a",
        "option_c",
        "option_a"
    };
    size_t input_count = 5;
    
    char* result = NULL;
    agentos_coordination_context_t context = {0};
    
    err = coordinator->coordinate(coordinator, &context, inputs, input_count, &result);
    if (err != AGENTOS_SUCCESS) {
        printf("    协调执行失败: %d\n", err);
        coordinator->destroy(coordinator);
        return 1;
    }
    
    if (result == NULL) {
        printf("    结果为空\n");
        coordinator->destroy(coordinator);
        return 1;
    }
    
    /* 验证结果：option_a应该获胜（3票） */
    if (strcmp(result, "option_a") != 0) {
        printf("    预期结果为 'option_a'，实际为 '%s'\n", result);
        free(result);
        coordinator->destroy(coordinator);
        return 1;
    }
    
    free(result);
    coordinator->destroy(coordinator);
    printf("    基本功能测试通过\n");
    return 0;
}

/**
 * @brief 测试边缘情况（特别是修复的除零问题）
 * @return 0表示成功，非0表示失败
 */
int test_majority_edge_cases(void) {
    printf("  测试边缘情况...\n");
    
    int failures = 0;
    
    /* 测试1：输入数量为0（修复的除零问题） */
    {
        agentos_coordinator_base_t* coordinator = NULL;
        agentos_error_t err = agentos_coordinator_majority_create(3, 0.5f, &coordinator);
        if (err != AGENTOS_SUCCESS) {
            printf("    测试1：创建协调器失败: %d\n", err);
            failures++;
        } else {
            const char** inputs = NULL;
            size_t input_count = 0;
            char* result = NULL;
            agentos_coordination_context_t context = {0};
            
            err = coordinator->coordinate(coordinator, &context, inputs, input_count, &result);
            if (err != AGENTOS_SUCCESS) {
                printf("    测试1：协调执行失败（预期成功）: %d\n", err);
                failures++;
            } else if (result == NULL) {
                printf("    测试1：结果为空（预期 'no_votes'）\n");
                failures++;
            } else if (strcmp(result, "no_votes") != 0) {
                printf("    测试1：预期结果为 'no_votes'，实际为 '%s'\n", result);
                free(result);
                failures++;
            } else {
                printf("    测试1：输入数量为0测试通过\n");
                free(result);
            }
            
            coordinator->destroy(coordinator);
        }
    }
    
    /* 测试2：输入数量不足 */
    {
        agentos_coordinator_base_t* coordinator = NULL;
        agentos_error_t err = agentos_coordinator_majority_create(5, 0.5f, &coordinator);
        if (err != AGENTOS_SUCCESS) {
            printf("    测试2：创建协调器失败: %d\n", err);
            failures++;
        } else {
            const char* inputs[] = {"option_a", "option_b"};
            size_t input_count = 2;
            char* result = NULL;
            agentos_coordination_context_t context = {0};
            
            err = coordinator->coordinate(coordinator, &context, inputs, input_count, &result);
            if (err != AGENTOS_SUCCESS) {
                printf("    测试2：协调执行失败（预期成功）: %d\n", err);
                failures++;
            } else if (result == NULL) {
                printf("    测试2：结果为空（预期 'insufficient_voters'）\n");
                failures++;
            } else if (strcmp(result, "insufficient_voters") != 0) {
                printf("    测试2：预期结果为 'insufficient_voters'，实际为 '%s'\n", result);
                free(result);
                failures++;
            } else {
                printf("    测试2：输入数量不足测试通过\n");
                free(result);
            }
            
            coordinator->destroy(coordinator);
        }
    }
    
    /* 测试3：阈值为0.0f（特殊情况） */
    {
        agentos_coordinator_base_t* coordinator = NULL;
        agentos_error_t err = agentos_coordinator_majority_create(3, 0.0f, &coordinator);
        if (err != AGENTOS_SUCCESS) {
            printf("    测试3：创建协调器失败（阈值为0）: %d\n", err);
            failures++;
        } else {
            const char* inputs[] = {"option_a", "option_b", "option_c"};
            size_t input_count = 3;
            char* result = NULL;
            agentos_coordination_context_t context = {0};
            
            err = coordinator->coordinate(coordinator, &context, inputs, input_count, &result);
            if (err != AGENTOS_SUCCESS) {
                printf("    测试3：协调执行失败: %d\n", err);
                failures++;
            } else if (result == NULL) {
                printf("    测试3：结果为空\n");
                failures++;
            } else {
                /* 阈值为0时，第一个选项应该获胜？这取决于实现，但至少不应该崩溃 */
                printf("    测试3：阈值为0测试通过（结果为 '%s'）\n", result);
                free(result);
            }
            
            coordinator->destroy(coordinator);
        }
    }
    
    /* 测试4：输入包含NULL指针 */
    {
        agentos_coordinator_base_t* coordinator = NULL;
        agentos_error_t err = agentos_coordinator_majority_create(3, 0.5f, &coordinator);
        if (err != AGENTOS_SUCCESS) {
            printf("    测试4：创建协调器失败: %d\n", err);
            failures++;
        } else {
            const char* inputs[] = {"option_a", NULL, "option_a"};
            size_t input_count = 3;
            char* result = NULL;
            agentos_coordination_context_t context = {0};
            
            err = coordinator->coordinate(coordinator, &context, inputs, input_count, &result);
            if (err != AGENTOS_SUCCESS) {
                printf("    测试4：协调执行失败（预期成功）: %d\n", err);
                failures++;
            } else if (result == NULL) {
                printf("    测试4：结果为空\n");
                failures++;
            } else if (strcmp(result, "option_a") != 0) {
                printf("    测试4：预期结果为 'option_a'，实际为 '%s'\n", result);
                free(result);
                failures++;
            } else {
                printf("    测试4：包含NULL输入测试通过\n");
                free(result);
            }
            
            coordinator->destroy(coordinator);
        }
    }
    
    if (failures == 0) {
        printf("    所有边缘情况测试通过\n");
    }
    
    return failures;
}