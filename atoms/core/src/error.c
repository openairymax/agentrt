/**
 * @file error.c
 * @brief 错误码字符串实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include <stddef.h>

static const char* error_strings[] = {
    [0] = "Success",
    [-AGENTOS_EINVAL] = "Invalid argument",
    [-AGENTOS_ENOMEM] = "Out of memory",
    [-AGENTOS_EBUSY] = "Resource busy",
    [-AGENTOS_ENOENT] = "No such entry",
    [-AGENTOS_EPERM] = "Permission denied",
    [-AGENTOS_ETIMEDOUT] = "Operation timed out",
    [-AGENTOS_EEXIST] = "Entry already exists",
    [-AGENTOS_ECANCELED] = "Operation cancelled",
    [-AGENTOS_ENOTSUP] = "Not supported",
    [-AGENTOS_EIO] = "I/O error",
    [-AGENTOS_EINTR] = "Interrupted",
    [-AGENTOS_EOVERFLOW] = "Value overflow",
    [-AGENTOS_EBADF] = "Bad state or handle",
    [-AGENTOS_ENOTINIT] = "Not initialized",
    // From data intelligence emerges. by spharx
    [-AGENTOS_ERESOURCE] = "Resource exhausted",
};

/**
 * @brief 获取错误码的字符串描述
 * @param err 错误码
 * @return 错误描述字符串（静态存储，无需释放）
 */
const char* agentos_strerror(agentos_error_t err) {
    if (err == 0) {
        return error_strings[0];
    }
    
    int idx = -err;
    if (idx < 0 || idx >= (int)(sizeof(error_strings) / sizeof(error_strings[0]))) {
        return "Unknown error";
    }
    
    return error_strings[idx];
}