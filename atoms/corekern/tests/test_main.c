/**
 * @file test_main.c
 * @brief corekern 测试主程序
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* 测试函数声明 */
extern int test_error_basic(void);
extern int test_error_strings(void);

/**
 * @brief 运行所有测试
 * @return 0表示成功，非0表示失败
 */
int main(void) {
    printf("开始运行 corekern 单元测试...\n");
    
    int failures = 0;
    
    /* 运行错误处理测试 */
    if (test_error_basic() != 0) {
        printf("FAIL: test_error_basic\n");
        failures++;
    } else {
        printf("PASS: test_error_basic\n");
    }
    
    if (test_error_strings() != 0) {
        printf("FAIL: test_error_strings\n");
        failures++;
    } else {
        printf("PASS: test_error_strings\n");
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