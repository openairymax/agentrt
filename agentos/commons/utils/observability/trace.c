/**
 * @file trace.c
 * @brief 链路追踪实现（跨平台�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现分布式链路追踪功能�?
 * - 支持Span的创建和生命周期管理
 * - 提供事件注解和属性添�?
 * - 支持JSON格式的追踪导�?
 * - 符合OpenTelemetry追踪规范
 */

#include "trace.h"
#include "observability.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <time.h>
#include <ctype.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

#define MAX_SPANS 1024
#define MAX_EVENTS_PER_SPAN 64
#define MAX_TRACE_ID_LEN 64
#define MAX_SPAN_ID_LEN 32

/**
 * @brief 追踪事件结构
 */
typedef struct trace_event {
    char name[128];                      /**< 事件名称 */
    int64_t timestamp;                 /**< 事件时间戳（微秒�?*/
    char attributes[512];                /**< 事件属�?*/
    struct trace_event* next;           /**< 下一个事�?*/
} trace_event_t;

/**
 * @brief 跨平台互斥锁类型
 */
#ifdef _WIN32
typedef CRITICAL_SECTION trace_mutex_t;
#else
typedef pthread_mutex_t trace_mutex_t;
#endif

/**
 * @brief 初始化互斥锁
 */
static int trace_mutex_init(trace_mutex_t* mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

/**
 * @brief 销毁互斥锁
 */
static void trace_mutex_destroy(trace_mutex_t* mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

/**
 * @brief 加锁
 */
static void trace_mutex_lock(trace_mutex_t* mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

/**
 * @brief 解锁
 */
static void trace_mutex_unlock(trace_mutex_t* mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

/**
 * @brief 追踪Span内部结构
 */
struct agentos_trace_span {
    char trace_id[MAX_TRACE_ID_LEN];    /**< 追踪ID */
    char span_id[MAX_SPAN_ID_LEN];      /**< Span ID */
    char parent_id[MAX_SPAN_ID_LEN];    /**< 父Span ID */
    char name[128];                      /**< Span名称 */
    int64_t start_time;                 /**< 开始时间（微秒�?*/
    int64_t end_time;                   /**< 结束时间（微秒） */
    atomic_int status;                 /**< 状态：0=运行�? 1=完成, 2=错误 */
    trace_event_t* events;             /**< 事件链表 */
    trace_event_t* events_tail;         /**< 事件链表�?*/
    int event_count;                    /**< 事件数量 */
    trace_mutex_t mutex;               /**< 互斥�?*/
    struct agentos_trace_span* next;   /**< 下一个Span */
};

/**
 * @brief 全局追踪状�?
 */
static struct {
    atomic_uint64_t span_counter;      /**< Span计数�?*/
    atomic_uint64_t trace_counter;     /**< 追踪计数�?*/
    agentos_trace_span_t* head;        /**< Span链表�?*/
    agentos_trace_span_t* tail;        /**< Span链表�?*/
    trace_mutex_t mutex;               /**< 互斥�?*/
    int initialized;                   /**< 初始化标�?*/
} g_trace_state;

/**
 * @brief 获取当前时间戳（微秒�? 跨平台实�?
 */
static int64_t get_current_time_us(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (int64_t)((uli.QuadPart - 116444736000000000LL) / 10);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
}

/**
 * @brief 初始化追踪系�?
 */
static int init_trace_system(void) {
    if (g_trace_state.initialized) {
        return 0;
    }

    if (trace_mutex_init(&g_trace_state.mutex) != 0) {
        return -1;
    }

    atomic_init(&g_trace_state.span_counter, 0);
    atomic_init(&g_trace_state.trace_counter, 0);
    g_trace_state.head = NULL;
    g_trace_state.tail = NULL;
    g_trace_state.initialized = 1;
    return 0;
}

/**
 * @brief 生成ID
 */
static void generate_id(char* buffer, size_t size, uint64_t counter, const char* prefix) {
    snprintf(buffer, size, "%s-%016llx-%08llx",
             prefix,
             (unsigned long long)time(NULL),
             (unsigned long long)counter);
}

/**
 * @brief 创建追踪事件
 */
static trace_event_t* create_event(const char* name, const char* attributes) {
    trace_event_t* event = (trace_event_t*)AGENTOS_MALLOC(sizeof(trace_event_t));
    if (!event) {
        return NULL;
    }

    memset(event, 0, sizeof(trace_event_t));

    if (name) {
        strncpy(event->name, name, sizeof(event->name) - 1);
        event->name[sizeof(event->name) - 1] = '\0';
    }

    event->timestamp = get_current_time_us();

    if (attributes) {
        strncpy(event->attributes, attributes, sizeof(event->attributes) - 1);
        event->attributes[sizeof(event->attributes) - 1] = '\0';
    }

    return event;
}

/**
 * @brief 释放事件链表
 */
static void free_events(trace_event_t* head) {
    while (head) {
        trace_event_t* next = head->next;
        AGENTOS_FREE(head);
        head = next;
    }
}

agentos_trace_span_t* agentos_trace_begin(const char* name, const char* parent_id) {
    if (!name) {
        return NULL;
    }

    if (init_trace_system() != 0) {
        return NULL;
    }

    agentos_trace_span_t* span = (agentos_trace_span_t*)AGENTOS_MALLOC(
        sizeof(agentos_trace_span_t));
    if (!span) {
        return NULL;
    }

    memset(span, 0, sizeof(agentos_trace_span_t));

    uint64_t trace_id = atomic_fetch_add(&g_trace_state.trace_counter, 1);
    uint64_t span_id = atomic_fetch_add(&g_trace_state.span_counter, 1);

    generate_id(span->trace_id, sizeof(span->trace_id), trace_id, "tr");
    generate_id(span->span_id, sizeof(span->span_id), span_id, "sp");

    if (parent_id) {
        strncpy(span->parent_id, parent_id, sizeof(span->parent_id) - 1);
        span->parent_id[sizeof(span->parent_id) - 1] = '\0';
    } else {
        span->parent_id[0] = '\0';
    }

    strncpy(span->name, name, sizeof(span->name) - 1);
    span->name[sizeof(span->name) - 1] = '\0';

    span->start_time = get_current_time_us();
    span->end_time = 0;
    atomic_init(&span->status, 0);
    span->events = NULL;
    span->events_tail = NULL;
    span->event_count = 0;

    if (trace_mutex_init(&span->mutex) != 0) {
        AGENTOS_FREE(span);
        return NULL;
    }

    trace_mutex_lock(&g_trace_state.mutex);

    span->next = NULL;
    if (g_trace_state.tail) {
        g_trace_state.tail->next = span;
        g_trace_state.tail = span;
    } else {
        g_trace_state.head = span;
        g_trace_state.tail = span;
    }

    trace_mutex_unlock(&g_trace_state.mutex);

    return span;
}

void agentos_trace_end(agentos_trace_span_t* span) {
    if (!span) {
        return;
    }

    trace_mutex_lock(&span->mutex);

    if (atomic_load(&span->status) != 0) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    span->end_time = get_current_time_us();
    atomic_store(&span->status, 1);

    trace_mutex_unlock(&span->mutex);
}

void agentos_trace_add_event(agentos_trace_span_t* span, const char* name, const char* attributes) {
    if (!span || !name) {
        return;
    }

    trace_mutex_lock(&span->mutex);

    if (atomic_load(&span->status) == 2) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    if (span->event_count >= MAX_EVENTS_PER_SPAN) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    trace_event_t* event = create_event(name, attributes);
    if (!event) {
        trace_mutex_unlock(&span->mutex);
        return;
    }

    if (span->events_tail) {
        span->events_tail->next = event;
        span->events_tail = event;
    } else {
        span->events = event;
        span->events_tail = event;
    }

    span->event_count++;

    trace_mutex_unlock(&span->mutex);
}

char* agentos_trace_export(void) {
    if (init_trace_system() != 0) {
        return NULL;
    }

    trace_mutex_lock(&g_trace_state.mutex);

    size_t buffer_size = 4096;
    char* buffer = (char*)AGENTOS_MALLOC(buffer_size);
    if (!buffer) {
        trace_mutex_unlock(&g_trace_state.mutex);
        return NULL;
    }

    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, "[\n");

    agentos_trace_span_t* span = g_trace_state.head;
    int first = 1;

    while (span) {
        trace_mutex_lock(&span->mutex);

        if (!first) {
            offset += snprintf(buffer + offset, buffer_size - offset, ",\n");
        }
        first = 0;

        offset += snprintf(buffer + offset, buffer_size - offset, "  {\n");
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "    \"trace_id\": \"%s\",\n", span->trace_id);
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "    \"span_id\": \"%s\",\n", span->span_id);

        if (span->parent_id[0]) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                              "    \"parent_id\": \"%s\",\n", span->parent_id);
        }

        offset += snprintf(buffer + offset, buffer_size - offset,
                          "    \"name\": \"%s\",\n", span->name);
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "    \"start_time\": %lld,\n", (long long)span->start_time);

        if (span->end_time > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                              "    \"end_time\": %lld,\n", (long long)span->end_time);
            offset += snprintf(buffer + offset, buffer_size - offset,
                              "    \"duration_us\": %lld,\n", (long long)(span->end_time - span->start_time));
        }

        offset += snprintf(buffer + offset, buffer_size - offset,
                          "    \"status\": \"%s\",\n",
                          atomic_load(&span->status) == 1 ? "ok" :
                          atomic_load(&span->status) == 2 ? "error" : "running");

        if (span->event_count > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                              "    \"events\": [\n");

            trace_event_t* event = span->events;
            int first_event = 1;
            while (event) {
                if (!first_event) {
                    offset += snprintf(buffer + offset, buffer_size - offset, ",\n");
                }
                first_event = 0;

                offset += snprintf(buffer + offset, buffer_size - offset,
                                  "      {\"name\": \"%s\", \"timestamp\": %lld",
                                  event->name, (long long)event->timestamp);

                if (event->attributes[0]) {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                      ", \"attributes\": %s", event->attributes);
                }

                offset += snprintf(buffer + offset, buffer_size - offset, "}");
                event = event->next;
            }

            offset += snprintf(buffer + offset, buffer_size - offset, "\n    ]");
        }

        offset += snprintf(buffer + offset, buffer_size - offset, "\n  }");

        trace_mutex_unlock(&span->mutex);

        span = span->next;

        if (buffer_size > 512 && offset >= buffer_size - 512) {
            buffer_size *= 2;
            char* new_buffer = (char*)AGENTOS_REALLOC(buffer, buffer_size);
            if (!new_buffer) {
                trace_mutex_unlock(&g_trace_state.mutex);
                AGENTOS_FREE(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
    }

    offset += snprintf(buffer + offset, buffer_size - offset, "\n]");

    trace_mutex_unlock(&g_trace_state.mutex);

    return buffer;
}

void agentos_trace_cleanup(void) {
    if (!g_trace_state.initialized) {
        return;
    }

    trace_mutex_lock(&g_trace_state.mutex);

    agentos_trace_span_t* span = g_trace_state.head;
    while (span) {
        agentos_trace_span_t* next = span->next;

        trace_mutex_lock(&span->mutex);
        span->end_time = get_current_time_us();
        atomic_store(&span->status, 1);
        free_events(span->events);
        trace_mutex_unlock(&span->mutex);

        trace_mutex_destroy(&span->mutex);
        AGENTOS_FREE(span);

        span = next;
    }

    g_trace_state.head = NULL;
    g_trace_state.tail = NULL;

    trace_mutex_unlock(&g_trace_state.mutex);
}

int agentos_trace_get_span_count(void) {
    if (!g_trace_state.initialized) {
        return 0;
    }

    trace_mutex_lock(&g_trace_state.mutex);

    int count = 0;
    agentos_trace_span_t* span = g_trace_state.head;
    while (span) {
        count++;
        span = span->next;
    }

    trace_mutex_unlock(&g_trace_state.mutex);

    return count;
}
