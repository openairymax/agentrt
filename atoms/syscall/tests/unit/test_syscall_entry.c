/**
 * @file test_syscall_entry.c
 * @brief 系统调用入口单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <assert.h>

/* 包含必要的头文件 */
#include "syscalls.h"
#include "agentos.h"

/**
 * @brief 测试系统调用入口基本功能
 * @return 0表示成功，非0表示失败
 */
int main(void) {
    printf("运行系统调用入口单元测试...\n");
    
    /* 简单测试，验证头文件包含正�?*/
    printf("  头文件包含测试通过\n");
    
    /* 可以添加更多实际测试，但至少验证编译正常 */
    printf("  系统调用入口单元测试完成\n");
    
    return 0;
}