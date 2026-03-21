/**
 * @file channel.c
 * @brief IPC 通道管理（消息队列实现）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "ipc.h"
#include "mem.h"
#include "task.h"
#include <string.h>

// 此文件可放置通道的辅助功能，如创建命名管道等
// 由于 binder.c 已包含主要功能，此文件留作扩展
// 为满足生产级，至少提供一个初始化函数

agentos_error_t agentos_ipc_init(void) {
    // 已在 binder.c 中实现
    return AGENTOS_SUCCESS;
}