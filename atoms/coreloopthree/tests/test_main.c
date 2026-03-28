/**
 * @file test_main.c
 * @brief coreloopthree 测试主程�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h"
#include "../../../bases/utils/string/include/string_compat.h"
#include <assert.h>

/* 测试函数声明 */
extern int test_majority_basic(void);
extern int test_majority_edge_cases(void);
extern int test_coordinator_basic(void);

/**
 * @brief 运行所有测�?
 * @return 0表示成功，非0表示失败
 */
int main(void) {
    printf("开始运�?coreloopthree 单元测试...\n");
    
    int failures = 0;
    
    /* 运行多数投票协调器测�?*/
    if (test_majority_basic() != 0) {
        printf("FAIL: test_majority_basic\n");
        failures++;
    } else {
        printf("PASS: test_majority_basic\n");
    }
    
    if (test_majority_edge_cases() != 0) {
        printf("FAIL: test_majority_edge_cases\n");
        failures++;
    } else {
        printf("PASS: test_majority_edge_cases\n");
    }
    
    /* 运行协调器基础测试 */
    if (test_coordinator_basic() != 0) {
        printf("FAIL: test_coordinator_basic\n");
        failures++;
    } else {
        printf("PASS: test_coordinator_basic\n");
    }
    
    /* 汇总结�?*/
    if (failures == 0) {
        printf("\n所有测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个测试失败\n", failures);
        return 1;
    }
}