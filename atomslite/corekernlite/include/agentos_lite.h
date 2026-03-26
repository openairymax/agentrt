/**
 * @file agentos_lite.h
 * @brief AgentOS Lite 统一入口头文件
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
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
 * @brief 初始化AgentOS Lite内核
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_init(void);

/**
 * @brief 清理AgentOS Lite内核
 */
AGENTOS_LITE_API void agentos_lite_cleanup(void);

/**
 * @brief 获取AgentOS Lite版本信息
 * @return 版本字符串
 */
AGENTOS_LITE_API const char* agentos_lite_version(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_H */