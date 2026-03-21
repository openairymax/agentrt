/**
 * @file handler.c
 * @brief 错误处理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "error.h"
#include "../observability/include/logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

static agentos_error_handler_t g_error_handler = NULL;
static pthread_mutex_t g_error_mutex = PTHREAD_MUTEX_INITIALIZER;

void agentos_error_set_handler(agentos_error_handler_t handler) {
    pthread_mutex_lock(&g_error_mutex);
    g_error_handler = handler;
    pthread_mutex_unlock(&g_error_mutex);
}

void agentos_error_handle(agentos_error_t err, const char* file, int line, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AGENTOS_LOG_ERROR("Error %d (%s) at %s:%d: %s", err, agentos_error_str(err), file, line, buf);
    
    // 调用回调函数
    agentos_error_handler_t handler;
    pthread_mutex_lock(&g_error_mutex);
    handler = g_error_handler;
    pthread_mutex_unlock(&g_error_mutex);
    
    if (handler) {
        agentos_error_context_t context = {
            .function = NULL,
            .file = file,
            .line = line,
            .user_data = NULL
        };
        snprintf(context.message, sizeof(context.message), "%s", buf);
        handler(err, &context);
    }
}

void agentos_error_handle_with_context(agentos_error_t err, const char* function, const char* file, int line, void* user_data, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AGENTOS_LOG_ERROR("Error %d (%s) at %s:%s:%d: %s", err, agentos_error_str(err), function, file, line, buf);
    
    // 调用回调函数
    agentos_error_handler_t handler;
    pthread_mutex_lock(&g_error_mutex);
    handler = g_error_handler;
    pthread_mutex_unlock(&g_error_mutex);
    
    if (handler) {
        agentos_error_context_t context = {
            .function = function,
            .file = file,
            .line = line,
            .user_data = user_data
        };
        snprintf(context.message, sizeof(context.message), "%s", buf);
        handler(err, &context);
    }
}