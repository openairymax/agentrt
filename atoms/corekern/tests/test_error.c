/**
 * @file test_error.c
 * @brief 错误处理单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/**
 * @brief 测试基本错误代码
 * @return 0表示成功，非0表示失败
 */
int test_error_basic(void) {
    printf("  测试基本错误代码...\n");
    
    /* 测试错误代码范围 */
    assert(AGENTOS_SUCCESS == 0);
    assert(AGENTOS_EINVAL > 0);
    assert(AGENTOS_ENOMEM > 0);
    assert(AGENTOS_EIO > 0);
    
    /* 测试错误代码唯一性 */
    assert(AGENTOS_SUCCESS != AGENTOS_EINVAL);
    assert(AGENTOS_EINVAL != AGENTOS_ENOMEM);
    assert(AGENTOS_ENOMEM != AGENTOS_EIO);
    
    return 0;
}

/**
 * @brief 测试错误字符串转换
 * @return 0表示成功，非0表示失败
 */
int test_error_strings(void) {
    printf("  测试错误字符串转换...\n");
    
    /* 测试已知错误代码的字符串表示 */
    const char* success_str = agentos_error_string(AGENTOS_SUCCESS);
    assert(success_str != NULL);
    assert(strstr(success_str, "成功") != NULL || strstr(success_str, "Success") != NULL);
    
    const char* einval_str = agentos_error_string(AGENTOS_EINVAL);
    assert(einval_str != NULL);
    assert(strlen(einval_str) > 0);
    
    const char* enomem_str = agentos_error_string(AGENTOS_ENOMEM);
    assert(enomem_str != NULL);
    assert(strlen(enomem_str) > 0);
    
    const char* eio_str = agentos_error_string(AGENTOS_EIO);
    assert(eio_str != NULL);
    assert(strlen(eio_str) > 0);
    
    /* 测试未知错误代码 */
    const char* unknown_str = agentos_error_string(9999);
    assert(unknown_str != NULL);
    assert(strstr(unknown_str, "未知") != NULL || strstr(unknown_str, "Unknown") != NULL);
    
    return 0;
}