/**
 * @file handler.c
 * @brief 错误处理实现（跨平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include "../observability/include/logger.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

static agentos_error_handler_t g_error_handler = NULL;

#ifdef _WIN32
static CRITICAL_SECTION g_error_mutex;
static volatile LONG g_error_initialized = 0;

static void ensure_error_init(void) {
    if (InterlockedCompareExchange(&g_error_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_error_mutex);
    }
}
#else
static pthread_mutex_t g_error_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/**
 * @brief 设置全局错误处理回调
 */
void agentos_error_set_handler(agentos_error_handler_t handler) {
#ifdef _WIN32
    ensure_error_init();
    EnterCriticalSection(&g_error_mutex);
#else
    pthread_mutex_lock(&g_error_mutex);
#endif
    g_error_handler = handler;
#ifdef _WIN32
    LeaveCriticalSection(&g_error_mutex);
#else
    pthread_mutex_unlock(&g_error_mutex);
#endif
}

/**
 * @brief 处理错误并记录日志
 */
void agentos_error_handle(agentos_error_t err, const char* file, int line, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AGENTOS_LOG_ERROR("Error %d (%s) at %s:%d: %s", err, agentos_error_str(err), file, line, buf);

    agentos_error_handler_t handler;
#ifdef _WIN32
    ensure_error_init();
    EnterCriticalSection(&g_error_mutex);
    handler = g_error_handler;
    LeaveCriticalSection(&g_error_mutex);
#else
    pthread_mutex_lock(&g_error_mutex);
    handler = g_error_handler;
    pthread_mutex_unlock(&g_error_mutex);
#endif

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

/**
 * @brief 带上下文的错误处理
 */
void agentos_error_handle_with_context(agentos_error_t err, const char* function, const char* file, int line, void* user_data, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AGENTOS_LOG_ERROR("Error %d (%s) at %s:%s:%d: %s", err, agentos_error_str(err), function, file, line, buf);

    agentos_error_handler_t handler;
#ifdef _WIN32
    ensure_error_init();
    EnterCriticalSection(&g_error_mutex);
    handler = g_error_handler;
    LeaveCriticalSection(&g_error_mutex);
#else
    pthread_mutex_lock(&g_error_mutex);
    handler = g_error_handler;
    pthread_mutex_unlock(&g_error_mutex);
#endif

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
