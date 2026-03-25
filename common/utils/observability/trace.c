/**
 * @file trace.c
 * @brief 链路追踪实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块实现分布式链路追踪功能：
 * - 支持Span的创建和生命周期管理
 * - 提供事件注解和属性添加
 * - 支持JSON格式的追踪导出
 * - 符合OpenTelemetry追踪规范
 */

#include "trace.h"
#include "observability.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <ctype.h>

#define MAX_SPANS 1024
#define MAX_EVENTS_PER_SPAN 64
#define MAX_TRACE_ID_LEN 64
#define MAX_SPAN_ID_LEN 32

/**
 * @brief 追踪事件结构
 */
typedef struct trace_event {
    char name[128];                      /**< 事件名称 */
    int64_t timestamp;                 /**< 事件时间戳（微秒） */
    char attributes[512];                /**< 事件属性 */
    struct trace_event* next;           /**< 下一个事件 */
} trace_event_t;

/**
 * @brief 追踪Span内部结构
 */
struct agentos_trace_span {
    char trace_id[MAX_TRACE_ID_LEN];    /**< 追踪ID */
    char span_id[MAX_SPAN_ID_LEN];      /**< Span ID */
    char parent_id[MAX_SPAN_ID_LEN];    /**< 父Span ID */
    char name[128];                      /**< Span名称 */
    int64_t start_time;                 /**< 开始时间（微秒） */
    int64_t end_time;                   /**< 结束时间（微秒） */
    atomic_int status;                 /**< 状态：0=运行中, 1=完成, 2=错误 */
    trace_event_t* events;             /**< 事件链表 */
    trace_event_t* events_tail;         /**< 事件链表尾 */
    int event_count;                    /**< 事件数量 */
    pthread_mutex_t mutex;             /**< 互斥锁 */
    struct agentos_trace_span* next;   /**< 下一个Span */
};

/**
 * @brief 全局追踪状态
 */
static struct {
    atomic_uint64_t span_counter;      /**< Span计数器 */
    atomic_uint64_t trace_counter;     /**< 追踪计数器 */
    agentos_trace_span_t* head;        /**< Span链表头 */
    agentos_trace_span_t* tail;        /**< Span链表尾 */
    pthread_mutex_t mutex;             /**< 互斥锁 */
    int initialized;                   /**< 初始化标志 */
} g_trace_state = {
    .span_counter = 0,
    .trace_counter = 0,
    .head = NULL,
    .tail = NULL,
    .mutex = {0},
    .initialized = 0
};

/**
 * @brief 初始化追踪系统
 */
static int init_trace_system(void) {
    if (g_trace_state.initialized) {
        return 0;
    }
    
    if (pthread_mutex_init(&g_trace_state.mutex, NULL) != 0) {
        return -1;
    }
    
    g_trace_state.initialized = 1;
    return 0;
}

/**
 * @brief 生成ID
 */
static void generate_id(char* buffer, size_t size, uint64_t counter, const char* prefix) {
    snprintf(buffer, size, "%s-%016lx-%08lx", 
             prefix, 
             (unsigned long)time(NULL),
             (unsigned long)counter);
}

/**
 * @brief 获取当前时间戳（微秒）
 */
static int64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/**
 * @brief 创建追踪事件
 */
static trace_event_t* create_event(const char* name, const char* attributes) {
    trace_event_t* event = (trace_event_t*)malloc(sizeof(trace_event_t));
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
        free(head);
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
    
    agentos_trace_span_t* span = (agentos_trace_span_t*)malloc(
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
    
    if (pthread_mutex_init(&span->mutex, NULL) != 0) {
        free(span);
        return NULL;
    }
    
    pthread_mutex_lock(&g_trace_state.mutex);
    
    span->next = NULL;
    if (g_trace_state.tail) {
        g_trace_state.tail->next = span;
        g_trace_state.tail = span;
    } else {
        g_trace_state.head = span;
        g_trace_state.tail = span;
    }
    
    pthread_mutex_unlock(&g_trace_state.mutex);
    
    return span;
}

void agentos_trace_end(agentos_trace_span_t* span) {
    if (!span) {
        return;
    }
    
    pthread_mutex_lock(&span->mutex);
    
    if (atomic_load(&span->status) != 0) {
        pthread_mutex_unlock(&span->mutex);
        return;
    }
    
    span->end_time = get_current_time_us();
    atomic_store(&span->status, 1);
    
    pthread_mutex_unlock(&span->mutex);
}

void agentos_trace_add_event(agentos_trace_span_t* span, const char* name, const char* attributes) {
    if (!span || !name) {
        return;
    }
    
    pthread_mutex_lock(&span->mutex);
    
    if (atomic_load(&span->status) == 2) {
        pthread_mutex_unlock(&span->mutex);
        return;
    }
    
    if (span->event_count >= MAX_EVENTS_PER_SPAN) {
        pthread_mutex_unlock(&span->mutex);
        return;
    }
    
    trace_event_t* event = create_event(name, attributes);
    if (!event) {
        pthread_mutex_unlock(&span->mutex);
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
    
    pthread_mutex_unlock(&span->mutex);
}

char* agentos_trace_export(void) {
    if (init_trace_system() != 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_trace_state.mutex);
    
    size_t buffer_size = 4096;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) {
        pthread_mutex_unlock(&g_trace_state.mutex);
        return NULL;
    }
    
    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, "[\n");
    
    agentos_trace_span_t* span = g_trace_state.head;
    int first = 1;
    
    while (span) {
        pthread_mutex_lock(&span->mutex);
        
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
                          "    \"start_time\": %ld,\n", span->start_time);
        
        if (span->end_time > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, 
                              "    \"end_time\": %ld,\n", span->end_time);
            offset += snprintf(buffer + offset, buffer_size - offset, 
                              "    \"duration_us\": %ld,\n", span->end_time - span->start_time);
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
                                  "      {\"name\": \"%s\", \"timestamp\": %ld",
                                  event->name, event->timestamp);
                
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
        
        pthread_mutex_unlock(&span->mutex);
        
        span = span->next;
        
        if (offset >= buffer_size - 512) {
            buffer_size *= 2;
            char* new_buffer = (char*)realloc(buffer, buffer_size);
            if (!new_buffer) {
                pthread_mutex_unlock(&g_trace_state.mutex);
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, "\n]");
    
    pthread_mutex_unlock(&g_trace_state.mutex);
    
    return buffer;
}

void agentos_trace_cleanup(void) {
    pthread_mutex_lock(&g_trace_state.mutex);
    
    agentos_trace_span_t* span = g_trace_state.head;
    while (span) {
        agentos_trace_span_t* next = span->next;
        
        pthread_mutex_lock(&span->mutex);
        span->end_time = get_current_time_us();
        atomic_store(&span->status, 1);
        free_events(span->events);
        pthread_mutex_unlock(&span->mutex);
        
        pthread_mutex_destroy(&span->mutex);
        free(span);
        
        span = next;
    }
    
    g_trace_state.head = NULL;
    g_trace_state.tail = NULL;
    
    pthread_mutex_unlock(&g_trace_state.mutex);
}

int agentos_trace_get_span_count(void) {
    pthread_mutex_lock(&g_trace_state.mutex);
    
    int count = 0;
    agentos_trace_span_t* span = g_trace_state.head;
    while (span) {
        count++;
        span = span->next;
    }
    
    pthread_mutex_unlock(&g_trace_state.mutex);
    
    return count;
}
