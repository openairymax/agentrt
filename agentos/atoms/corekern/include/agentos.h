/**
 * @file agentos.h
 * @brief AgentOS 微内核统一入口头文件
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 这是 AgentOS 微内核的统一入口点，提供了核心初始化的统一接口
 */

#ifndef AGENTOS_AGENTOS_H
#define AGENTOS_AGENTOS_H

/**
 * @brief API 版本声明 (MAJOR.MINOR.PATCH)
 *
 * 在相同 MAJOR 版本内保证 ABI 兼容
 * 破坏性更改需递增 MAJOR 并发布迁移说明
 */
#define AGENTOS_CORE_API_VERSION_MAJOR 1
#define AGENTOS_CORE_API_VERSION_MINOR 0
#define AGENTOS_CORE_API_VERSION_PATCH 0

/**
 * @brief ABI 兼容性说明
 *
 * 在相同 MAJOR 版本内保证 ABI 兼容
 * 破坏性更改需递增 MAJOR 并发布迁移说明
 */

/**
 * @brief 导出宏定义
 */
#include "export.h"
/**
 * @brief 错误码定义
 */
#include "error.h"
/**
 * @brief 内存管理接口
 */
#include "mem.h"
/**
 * @brief 任务调度接口
 */
#include "task.h"
/**
 * @brief 进程间通信接口
 */
#include "ipc.h"
/**
 * @brief 时间管理接口
 */
#include "agentos_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 AgentOS 核心
 *
 * @return agentos_error_t 错误码
 *
 * @ownership 内部管理所有核心资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @note 必须在使用其他 API 前调用此函数
 * @note 多次调用会返回 AGENTOS_SUCCESS（幂等操作）
 *
 * @see agentos_core_shutdown()
 */
AGENTOS_API int agentos_core_init(void);

/**
 * @brief 关闭 AgentOS 核心并清理资源
 *
 * @ownership 内部释放所有核心资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @note 调用后所有核心 API 将不可用
 * @note 应在程序退出前调用
 *
 * @see agentos_core_init()
 */
AGENTOS_API void agentos_core_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AGENTOS_H */
