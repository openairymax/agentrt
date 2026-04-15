/**
 * @file memory_service.c
 * @brief 记忆服务高级接口（异步操作、批量处理等）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory.h"
#include "logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>

typedef struct async_write_req {
    agentos_memory_engine_t* engine;
    agentos_memory_record_t* record;
    void (*callback)(agentos_error_t err, const char* record_id, void* userdata);
    void* userdata;
} async_write_req_t;

/**
 * @brief 释放深拷贝的记忆记录
 */
static void free_record_copy(agentos_memory_record_t* rec) {
    if (!rec) return;
    if (rec->record_id) AGENTOS_FREE(rec->record_id);
    if (rec->source_agent) AGENTOS_FREE(rec->source_agent);
    if (rec->trace_id) AGENTOS_FREE(rec->trace_id);
    if (rec->data) AGENTOS_FREE(rec->data);
    AGENTOS_FREE(rec);
}

/**
 * @brief 深拷贝记忆记录
 * @return 成功返回拷贝指针，失败返回NULL
 */
static agentos_memory_record_t* deep_copy_record(const agentos_memory_record_t* record) {
    agentos_memory_record_t* copy = (agentos_memory_record_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_record_t));
    if (!copy) return NULL;

    if (record->record_id) {
        copy->record_id = AGENTOS_STRDUP(record->record_id);
        if (!copy->record_id) goto fail;
    }
    if (record->source_agent) {
        copy->source_agent = AGENTOS_STRDUP(record->source_agent);
        if (!copy->source_agent) goto fail;
    }
    if (record->trace_id) {
        copy->trace_id = AGENTOS_STRDUP(record->trace_id);
        if (!copy->trace_id) goto fail;
    }
    if (record->data && record->data_len > 0) {
        copy->data = AGENTOS_MALLOC(record->data_len);
        if (!copy->data) goto fail;
        memcpy(copy->data, record->data, record->data_len);
        copy->data_len = record->data_len;
    }
    copy->type = record->type;
    return copy;

fail:
    free_record_copy(copy);
    return NULL;
}

/**
 * @brief 异步写入线程入口
 */
static void async_write_thread(void* arg) {
    async_write_req_t* req = (async_write_req_t*)arg;
    if (!req) return;

    char* record_id = NULL;
    agentos_error_t err = agentos_memory_write(req->engine, req->record, &record_id);

    if (req->callback) {
        req->callback(err, record_id, req->userdata);
    }
    if (record_id) AGENTOS_FREE(record_id);

    free_record_copy(req->record);
    AGENTOS_FREE(req);
}

/**
 * @brief 异步写入记忆记录
 * @param engine 记忆引擎
 * @param record 记录数据（函数内部深拷贝，调用者可立即释放）
 * @param callback 写入完成回调（可为NULL）
 * @param userdata 回调用户数据
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_memory_write_async(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    void (*callback)(agentos_error_t err, const char* record_id, void* userdata),
    void* userdata) {

    if (!engine || !record) return AGENTOS_EINVAL;

    async_write_req_t* req = (async_write_req_t*)AGENTOS_CALLOC(1, sizeof(async_write_req_t));
    if (!req) return AGENTOS_ENOMEM;

    agentos_memory_record_t* rec_copy = deep_copy_record(record);
    if (!rec_copy) {
        AGENTOS_FREE(req);
        return AGENTOS_ENOMEM;
    }

    req->engine = engine;
    req->record = rec_copy;
    req->callback = callback;
    req->userdata = userdata;

    agentos_thread_t thread;
    agentos_error_t err = agentos_thread_create(&thread, async_write_thread, req);
    if (err != AGENTOS_SUCCESS) {
        free_record_copy(rec_copy);
        AGENTOS_FREE(req);
        return err;
    }
    agentos_thread_detach(thread);
    return AGENTOS_SUCCESS;
}
