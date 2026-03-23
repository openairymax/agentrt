/**
 * @file error.c
 * @brief 错误码转字符串实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"

static const char* error_strings[] = {
    "Success",
    "Invalid argument",
    "Out of memory",
    "Resource busy",
    "No such entry",
    "Permission denied",
    "Operation timed out",
    "Already exists",
    "Operation canceled",
    "Not supported",
    "I/O error",
    "Interrupted",
    "Value overflow",
    "Bad file descriptor",
    "Not initialized",
    "Resource exhausted"
};

#define ERROR_COUNT (sizeof(error_strings) / sizeof(error_strings[0]))

const char* agentos_strerror(agentos_error_t err) {
    if (err > 0 || err <= -(agentos_error_t)ERROR_COUNT) {
        return "Unknown error";
    }
    return error_strings[-err];
}
