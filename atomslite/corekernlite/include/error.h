/**
 * @file error.h
 * @brief 错误码定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LITE_ERROR_H
#define AGENTOS_LITE_ERROR_H

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AgentOS Lite 错误码
 */
typedef enum {
    AGENTOS_LITE_SUCCESS = 0,      /**< 成功 */
    AGENTOS_LITE_EINVAL = -1,      /**< 参数无效 */
    AGENTOS_LITE_ENOMEM = -2,      /**< 内存不足 */
    AGENTOS_LITE_EBUSY = -3,       /**< 资源忙 */
    AGENTOS_LITE_ENOENT = -4,      /**< 实体不存在 */
    AGENTOS_LITE_EPERM = -5,       /**< 操作不允许 */
    AGENTOS_LITE_ETIMEDOUT = -6,   /**< 操作超时 */
    AGENTOS_LITE_EEXIST = -7,      /**< 实体已存在 */
    AGENTOS_LITE_ECANCELED = -8,   /**< 操作取消 */
    AGENTOS_LITE_ENOTSUP = -9,     /**< 不支持 */
    AGENTOS_LITE_EIO = -10,        /**< I/O错误 */
} agentos_lite_error_t;

/**
 * @brief 获取错误码对应的描述字符串
 * @param err 错误码
 * @return 错误描述字符串
 */
AGENTOS_LITE_API const char* agentos_lite_strerror(agentos_lite_error_t err);

/**
 * @brief 检查错误码是否表示成功
 * @param err 错误码
 * @return 1表示成功，0表示失败
 */
AGENTOS_LITE_API int agentos_lite_is_success(agentos_lite_error_t err);

/**
 * @brief 检查错误码是否表示失败
 * @param err 错误码
 * @return 1表示失败，0表示成功
 */
AGENTOS_LITE_API int agentos_lite_is_error(agentos_lite_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_ERROR_H */