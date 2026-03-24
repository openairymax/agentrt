/**
 * @file resource_guard.c
 * @brief 资源作用域守卫实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "resource_guard.h"
#include "../observability/include/logger.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

/* ==================== 核心接口实现 ==================== */

/**
 * @brief 初始化资源守卫
 */
void agentos_resource_guard_init(
    agentos_resource_guard_t* guard,
    void* resource,
    agentos_resource_cleanup_t cleanup,
    const char* file,
    int line,
    const char* name) {
    
    if (!guard) return;
    
    guard->resource = resource;
    guard->cleanup = cleanup;
    guard->file = file;
    guard->line = line;
    guard->name = name;
    guard->active = 1;
}

/**
 * @brief 执行资源清理
 */
void agentos_resource_guard_cleanup(agentos_resource_guard_t* guard) {
    if (!guard) return;
    
    if (guard->active && guard->resource && guard->cleanup) {
        AGENTOS_LOG_DEBUG("Resource guard cleanup: %s at %s:%d", 
                         guard->name ? guard->name : "unknown",
                         guard->file ? guard->file : "unknown",
                         guard->line);
        guard->cleanup(guard->resource);
        guard->active = 0;
    }
}

/**
 * @brief 取消资源清理（转移所有权）
 */
void agentos_resource_guard_dismiss(agentos_resource_guard_t* guard) {
    if (!guard) return;
    
    AGENTOS_LOG_DEBUG("Resource guard dismissed: %s at %s:%d",
                     guard->name ? guard->name : "unknown",
                     guard->file ? guard->file : "unknown",
                     guard->line);
    guard->active = 0;
}

/* ==================== 资源追踪实现 ==================== */

#ifdef AGENTOS_RESOURCE_TRACKING

#define MAX_TRACKED_RESOURCES 4096

static agentos_resource_record_t g_resource_records[MAX_TRACKED_RESOURCES];
static int g_resource_count = 0;

#ifdef _WIN32
static CRITICAL_SECTION g_track_mutex;
static volatile LONG g_track_initialized = 0;

static void ensure_track_init(void) {
    if (InterlockedCompareExchange(&g_track_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_track_mutex);
    }
}

static uint64_t get_monotonic_ns(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
}
#else
static pthread_mutex_t g_track_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t get_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

/**
 * @brief 注册资源分配
 */
void agentos_resource_track_alloc(void* resource, const char* type, const char* file, int line) {
    if (!resource) return;
    
#ifdef _WIN32
    ensure_track_init();
    EnterCriticalSection(&g_track_mutex);
#else
    pthread_mutex_lock(&g_track_mutex);
#endif
    
    if (g_resource_count < MAX_TRACKED_RESOURCES) {
        agentos_resource_record_t* record = &g_resource_records[g_resource_count++];
        record->resource = resource;
        record->type = type;
        record->file = file;
        record->line = line;
        record->timestamp_ns = get_monotonic_ns();
        record->next = NULL;
        
        AGENTOS_LOG_DEBUG("Resource tracked: %s at %p (%s:%d)", 
                         type ? type : "unknown", resource, 
                         file ? file : "unknown", line);
    } else {
        AGENTOS_LOG_WARN("Resource tracking buffer full, cannot track: %p", resource);
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&g_track_mutex);
#else
    pthread_mutex_unlock(&g_track_mutex);
#endif
}

/**
 * @brief 注销资源分配
 */
void agentos_resource_track_free(void* resource) {
    if (!resource) return;
    
#ifdef _WIN32
    ensure_track_init();
    EnterCriticalSection(&g_track_mutex);
#else
    pthread_mutex_lock(&g_track_mutex);
#endif
    
    int found = 0;
    for (int i = 0; i < g_resource_count; i++) {
        if (g_resource_records[i].resource == resource) {
            AGENTOS_LOG_DEBUG("Resource untracked: %p (%s:%d)",
                             resource,
                             g_resource_records[i].file ? g_resource_records[i].file : "unknown",
                             g_resource_records[i].line);
            
            g_resource_records[i] = g_resource_records[--g_resource_count];
            found = 1;
            break;
        }
    }
    
    if (!found) {
        AGENTOS_LOG_WARN("Resource not found in tracking: %p", resource);
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&g_track_mutex);
#else
    pthread_mutex_unlock(&g_track_mutex);
#endif
}

/**
 * @brief 获取资源追踪报告
 */
int agentos_resource_track_report(char** out_report) {
    if (!out_report) return -1;
    
#ifdef _WIN32
    ensure_track_init();
    EnterCriticalSection(&g_track_mutex);
#else
    pthread_mutex_lock(&g_track_mutex);
#endif
    
    int count = g_resource_count;
    
    size_t buf_size = 1024 + count * 256;
    char* report = (char*)malloc(buf_size);
    if (!report) {
#ifdef _WIN32
        LeaveCriticalSection(&g_track_mutex);
#else
        pthread_mutex_unlock(&g_track_mutex);
#endif
        return -1;
    }
    
    int offset = snprintf(report, buf_size, "Resource Tracking Report\n");
    offset += snprintf(report + offset, buf_size - offset, "========================\n");
    offset += snprintf(report + offset, buf_size - offset, "Total tracked: %d\n\n", count);
    
    for (int i = 0; i < count && offset < (int)buf_size - 256; i++) {
        agentos_resource_record_t* rec = &g_resource_records[i];
        offset += snprintf(report + offset, buf_size - offset,
                          "[%d] %s at %p\n"
                          "    File: %s:%d\n"
                          "    Time: %" PRIu64 " ns\n\n",
                          i + 1,
                          rec->type ? rec->type : "unknown",
                          rec->resource,
                          rec->file ? rec->file : "unknown",
                          rec->line,
                          rec->timestamp_ns);
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&g_track_mutex);
#else
    pthread_mutex_unlock(&g_track_mutex);
#endif
    
    *out_report = report;
    return count;
}

/**
 * @brief 清空资源追踪记录
 */
void agentos_resource_track_clear(void) {
#ifdef _WIN32
    ensure_track_init();
    EnterCriticalSection(&g_track_mutex);
#else
    pthread_mutex_lock(&g_track_mutex);
#endif
    
    g_resource_count = 0;
    
#ifdef _WIN32
    LeaveCriticalSection(&g_track_mutex);
#else
    pthread_mutex_unlock(&g_track_mutex);
#endif
}

#endif /* AGENTOS_RESOURCE_TRACKING */
