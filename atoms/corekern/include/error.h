/**
 * @file error.h
 * @brief 内核错误码定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ERROR_H
#define AGENTOS_ERROR_H

#include <stdint.h>
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AGENTOS_COREKERN_API_VERSION 1

typedef int32_t agentos_error_t;

#define AGENTOS_SUCCESS                0
#define AGENTOS_EINVAL                 -1
#define AGENTOS_ENOMEM                 -2
#define AGENTOS_EBUSY                  -3
#define AGENTOS_ENOENT                 -4
#define AGENTOS_EPERM                  -5
#define AGENTOS_ETIMEDOUT              -6
#define AGENTOS_EEXIST                 -7
#define AGENTOS_ECANCELED              -8
#define AGENTOS_ENOTSUP                -9
#define AGENTOS_EIO                    -10
#define AGENTOS_EINTR                  -11
#define AGENTOS_EOVERFLOW              -12
#define AGENTOS_EBADF                  -13
#define AGENTOS_ENOTINIT               -14
#define AGENTOS_ERESOURCE              -15

AGENTOS_API const char* agentos_strerror(agentos_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ERROR_H */
