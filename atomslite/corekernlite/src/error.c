/**
 * @file error.c
 * @brief 错误处理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos_lite.h"
#include <string.h>

static const char* error_strings[] = {
    "Success",
    "Invalid argument",
    "Out of memory",
    "Resource busy",
    "No such entity",
    "Operation not permitted",
    "Operation timed out",
    "Entity already exists",
    "Operation canceled",
    "Operation not supported",
    "I/O error",
};

AGENTOS_LITE_API const char* agentos_lite_strerror(agentos_lite_error_t err) {
    int index = -err;
    if (index >= 0 && index < (int)(sizeof(error_strings) / sizeof(error_strings[0]))) {
        return error_strings[index];
    }
    return "Unknown error";
}

AGENTOS_LITE_API int agentos_lite_is_success(agentos_lite_error_t err) {
    return err == AGENTOS_LITE_SUCCESS;
}

AGENTOS_LITE_API int agentos_lite_is_error(agentos_lite_error_t err) {
    return err != AGENTOS_LITE_SUCCESS;
}