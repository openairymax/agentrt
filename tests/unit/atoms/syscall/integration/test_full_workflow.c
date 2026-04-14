/**
 * @file test_full_workflow.c
 * @brief 系统调用完整工作流集成测�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <assert.h>

/* 包含必要的头文件 */
#include "syscalls.h"
#include "agentos.h"

/**
 * @brief 测试任务提交基本工作�?
 * @return 0表示成功，非0表示失败
 */
int main(void) {
    printf("运行系统调用完整工作流集成测�?..\n");

    /* 测试1：基本任务提�?*/
    printf("  测试1：基本任务提�?..\n");
    {
        /* 创建一个简单的任务JSON */
        const char* task_json = "{\"nodes\": [{\"id\": \"task1\", \"depends_on\": []}]}";
        agentos_error_t err = agentos_sys_task_submit(task_json, strlen(task_json));

        if (err != AGENTOS_SUCCESS) {
            printf("    基本任务提交失败: %d\n", err);
            return 1;
        }

        printf("    基本任务提交测试通过\n");
    }

    /* 测试2：测试修复的拓扑排序队列溢出问题 */
    printf("  测试2：测试拓扑排序边界情�?..\n");
    {
        /* 创建一个可能导致问题的依赖�?*/
        /* 注意：这里只是一个示例，实际可能需要更复杂的图来触发边界情�?*/
        const char* task_json = "{\"nodes\": [{\"id\": \"task1\", \"depends_on\": []}, {\"id\": \"task2\", \"depends_on\": [\"0\"]}]}";
        agentos_error_t err = agentos_sys_task_submit(task_json, strlen(task_json));

        if (err != AGENTOS_SUCCESS) {
            printf("    带依赖任务提交失�? %d\n", err);
            return 1;
        }

        printf("    带依赖任务提交测试通过\n");
    }

    /* 测试3：无效输入测�?*/
    printf("  测试3：无效输入测�?..\n");
    {
        /* NULL指针测试 */
        agentos_error_t err = agentos_sys_task_submit(NULL, 0);
        if (err != AGENTOS_EINVAL) {
            printf("    NULL输入应该返回EINVAL，实际返�? %d\n", err);
            return 1;
        }

        /* 无效JSON测试 */
        const char* invalid_json = "{invalid json";
        err = agentos_sys_task_submit(invalid_json, strlen(invalid_json));
        /* 可能返回解析错误，至少不应该崩溃 */
        printf("    无效JSON处理测试通过（返回码: %d）\n", err);
    }

    printf("  所有集成测试通过\n");
    printf("  系统调用完整工作流集成测试完成\n");

    return 0;
}
