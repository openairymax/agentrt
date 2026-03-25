/**
 * @file logger.c
 * @brief 日志实现（跨平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

#ifdef _WIN32
static CRITICAL_SECTION log_mutex;
static DWORD trace_key = TLS_OUT_OF_INDEXES;
static volatile LONG initialized = 0;

static void ensure_init(void) {
    if (InterlockedCompareExchange(&initialized, 1, 0) == 0) {
        InitializeCriticalSection(&log_mutex);
        trace_key = TlsAlloc();
    }
}
#else
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t trace_key;
static pthread_once_t trace_key_once = PTHREAD_ONCE_INIT;

static void make_key(void) {
    pthread_key_create(&trace_key, free);
}
#endif

/**
 * @brief 设置当前线程的追踪ID
 */
const char* agentos_log_set_trace_id(const char* trace_id) {
#ifdef _WIN32
    ensure_init();
    char* old = (char*)TlsGetValue(trace_key);
    if (old) free(old);
    char* new_id;
    if (trace_id) {
        new_id = _strdup(trace_id);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lx", (unsigned long)time(NULL) ^ (unsigned long)GetCurrentThreadId());
        new_id = _strdup(buf);
    }
    if (new_id) TlsSetValue(trace_key, new_id);
    return new_id;
#else
    pthread_once(&trace_key_once, make_key);
    char* old = (char*)pthread_getspecific(trace_key);
    if (old) free(old);
    char* new_id;
    if (trace_id) {
        new_id = strdup(trace_id);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lx", (unsigned long)time(NULL) ^ (unsigned long)pthread_self());
        new_id = strdup(buf);
    }
    if (new_id) pthread_setspecific(trace_key, new_id);
    return new_id;
#endif
}

/**
 * @brief 获取当前线程的追踪ID
 */
const char* agentos_log_get_trace_id(void) {
#ifdef _WIN32
    ensure_init();
    return (const char*)TlsGetValue(trace_key);
#else
    pthread_once(&trace_key_once, make_key);
    return (const char*)pthread_getspecific(trace_key);
#endif
}

static const char* level_str(int level) {
    switch (level) {
        case AGENTOS_LOG_LEVEL_ERROR: return "ERROR";
        case AGENTOS_LOG_LEVEL_WARN:  return "WARN";
        case AGENTOS_LOG_LEVEL_INFO:  return "INFO";
        case AGENTOS_LOG_LEVEL_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 记录日志
 */
void agentos_log_write(int level, const char* file, int line, const char* fmt, ...) {
    if (level > AGENTOS_LOG_LEVEL) return;

    double timestamp;
#ifdef _WIN32
    ensure_init();
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    timestamp = (double)(uli.QuadPart - 116444736000000000LL) / 10000000.0;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timestamp = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif

    time_t now = (time_t)timestamp;
    struct tm* tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    char message[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    char escaped_message[8192];
    int i = 0, j = 0;
    while (message[i] && j < (int)sizeof(escaped_message) - 1) {
        switch (message[i]) {
            case '\\':
                escaped_message[j++] = '\\';
                escaped_message[j++] = '\\';
                break;
            case '"':
                escaped_message[j++] = '\\';
                escaped_message[j++] = '"';
                break;
            case '\n':
                escaped_message[j++] = '\\';
                escaped_message[j++] = 'n';
                break;
            case '\r':
                escaped_message[j++] = '\\';
                escaped_message[j++] = 'r';
                break;
            case '\t':
                escaped_message[j++] = '\\';
                escaped_message[j++] = 't';
                break;
            default:
                escaped_message[j++] = message[i];
                break;
        }
        i++;
    }
    escaped_message[j] = '\0';

#ifdef _WIN32
    EnterCriticalSection(&log_mutex);
#else
    pthread_mutex_lock(&log_mutex);
#endif

    fprintf(stderr, "{\n");
    fprintf(stderr, "  \"timestamp\": %.3f,\n", timestamp);
    fprintf(stderr, "  \"level\": \"%s\",\n", level_str(level));
    fprintf(stderr, "  \"module\": \"%s\",\n", file);
    fprintf(stderr, "  \"line\": %d", line);
    const char* trace = agentos_log_get_trace_id();
    if (trace) {
        fprintf(stderr, ",\n  \"trace_id\": \"%s\"", trace);
    }
    fprintf(stderr, ",\n  \"message\": \"%s\"\n", escaped_message);
    fprintf(stderr, "}\n");
    fflush(stderr);

#ifdef _WIN32
    LeaveCriticalSection(&log_mutex);
#else
    pthread_mutex_unlock(&log_mutex);
#endif
}
