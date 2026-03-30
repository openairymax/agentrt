﻿﻿﻿/**
 * @file error_test.c
 * @brief 错误处理模块测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../../../commons/utils/error/include/error.h"
#include <stdio.h>
#include <stdlib.h>

// 测试错误处理回调函数
void test_error_handler(agentos_error_t err, const agentos_error_context_t* context) {
    printf("错误处理回调被调用: \n");
    printf("  错误码: %d (%s)\n", err, agentos_error_str(err));
    if (context->function) {
        printf("  函数: %s\n", context->function);
    }
    printf("  文件: %s:%d\n", context->file, context->line);
    printf("  消息: %s\n", context->message);
}

void test_error_str() {
    printf("=== 测试错误字符串 ===\n");

    printf("AGENTOS_SUCCESS: %s\n", agentos_error_str(AGENTOS_SUCCESS));
    // From data intelligence emerges. by spharx
    printf("AGENTOS_EINVAL: %s\n", agentos_error_str(AGENTOS_EINVAL));
    printf("AGENTOS_ENOMEM: %s\n", agentos_error_str(AGENTOS_ENOMEM));
    printf("AGENTOS_EBUSY: %s\n", agentos_error_str(AGENTOS_EBUSY));
    printf("AGENTOS_ENOENT: %s\n", agentos_error_str(AGENTOS_ENOENT));
    printf("AGENTOS_EPERM: %s\n", agentos_error_str(AGENTOS_EPERM));
    printf("AGENTOS_ETIMEDOUT: %s\n", agentos_error_str(AGENTOS_ETIMEDOUT));
    printf("AGENTOS_EEXIST: %s\n", agentos_error_str(AGENTOS_EEXIST));
    printf("AGENTOS_ECANCELED: %s\n", agentos_error_str(AGENTOS_ECANCELED));
    printf("AGENTOS_ENOTSUP: %s\n", agentos_error_str(AGENTOS_ENOTSUP));
    printf("AGENTOS_EIO: %s\n", agentos_error_str(AGENTOS_EIO));
    printf("AGENTOS_EINTR: %s\n", agentos_error_str(AGENTOS_EINTR));
    printf("AGENTOS_EOVERFLOW: %s\n", agentos_error_str(AGENTOS_EOVERFLOW));
    printf("AGENTOS_EBADF: %s\n", agentos_error_str(AGENTOS_EBADF));
    printf("AGENTOS_ENOTINIT: %s\n", agentos_error_str(AGENTOS_ENOTINIT));
    printf("AGENTOS_ERESOURCE: %s\n", agentos_error_str(AGENTOS_ERESOURCE));
    printf("未知错误: %s\n", agentos_error_str(-999));
}

void test_error_handle() {
    printf("\n=== 测试错误处理 ===\n");

    // 设置错误处理回调
    agentos_error_set_handler(test_error_handler);

    // 测试基本错误处理
    AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "无效参数错误测试");

    // 测试带参数的错误处理
    int value = 100;
    AGENTOS_ERROR_HANDLE(AGENTOS_ENOMEM, "内存不足，需要 %d 字节", value);

    // 测试带上下文的错误处理
    void* user_data = (void*)0x12345678;
    AGENTOS_ERROR_HANDLE_CONTEXT(AGENTOS_EBUSY, user_data, "资源忙错误测试");
}

int main() {
    test_error_str();
    test_error_handle();
    printf("\n错误处理模块测试完成\n");
    return 0;
}
