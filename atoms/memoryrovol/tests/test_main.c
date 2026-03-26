/**
 * @file test_main.c
 * @brief memoryrovol 测试主程序
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* 测试函数声明 */
extern int test_advanced_storage_basic(void);
extern int test_advanced_storage_edge_cases(void);

/**
 * @brief 运行所有测试
 * @return 0表示成功，非0表示失败
 */
int main(void) {
    printf("开始运行 memoryrovol 单元测试...\n");
    
    int failures = 0;
    
    /* 运行高级存储测试 */
    if (test_advanced_storage_basic() != 0) {
        printf("FAIL: test_advanced_storage_basic\n");
        failures++;
    } else {
        printf("PASS: test_advanced_storage_basic\n");
    }
    
    if (test_advanced_storage_edge_cases() != 0) {
        printf("FAIL: test_advanced_storage_edge_cases\n");
        failures++;
    } else {
        printf("PASS: test_advanced_storage_edge_cases\n");
    }
    
    /* 汇总结果 */
    if (failures == 0) {
        printf("\n所有测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个测试失败\n", failures);
        return 1;
    }
}