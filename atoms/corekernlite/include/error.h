/**
 * @file error.h
 * @brief AgentOS Lite 内核错误码定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 定义统一的错误码体系，遵循以下原则：
 * - 0 表示成功
 * - 负值表示通用错误
 * - 正值保留给子系统特定错误
 */

#ifndef AGENTOS_LITE_ERROR_H
#define AGENTOS_LITE_ERROR_H

#include <stdint.h>
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AGENTOS_LITE_API_VERSION 1

typedef int32_t agentos_lite_error_t;

#define AGENTOS_LITE_SUCCESS                0
#define AGENTOS_LITE_EINVAL                 -1
#define AGENTOS_LITE_ENOMEM                 -2
#define AGENTOS_LITE_EBUSY                  -3
#define AGENTOS_LITE_ENOENT                 -4
#define AGENTOS_LITE_EPERM                  -5
#define AGENTOS_LITE_ETIMEDOUT              -6
#define AGENTOS_LITE_EEXIST                 -7
#define AGENTOS_LITE_ECANCELED              -8
#define AGENTOS_LITE_ENOTSUP                -9
#define AGENTOS_LITE_EIO                    -10
#define AGENTOS_LITE_EINTR                  -11
#define AGENTOS_LITE_EOVERFLOW              -12
#define AGENTOS_LITE_EBADF                  -13
#define AGENTOS_LITE_ENOTINIT               -14
#define AGENTOS_LITE_ERESOURCE              -15

/**
 * @brief 获取错误码对应的错误描述字符串
 * @param err 错误码
 * @return 错误描述字符串（只读）
 */
AGENTOS_LITE_API const char* agentos_lite_strerror(agentos_lite_error_t err);

/**
 * @brief 检查错误码是否为成功
 * @param err 错误码
 * @return 1表示成功，0表示失败
 */
static inline int agentos_lite_is_success(agentos_lite_error_t err) {
    return err == AGENTOS_LITE_SUCCESS;
}

/**
 * @brief 检查错误码是否为错误
 * @param err 错误码
 * @return 1表示错误，0表示成功
 */
static inline int agentos_lite_is_error(agentos_lite_error_t err) {
    return err != AGENTOS_LITE_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_ERROR_H */
