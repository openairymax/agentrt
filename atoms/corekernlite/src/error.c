/**
 * @file error.c
 * @brief AgentOS Lite 错误处理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/error.h"
#include <string.h>

static const char* error_messages[] = {
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
    "Interrupted system call",
    "Value overflow",
    "Bad file descriptor",
    "Subsystem not initialized",
    "Resource exhausted"
};

static const int error_count = sizeof(error_messages) / sizeof(error_messages[0]);

AGENTOS_LITE_API const char* agentos_lite_strerror(agentos_lite_error_t err) {
    if (err == AGENTOS_LITE_SUCCESS) {
        return error_messages[0];
    }
    
    int index = -err;
    if (index > 0 && index < error_count) {
        return error_messages[index];
    }
    
    return "Unknown error";
}
