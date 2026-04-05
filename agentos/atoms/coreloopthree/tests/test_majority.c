/**
 * @file test_majority.c
 * @brief 多数投票协调器单元测试
 * @copyright (c) 2026 SPHARx. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <assert.h>

/* 包含必要的头文件 */
#include "cognition.h"
#include "coordinator/strategy.h"
#include "agentos.h"

/**
 * @brief 测试多数投票协调器基本功能
 * @return 0 表示成功，非 0 表示失败
 */
int test_majority_basic(void) {
    printf("  测试多数投票协调器基本功能...\n");

    agentos_coordinator_base_t* coordinator = NULL;
    agentos_error_t err = agentos_coordinator_majority_create(3, 0.5f, &coordinator);
    if (err != AGENTOS_SUCCESS) {
        printf("    创建协调器失败：%d\n", err);
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
        printf("    协调执行失败：%d\n", err);
