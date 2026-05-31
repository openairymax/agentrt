/**
 * @file memory_service.c
 * @brief 记忆服务高级接口（异步操作、批量处理等）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory.h"
#include "memory_compat.h"
#include "platform.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct async_write_req {
    agentos_memory_engine_t *engine;
    agentos_memory_record_t *record;
    void (*callback)(int err, const char *record_id, void *userdata);
    void *userdata;
} async_write_req_t;

static void free_record_copy(agentos_memory_record_t *rec)
{
    if (!rec)
        return;
    if (rec->memory_record_id)
        AGENTOS_FREE(rec->memory_record_id);
    if (rec->memory_record_source_agent)
        AGENTOS_FREE(rec->memory_record_source_agent);
    if (rec->memory_record_trace_id)
        AGENTOS_FREE(rec->memory_record_trace_id);
    if (rec->memory_record_data)
        AGENTOS_FREE(rec->memory_record_data);
    AGENTOS_FREE(rec);
}

static agentos_memory_record_t *deep_copy_record(const agentos_memory_record_t *record)
{
    agentos_memory_record_t *copy =
        (agentos_memory_record_t *)AGENTOS_CALLOC(1, sizeof(agentos_memory_record_t));
    if (!copy) return NULL;

    if (record->memory_record_id) {
        copy->memory_record_id = AGENTOS_STRDUP(record->memory_record_id);
        if (!copy->memory_record_id)
            goto fail;
    }
    if (record->memory_record_source_agent) {
        copy->memory_record_source_agent = AGENTOS_STRDUP(record->memory_record_source_agent);
        if (!copy->memory_record_source_agent)
            goto fail;
    }
    if (record->memory_record_trace_id) {
        copy->memory_record_trace_id = AGENTOS_STRDUP(record->memory_record_trace_id);
        if (!copy->memory_record_trace_id)
            goto fail;
    }
    if (record->memory_record_data && record->memory_record_data_len > 0) {
        copy->memory_record_data = AGENTOS_MALLOC(record->memory_record_data_len);
        if (!copy->memory_record_data)
            goto fail;
        memcpy(copy->memory_record_data, record->memory_record_data,
               record->memory_record_data_len);
        copy->memory_record_data_len = record->memory_record_data_len;
    }
    copy->memory_record_type = record->memory_record_type;
    copy->memory_record_timestamp_ns = record->memory_record_timestamp_ns;
    copy->memory_record_importance = record->memory_record_importance;
    copy->memory_record_access_count = record->memory_record_access_count;
    return copy;

fail:
    free_record_copy(copy);
    return NULL;
}

static void *async_write_thread(void *arg)
{
    async_write_req_t *req = (async_write_req_t *)arg;
    if (!req)
        return NULL;

    char *record_id = NULL;
    int err = agentos_memory_write(req->engine, req->record, &record_id);

    if (req->callback) {
        req->callback(err, record_id, req->userdata);
    }
    if (record_id)
        AGENTOS_FREE(record_id);

    free_record_copy(req->record);
    AGENTOS_FREE(req);
    return NULL;
}

int agentos_memory_write_async(agentos_memory_engine_t *engine,
                               const agentos_memory_record_t *record,
                               void (*callback)(int err, const char *record_id, void *userdata),
                               void *userdata)
{

    if (!engine || !record)
        return AGENTOS_EINVAL;

    async_write_req_t *req = (async_write_req_t *)AGENTOS_CALLOC(1, sizeof(async_write_req_t));
    if (!req)
        return AGENTOS_ENOMEM;

    agentos_memory_record_t *rec_copy = deep_copy_record(record);
    if (!rec_copy) {
        AGENTOS_FREE(req);
        return AGENTOS_ENOMEM;
    }

    req->engine = engine;
    req->record = rec_copy;
    req->callback = callback;
    req->userdata = userdata;

    agentos_thread_t thread;
    int rc = agentos_platform_thread_create(&thread, async_write_thread, req);
    if (rc == 0) {
        pthread_detach(thread);
    }
    if (rc != 0) {
        free_record_copy(rec_copy);
        AGENTOS_FREE(req);
        return AGENTOS_EINVAL;
    }
    return AGENTOS_SUCCESS;
}
