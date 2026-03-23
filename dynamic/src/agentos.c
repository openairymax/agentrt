/**
 * @file agentos.c
 * @brief AgentOS Dynamic 模块核心依赖实现
 * 
 * 此文件提供 dynamic 模块所需的核心函数实现。
 * 完整实现请参考 atoms/corekern/src/ 目录。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#endif

/* ========== 错误码字符串 ========== */

static const char* error_strings[] = {
    [AGENTOS_SUCCESS]       = "Success",
    [-AGENTOS_EINVAL]       = "Invalid argument",
    [-AGENTOS_ENOMEM]       = "Out of memory",
    [-AGENTOS_EBUSY]        = "Resource busy",
    [-AGENTOS_ENOENT]       = "No such entity",
    [-AGENTOS_EPERM]        = "Permission denied",
    [-AGENTOS_ETIMEDOUT]    = "Timeout",
    [-AGENTOS_EEXIST]       = "Entity already exists",
    [-AGENTOS_ECANCELED]    = "Operation canceled",
    [-AGENTOS_ENOTSUP]      = "Not supported",
    [-AGENTOS_EIO]          = "I/O error",
    [-AGENTOS_EINTR]        = "Interrupted",
    [-AGENTOS_EOVERFLOW]    = "Overflow",
    [-AGENTOS_EBADF]        = "Bad file descriptor",
    [-AGENTOS_ENOTINIT]     = "Not initialized",
    [-AGENTOS_ERESOURCE]    = "Resource exhausted",
};

const char* agentos_strerror(agentos_error_t err) {
    if (err == AGENTOS_SUCCESS) {
        return "Success";
    }
    
    int idx = -err;
    if (idx > 0 && (size_t)idx < sizeof(error_strings) / sizeof(error_strings[0])) {
        return error_strings[idx];
    }
    
    return "Unknown error";
}

/* ========== 时间服务 ========== */

uint64_t agentos_time_monotonic_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency = {0};
    static int initialized = 0;
    
    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }
    
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

uint64_t agentos_time_monotonic_ms(void) {
    return agentos_time_monotonic_ns() / 1000000ULL;
}

uint64_t agentos_time_current_ns(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    uint64_t ns = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* Windows FILETIME 从 1601-01-01，转换为 Unix 时间戳 */
    ns -= 11644473600000000000ULL;  /* 1601-1970 的 100ns 间隔数 */
    return ns * 100;  /* 转换为纳秒 */
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

agentos_error_t agentos_time_nanosleep(uint64_t ns) {
#ifdef _WIN32
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (!timer) {
        return AGENTOS_ERROR;
    }
    
    LARGE_INTEGER li;
    li.QuadPart = -(LONGLONG)(ns / 100);  /* 负值表示相对时间，单位 100ns */
    
    SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    
    return AGENTOS_SUCCESS;
#else
    struct timespec req = {
        .tv_sec = ns / 1000000000ULL,
        .tv_nsec = ns % 1000000000ULL
    };
    
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        /* 被信号中断，继续睡眠 */
    }
    
    return AGENTOS_SUCCESS;
#endif
}

/* ========== 日志服务 ========== */

static int g_log_level = AGENTOS_LOG_LEVEL_INFO;
static FILE* g_log_file = NULL;
static int g_log_initialized = 0;

#ifdef _WIN32
static CRITICAL_SECTION g_log_lock;
static DWORD g_main_thread_id = 0;
#else
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_main_thread_id = 0;
#endif

#ifdef _WIN32
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif

static THREAD_LOCAL char t_trace_id[64] = {0};

agentos_error_t agentos_logger_init(const char* config_path) {
    (void)config_path;
    
    if (g_log_initialized) {
        return AGENTOS_SUCCESS;
    }
    
#ifdef _WIN32
    InitializeCriticalSection(&g_log_lock);
    g_main_thread_id = GetCurrentThreadId();
#else
    g_main_thread_id = pthread_self();
#endif
    
    g_log_file = stderr;
    g_log_initialized = 1;
    
    return AGENTOS_SUCCESS;
}

void agentos_logger_shutdown(void) {
    if (!g_log_initialized) return;
    
#ifdef _WIN32
    DeleteCriticalSection(&g_log_lock);
#endif
    
    g_log_initialized = 0;
}

const char* agentos_log_set_trace_id(const char* trace_id) {
    if (trace_id) {
        strncpy(t_trace_id, trace_id, sizeof(t_trace_id) - 1);
        t_trace_id[sizeof(t_trace_id) - 1] = '\0';
    } else {
        /* 自动生成 */
        snprintf(t_trace_id, sizeof(t_trace_id), "%016llx",
            (unsigned long long)agentos_time_monotonic_ns());
    }
    return t_trace_id;
}

const char* agentos_log_get_trace_id(void) {
    return t_trace_id[0] ? t_trace_id : NULL;
}

void agentos_log_write(int level, const char* file, int line, const char* fmt, ...) {
    if (!g_log_initialized) {
        agentos_logger_init(NULL);
    }
    
    if (level > g_log_level) return;
    
    const char* level_str = 
        (level == AGENTOS_LOG_LEVEL_ERROR) ? "ERROR" :
        (level == AGENTOS_LOG_LEVEL_WARN)  ? "WARN" :
        (level == AGENTOS_LOG_LEVEL_INFO)  ? "INFO" :
        (level == AGENTOS_LOG_LEVEL_DEBUG) ? "DEBUG" : "???";
    
    /* 获取时间 */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    /* 提取文件名 */
    const char* basename = strrchr(file, '/');
    if (!basename) basename = strrchr(file, '\\');
    if (!basename) basename = file;
    else basename++;
    
#ifdef _WIN32
    EnterCriticalSection(&g_log_lock);
#else
    pthread_mutex_lock(&g_log_lock);
#endif
    
    /* 写入日志头 */
    fprintf(g_log_file, "[%s] [%s] [%s:%d] ",
        time_buf, level_str, basename, line);
    
    /* 写入消息 */
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_file, fmt, args);
    va_end(args);
    
    fprintf(g_log_file, "\n");
    fflush(g_log_file);
    
#ifdef _WIN32
    LeaveCriticalSection(&g_log_lock);
#else
    pthread_mutex_unlock(&g_log_lock);
#endif
}
