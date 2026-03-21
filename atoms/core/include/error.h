/**
 * @file error.h
 * @brief 内核错误码定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_ERROR_H
#define AGENTOS_ERROR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t agentos_error_t;

/** 成功 */
#define AGENTOS_SUCCESS                0
/** 无效参数 */
#define AGENTOS_EINVAL                 -1
/** 内存不足 */
#define AGENTOS_ENOMEM                  -2
/** 资源忙 */
#define AGENTOS_EBUSY                   -3
/** 资源不存在 */
#define AGENTOS_ENOENT                   -4
/** 权限不足 */
#define AGENTOS_EPERM                    -5
/** 超时 */
#define AGENTOS_ETIMEDOUT                 -6
/** 已存在 */
#define AGENTOS_EEXIST                    -7
/** 操作取消 */
#define AGENTOS_ECANCELED                 -8
/** 不支持 */
#define AGENTOS_ENOTSUP                   -9
/** IO错误 */
#define AGENTOS_EIO                       -10
/** 中断 */
#define AGENTOS_EINTR                     -11
/** 溢出 */
#define AGENTOS_EOVERFLOW                 -12
/** 无效状态 */
#define AGENTOS_EBADF                     -13
/** 未初始化 */
#define AGENTOS_ENOTINIT                  -14
/** 资源不足（如Token预算） */
#define AGENTOS_ERESOURCE                 -15

/**
 * @brief 获取错误码的字符串描述
 * @param err 错误码
 * @return 错误描述字符串（静态存储，无需释放）
 */
const char* agentos_strerror(agentos_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ERROR_H */