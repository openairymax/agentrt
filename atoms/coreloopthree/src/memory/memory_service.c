/**
 * @file memory_service.c
 * @brief 记忆服务高级接口（异步操作、批量处理等）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "memory.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 异步写入请求结构
 */
typedef struct async_write_req {
    agentos_memory_engine_t* engine;
    agentos_memory_record_t* record;
    void (*callback)(agentos_error_t err, const char* record_id, void* userdata);
    void* userdata;
} async_write_req_t;

static void async_write_thread(void* arg) {
    async_write_req_t* req = (async_write_req_t*)arg;
    char* record_id = NULL;
    agentos_error_t err = agentos_memory_write(req->engine, req->record, &record_id);
    if (req->callback) {
        req->callback(err, record_id, req->userdata);
    }
    if (record_id) free(record_id);
    
    // 释放深拷贝的记录
    if (req->record) {
        if (req->record->record_id) free(req->record->record_id);
        if (req->record->source_agent) free(req->record->source_agent);
        if (req->record->trace_id) free(req->record->trace_id);
        if (req->record->data) free(req->record->data);
        free(req->record);
    }
    free(req);
}

agentos_error_t agentos_memory_write_async(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    void (*callback)(agentos_error_t err, const char* record_id, void* userdata),
    void* userdata) {

    if (!engine || !record) return AGENTOS_EINVAL;

    // 创建请求对象
    async_write_req_t* req = (async_write_req_t*)malloc(sizeof(async_write_req_t));
    if (!req) return AGENTOS_ENOMEM;

    // 复制记录（深拷贝）
    agentos_memory_record_t* rec_copy = (agentos_memory_record_t*)malloc(sizeof(agentos_memory_record_t));
    if (!rec_copy) {
        free(req);
        return AGENTOS_ENOMEM;
    }
    memcpy(rec_copy, record, sizeof(agentos_memory_record_t));
    
    // 深拷贝字符串字段
    if (record->record_id) {
        rec_copy->record_id = strdup(record->record_id);
        if (!rec_copy->record_id) {
            free(rec_copy);
            free(req);
            return AGENTOS_ENOMEM;
        }
    }
    
    if (record->source_agent) {
        rec_copy->source_agent = strdup(record->source_agent);
        if (!rec_copy->source_agent) {
            if (rec_copy->record_id) free(rec_copy->record_id);
            free(rec_copy);
            free(req);
            return AGENTOS_ENOMEM;
        }
    }
    
    if (record->trace_id) {
        rec_copy->trace_id = strdup(record->trace_id);
        if (!rec_copy->trace_id) {
            if (rec_copy->record_id) free(rec_copy->record_id);
            if (rec_copy->source_agent) free(rec_copy->source_agent);
            free(rec_copy);
            free(req);
            return AGENTOS_ENOMEM;
        }
    }
    
    if (record->data && record->data_len > 0) {
        rec_copy->data = malloc(record->data_len);
        if (!rec_copy->data) {
            if (rec_copy->record_id) free(rec_copy->record_id);
            if (rec_copy->source_agent) free(rec_copy->source_agent);
            if (rec_copy->trace_id) free(rec_copy->trace_id);
            free(rec_copy);
            free(req);
            return AGENTOS_ENOMEM;
        }
        memcpy(rec_copy->data, record->data, record->data_len);
    }

    req->engine = engine;
    req->record = rec_copy;
    req->callback = callback;
    req->userdata = userdata;

    // 创建工作线程（简化：直接创建线程，实际应使用线程池）
    agentos_thread_t thread;
    agentos_error_t err = agentos_thread_create(&thread, async_write_thread, req);
    if (err != AGENTOS_SUCCESS) {
        free(rec_copy);
        free(req);
        return err;
    }
    agentos_thread_detach(thread);
    return AGENTOS_SUCCESS;
}