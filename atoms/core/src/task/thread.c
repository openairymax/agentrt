/**
 * @file thread.c
 * @brief 线程管理（包装调度器接口）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "task.h"

// 此文件可作为任务创建、退出等函数的别名，实际实现在 scheduler.c 中
// 为保持模块独立，这里放置额外的线程相关函数（如设置优先级等）

agentos_error_t agentos_thread_set_priority(agentos_task_id_t tid, int priority) {
    if (priority < AGENTOS_TASK_PRIORITY_MIN || priority > AGENTOS_TASK_PRIORITY_MAX)
        return AGENTOS_EINVAL;
    // 查找 TCB 并修改优先级
    // 暂未实现
    return AGENTOS_ENOTSUP;
}