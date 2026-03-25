/**
 * @file handler.c
 * @brief 错误处理实现（跨平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include "../observability/include/logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

/* ==================== 全局状态 ==================== */

static agentos_error_handler_t g_error_handler = NULL;
static agentos_error_info_handler_t g_error_info_handler = NULL;

#ifdef _WIN32
static CRITICAL_SECTION g_error_mutex;
static volatile LONG g_error_initialized = 0;

static void ensure_error_init(void) {
    if (InterlockedCompareExchange(&g_error_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_error_mutex);
    }
}

/**
 * @brief 获取单调时间（纳秒）- Windows实现
 */
static uint64_t get_monotonic_ns(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
}
#else
static pthread_mutex_t g_error_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 获取单调时间（纳秒）- POSIX实现
 */
static uint64_t get_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

/* ==================== 错误码字符串映射 ==================== */

/**
 * @brief 错误码信息结构
 */
typedef struct {
    int code;
    const char* name;
    const char* description;
    agentos_error_severity_t severity;
    agentos_error_category_t category;
} error_info_entry_t;

static const error_info_entry_t g_error_entries[] = {
    /* 成功 */
    {AGENTOS_SUCCESS, "SUCCESS", "成功", AGENTOS_ERROR_SEVERITY_INFO, AGENTOS_ERROR_CAT_SYSTEM},
    
    /* 通用错误 */
    {AGENTOS_EUNKNOWN, "EUNKNOWN", "未知错误", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_EINVAL, "EINVAL", "无效参数", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_ENOMEM, "ENOMEM", "内存不足", AGENTOS_ERROR_SEVERITY_CRITICAL, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_EBUSY, "EBUSY", "资源忙碌", AGENTOS_ERROR_SEVERITY_WARNING, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_ENOENT, "ENOENT", "不存在", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_EPERM, "EPERM", "权限不足", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SECURITY},
    {AGENTOS_ETIMEDOUT, "ETIMEDOUT", "超时", AGENTOS_ERROR_SEVERITY_WARNING, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_EEXIST, "EEXIST", "已存在", AGENTOS_ERROR_SEVERITY_WARNING, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_ECANCELED, "ECANCELED", "已取消", AGENTOS_ERROR_SEVERITY_INFO, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_ENOTSUP, "ENOTSUP", "不支持", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    
    /* 系统错误 */
    {AGENTOS_EIO, "EIO", "I/O错误", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_EINTR, "EINTR", "被中断", AGENTOS_ERROR_SEVERITY_WARNING, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_EOVERFLOW, "EOVERFLOW", "溢出", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_EBADF, "EBADF", "错误的文件描述符", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_ENOTINIT, "ENOTINIT", "未初始化", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    {AGENTOS_ERESOURCE, "ERESOURCE", "资源不足", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SYSTEM},
    
    /* 内核错误 */
    {AGENTOS_EIPCFAIL, "EIPCFAIL", "IPC通信失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_KERNEL},
    {AGENTOS_ETASKFAIL, "ETASKFAIL", "任务失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_KERNEL},
    {AGENTOS_ESYNCFAIL, "ESYNCFAIL", "同步失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_KERNEL},
    {AGENTOS_ELOCKFAIL, "ELOCKFAIL", "锁失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_KERNEL},
    
    /* 认知层错误 */
    {AGENTOS_EPLANFAIL, "EPLANFAIL", "规划失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_COGNITION},
    {AGENTOS_ECOORDFAIL, "ECOORDFAIL", "协调失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_COGNITION},
    {AGENTOS_EDISPFAIL, "EDISPFAIL", "调度失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_COGNITION},
    {AGENTOS_EINTENTFAIL, "EINTENTFAIL", "意图理解失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_COGNITION},
    
    /* 执行层错误 */
    {AGENTOS_EEXECFAIL, "EEXECFAIL", "执行失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_EXECUTION},
    {AGENTOS_ECOMPENSATE, "ECOMPENSATE", "补偿失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_EXECUTION},
    {AGENTOS_ERETRYEXCEEDED, "ERETRYEXCEEDED", "重试次数超限", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_EXECUTION},
    {AGENTOS_EUNITNOTFOUND, "EUNITNOTFOUND", "执行单元未找到", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_EXECUTION},
    
    /* 记忆层错误 */
    {AGENTOS_EMEMWRITE, "EMEMWRITE", "记忆写入失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_MEMORY},
    {AGENTOS_EMEMREAD, "EMEMREAD", "记忆读取失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_MEMORY},
    {AGENTOS_EMEMQUERY, "EMEMQUERY", "记忆查询失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_MEMORY},
    {AGENTOS_EEVOLVE, "EEVOLVE", "演化失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_MEMORY},
    
    /* 安全错误 */
    {AGENTOS_ESECURITY, "ESECURITY", "安全违规", AGENTOS_ERROR_SEVERITY_CRITICAL, AGENTOS_ERROR_CAT_SECURITY},
    {AGENTOS_ESANITIZE, "ESANITIZE", "输入净化失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SECURITY},
    {AGENTOS_EAUDIT, "EAUDIT", "审计失败", AGENTOS_ERROR_SEVERITY_ERROR, AGENTOS_ERROR_CAT_SECURITY},
};

#define ERROR_ENTRIES_COUNT (sizeof(g_error_entries) / sizeof(g_error_entries[0]))

/* ==================== 核心接口实现 ==================== */

/**
 * @brief 获取错误码的字符串描述
 */
const char* agentos_error_str(agentos_error_t err) {
    for (size_t i = 0; i < ERROR_ENTRIES_COUNT; i++) {
        if (g_error_entries[i].code == err) {
            return g_error_entries[i].description;
        }
    }
    return "未知错误码";
}

/**
 * @brief 获取错误码的严重程度
 */
agentos_error_severity_t agentos_error_get_severity(agentos_error_t err) {
    for (size_t i = 0; i < ERROR_ENTRIES_COUNT; i++) {
        if (g_error_entries[i].code == err) {
            return g_error_entries[i].severity;
        }
    }
    return AGENTOS_ERROR_SEVERITY_ERROR;
}

/**
 * @brief 获取错误码的类别
 */
agentos_error_category_t agentos_error_get_category(agentos_error_t err) {
    for (size_t i = 0; i < ERROR_ENTRIES_COUNT; i++) {
        if (g_error_entries[i].code == err) {
            return g_error_entries[i].category;
        }
    }
    return AGENTOS_ERROR_CAT_SYSTEM;
}

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
 * @brief 设置结构化错误处理回调
 */
void agentos_error_set_info_handler(agentos_error_info_handler_t handler) {
#ifdef _WIN32
    ensure_error_init();
    EnterCriticalSection(&g_error_mutex);
#else
    pthread_mutex_lock(&g_error_mutex);
#endif
    g_error_info_handler = handler;
#ifdef _WIN32
    LeaveCriticalSection(&g_error_mutex);
#else
    pthread_mutex_unlock(&g_error_mutex);
#endif
}

/**
 * @brief 创建结构化错误信息
 */
void agentos_error_create_info(
    agentos_error_t err,
    const char* module,
    const char* function,
    const char* file,
    int line,
    const char* message,
    agentos_error_info_t* out_info) {
    
    if (!out_info) return;
    
    out_info->code = err;
    out_info->severity = agentos_error_get_severity(err);
    out_info->category = agentos_error_get_category(err);
    out_info->module = module;
    out_info->function = function;
    out_info->file = file;
    out_info->line = line;
    out_info->timestamp_ns = get_monotonic_ns();
    out_info->context = NULL;
    
    if (message) {
        strncpy(out_info->message, message, sizeof(out_info->message) - 1);
        out_info->message[sizeof(out_info->message) - 1] = '\0';
    } else {
        strncpy(out_info->message, agentos_error_str(err), sizeof(out_info->message) - 1);
        out_info->message[sizeof(out_info->message) - 1] = '\0';
    }
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
    agentos_error_info_handler_t info_handler;
    
#ifdef _WIN32
    ensure_error_init();
    EnterCriticalSection(&g_error_mutex);
    handler = g_error_handler;
    info_handler = g_error_info_handler;
    LeaveCriticalSection(&g_error_mutex);
#else
    pthread_mutex_lock(&g_error_mutex);
    handler = g_error_handler;
    info_handler = g_error_info_handler;
    pthread_mutex_unlock(&g_error_mutex);
#endif

    if (info_handler) {
        agentos_error_info_t info;
        agentos_error_create_info(err, NULL, NULL, file, line, buf, &info);
        info_handler(&info);
    } else if (handler) {
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
    agentos_error_info_handler_t info_handler;
    
#ifdef _WIN32
    ensure_error_init();
    EnterCriticalSection(&g_error_mutex);
    handler = g_error_handler;
    info_handler = g_error_info_handler;
    LeaveCriticalSection(&g_error_mutex);
#else
    pthread_mutex_lock(&g_error_mutex);
    handler = g_error_handler;
    info_handler = g_error_info_handler;
    pthread_mutex_unlock(&g_error_mutex);
#endif

    if (info_handler) {
        agentos_error_info_t info;
        agentos_error_create_info(err, NULL, function, file, line, buf, &info);
        info.context = user_data;
        info_handler(&info);
    } else if (handler) {
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
