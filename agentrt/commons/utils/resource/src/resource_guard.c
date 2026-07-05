/**
 * @file resource_guard.c
 * @brief 资源作用域守卫实?- RAII 模式
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "resource_guard.h"

#include "../memory/include/agentrt_memory.h"
#include "../string/include/agentrt_string.h"
#include "../sync/include/sync.h"
#include "atomic_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 核心接口实现 ==================== */

void agentrt_resource_guard_init(agentrt_resource_guard_t *guard, void *resource,
                                 agentrt_resource_cleanup_t cleanup, const char *file, int line,
                                 const char *name)
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

void agentrt_resource_guard_cleanup(agentrt_resource_guard_t *guard)
{
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

void agentrt_resource_guard_dismiss(agentrt_resource_guard_t *guard)
{
    if (!guard) {
        return;
    }

    guard->active = 0;
}

/* ==================== 资源追踪实现 ==================== */

#ifdef AGENTRT_RESOURCE_TRACKING

#include "platform.h"

#include <stdint.h>

typedef struct agentrt_resource_record {
    void *resource;
    const char *type;
    const char *file;
    int line;
    uint64_t timestamp_ns;
    struct agentrt_resource_record *next;
} agentrt_resource_record_t;

static agentrt_resource_record_t *g_resource_head = NULL;
static agentrt_mutex_t g_resource_mutex;
static atomic_int g_resource_mutex_initialized = 0;

static void ensure_mutex_initialized(void)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_resource_mutex_initialized, &expected, 1,
                                                memory_order_seq_cst, memory_order_seq_cst)) {
        agentrt_mutex_init(&g_resource_mutex);
    }
}

static uint64_t get_monotonic_ns(void)
{
    return agentrt_time_ns();
}

void agentrt_resource_track_alloc(void *resource, const char *type, const char *file, int line)
{
    if (!resource) {
        return;
    }

    agentrt_resource_record_t *record = (agentrt_resource_record_t *)memory_alloc(
        sizeof(agentrt_resource_record_t), "resource_record");
    if (!record) {
        return;
    }

    record->resource = resource;
    record->type = type;
    record->file = file;
    record->line = line;
    record->timestamp_ns = get_monotonic_ns();

    ensure_mutex_initialized();
    agentrt_mutex_lock(&g_resource_mutex);
    record->next = g_resource_head;
    g_resource_head = record;
    agentrt_mutex_unlock(&g_resource_mutex);
}

void agentrt_resource_track_free(void *resource)
{
    if (!resource) {
        return;
    }

    ensure_mutex_initialized();
    agentrt_mutex_lock(&g_resource_mutex);

    agentrt_resource_record_t *prev = NULL;
    agentrt_resource_record_t *curr = g_resource_head;

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

    agentrt_mutex_unlock(&g_resource_mutex);
}

int agentrt_resource_track_report(char **out_report)
{
    ensure_mutex_initialized();
    agentrt_mutex_lock(&g_resource_mutex);

    int count = 0;
    agentrt_resource_record_t *curr = g_resource_head;
    while (curr) {
        count++;
        curr = curr->next;
    }

    if (out_report) {
        size_t buf_size = 4096;
        char *buf = (char *)memory_alloc(buf_size, "resource_report_buffer");
        if (buf) {
            size_t offset = 0;
            offset += snprintf(buf + offset, buf_size - offset, "Resource leak report:\n");
            offset += snprintf(buf + offset, buf_size - offset, "===================\n");
            offset += snprintf(buf + offset, buf_size - offset, "Total leaks: %d\n\n", count);

            curr = g_resource_head;
            int i = 0;
            while (curr && i < 100) {
                offset += snprintf(buf + offset, buf_size - offset,
                                   "[%d] Type: %s, Ptr: %p, File: %s:%d, Time: %lu ns\n", i + 1,
                                   curr->type, curr->resource, curr->file, curr->line,
                                   curr->timestamp_ns);
                curr = curr->next;
                i++;
            }

            if (count > 100) {
                offset +=
                    snprintf(buf + offset, buf_size - offset, "... and %d more\n", count - 100);
            }

            *out_report = buf;
        }
    }

    agentrt_mutex_unlock(&g_resource_mutex);
    return count;
}

void agentrt_resource_track_clear(void)
{
    ensure_mutex_initialized();
    agentrt_mutex_lock(&g_resource_mutex);

    agentrt_resource_record_t *curr = g_resource_head;
    while (curr) {
        agentrt_resource_record_t *next = curr->next;
        AGENTRT_FREE(curr);
        curr = next;
    }
    g_resource_head = NULL;

    agentrt_mutex_unlock(&g_resource_mutex);
}

#endif /* AGENTRT_RESOURCE_TRACKING */
