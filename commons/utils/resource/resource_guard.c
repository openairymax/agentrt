/**
 * @file resource_guard.c
 * @brief 资源作用域守卫实�?- RAII 模式
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "resource_guard.h"
#include "../memory/include/memory.h"
#include "../sync/include/sync.h"
#include "../string/include/string.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../utils/memory/include/memory_compat.h"
#include "../../utils/string/include/string_compat.h"
#include <string.h>

/* ==================== 核心接口实现 ==================== */

void agentos_resource_guard_init(
    agentos_resource_guard_t* guard,
    void* resource,
    agentos_resource_cleanup_t cleanup,
    const char* file,
    int line,
    const char* name)
{
    if (!guard) {
        return;
    }
    
    guard->resource = resource;
    guard->cleanup = cleanup;
    guard->file = file;
    guard->line = line;
    guard->name = name;
    guard->active = 1;
}

void agentos_resource_guard_cleanup(agentos_resource_guard_t* guard) {
    if (!guard || !guard->active) {
        return;
    }
    
    if (guard->cleanup && guard->resource) {
        guard->cleanup(guard->resource);
    }
    
    guard->active = 0;
    guard->resource = NULL;
    guard->cleanup = NULL;
}

void agentos_resource_guard_dismiss(agentos_resource_guard_t* guard) {
    if (!guard) {
        return;
    }
    
    guard->active = 0;
}

/* ==================== 资源追踪实现 ==================== */

#ifdef AGENTOS_RESOURCE_TRACKING

#include <pthread.h>
#include <stdint.h>

typedef struct agentos_resource_record {
    void* resource;
    const char* type;
    const char* file;
    int line;
    uint64_t timestamp_ns;
    struct agentos_resource_record* next;
} agentos_resource_record_t;

static agentos_resource_record_t* g_resource_head = NULL;
static pthread_mutex_t g_resource_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t get_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void agentos_resource_track_alloc(void* resource, const char* type, const char* file, int line) {
    if (!resource) {
        return;
    }
    
    agentos_resource_record_t* record = (agentos_resource_record_t*)memory_alloc(sizeof(agentos_resource_record_t), "resource_record");
    if (!record) {
        return;
    }
    
    record->resource = resource;
    record->type = type;
    record->file = file;
    record->line = line;
    record->timestamp_ns = get_monotonic_ns();
    
    pthread_mutex_lock(&g_resource_mutex);
    record->next = g_resource_head;
    g_resource_head = record;
    pthread_mutex_unlock(&g_resource_mutex);
}

void agentos_resource_track_free(void* resource) {
    if (!resource) {
        return;
    }
    
    pthread_mutex_lock(&g_resource_mutex);
    
    agentos_resource_record_t* prev = NULL;
    agentos_resource_record_t* curr = g_resource_head;
    
    while (curr) {
        if (curr->resource == resource) {
            if (prev) {
                prev->next = curr->next;
            } else {
                g_resource_head = curr->next;
            }
            memory_free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&g_resource_mutex);
}

int agentos_resource_track_report(char** out_report) {
    pthread_mutex_lock(&g_resource_mutex);
    
    int count = 0;
    agentos_resource_record_t* curr = g_resource_head;
    while (curr) {
        count++;
        curr = curr->next;
    }
    
    if (out_report) {
        size_t buf_size = 4096;
        char* buf = (char*)memory_alloc(buf_size, "resource_report_buffer");
        if (buf) {
            size_t offset = 0;
            offset += snprintf(buf + offset, buf_size - offset, "Resource leak report:\n");
            offset += snprintf(buf + offset, buf_size - offset, "===================\n");
            offset += snprintf(buf + offset, buf_size - offset, "Total leaks: %d\n\n", count);
            
            curr = g_resource_head;
            int i = 0;
            while (curr && i < 100) {
                offset += snprintf(buf + offset, buf_size - offset,
                    "[%d] Type: %s, Ptr: %p, File: %s:%d, Time: %lu ns\n",
                    i + 1, curr->type, curr->resource, curr->file, curr->line, curr->timestamp_ns);
                curr = curr->next;
                i++;
            }
            
            if (count > 100) {
                offset += snprintf(buf + offset, buf_size - offset, "... and %d more\n", count - 100);
            }
            
            *out_report = buf;
        }
    }
    
    pthread_mutex_unlock(&g_resource_mutex);
    return count;
}

void agentos_resource_track_clear(void) {
    pthread_mutex_lock(&g_resource_mutex);
    
    agentos_resource_record_t* curr = g_resource_head;
    while (curr) {
        agentos_resource_record_t* next = curr->next;
        AGENTOS_FREE(curr);
        curr = next;
    }
    g_resource_head = NULL;
    
    pthread_mutex_unlock(&g_resource_mutex);
}

#endif /* AGENTOS_RESOURCE_TRACKING */
