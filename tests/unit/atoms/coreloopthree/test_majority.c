/**
 * @file test_majority.c
 * @brief 多数投票协调器单元测试
 * @copyright (c) 2026 SPHARx. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <assert.h>

/* 包含必要的头文件 */
#include "cognition.h"
#include "coordinator/strategy.h"
#include "agentrt.h"

/**
 * @brief 测试多数投票协调器基本功能
 * @return 0 表示成功，非 0 表示失败
 */
int test_majority_basic(void) {
    printf("  测试多数投票协调器基本功能...\n");

    agentrt_coordinator_base_t* coordinator = NULL;
    agentrt_error_t err = agentrt_coordinator_majority_create(3, 0.5f, &coordinator);
    if (err != AGENTRT_SUCCESS) {
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
    agentrt_coordination_context_t context = {0};

    err = coordinator->coordinate(coordinator, &context, inputs, input_count, &result);
    if (err != AGENTRT_SUCCESS) {
        printf("    协调执行失败：%d\n", err);
