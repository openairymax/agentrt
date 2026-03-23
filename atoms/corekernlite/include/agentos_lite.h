/**
 * @file agentos_lite.h
 * @brief AgentOS Lite 微内核统一入口头文件
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * AgentOS Lite 是 AgentOS 微内核的轻量级版本，提供：
 * - 错误处理
 * - 内存管理
 * - 任务调度
 * - 时间管理
 * 
 * 设计原则：
 * - 简洁：移除复杂特性，保留核心功能
 * - 易用：提供简洁的API接口
 * - 可移植：跨平台支持（Windows/Linux/macOS）
 * - 高效：最小化开销
 */

#ifndef AGENTOS_LITE_H
#define AGENTOS_LITE_H

#include "export.h"
#include "error.h"
#include "mem.h"
#include "task.h"
#include "time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 AgentOS Lite 内核
 * @return 成功返回AGENTOS_LITE_SUCCESS，失败返回错误码
 * 
 * 必须在使用任何其他API之前调用此函数。
 * 此函数会初始化所有子系统（内存、任务、时间）。
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_init(void);

/**
 * @brief 清理 AgentOS Lite 内核
 * 
 * 在程序结束前调用，清理所有子系统资源。
 * 调用后不应再使用任何API。
 */
AGENTOS_LITE_API void agentos_lite_cleanup(void);

/**
 * @brief 获取内核版本号
 * @return 版本号字符串（格式：major.minor.patch）
 */
AGENTOS_LITE_API const char* agentos_lite_version(void);

/**
 * @brief 获取内核API版本
 * @return API版本号
 */
AGENTOS_LITE_API int agentos_lite_api_version(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_H */
