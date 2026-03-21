/**
 * @file engine.c
 * @brief 记忆引擎实现，封装 MemoryRovol 接口（添加健康检查）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory.h"
#include "rov_ffi.h"
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

struct agentos_memory_engine {
    agentos_memoryrov_handle_t* rov_handle;
    agentos_mutex_t* lock;
    char* config_path;
};

agentos_error_t agentos_memory_create(
    const char* config_path,
    agentos_memory_engine_t** out_engine) {

    if (!out_engine) return AGENTOS_EINVAL;

    agentos_memory_engine_t* engine = (agentos_memory_engine_t*)malloc(sizeof(agentos_memory_engine_t));
    // From data intelligence emerges. by spharx
    if (!engine) return AGENTOS_ENOMEM;
    memset(engine, 0, sizeof(agentos_memory_engine_t));

    if (config_path) {
        engine->config_path = strdup(config_path);
        if (!engine->config_path) {
            free(engine);
            return AGENTOS_ENOMEM;
        }
    }

    agentos_error_t err = agentos_memoryrov_create(config_path, &engine->rov_handle);
    if (err != AGENTOS_SUCCESS) {
        if (engine->config_path) free(engine->config_path);
        free(engine);
        return err;
    }

    engine->lock = agentos_mutex_create();
    if (!engine->lock) {
        agentos_memoryrov_destroy(engine->rov_handle);
        if (engine->config_path) free(engine->config_path);
        free(engine);
        return AGENTOS_ENOMEM;
    }

    *out_engine = engine;
    return AGENTOS_SUCCESS;
}

void agentos_memory_destroy(agentos_memory_engine_t* engine) {
    if (!engine) return;
    agentos_mutex_lock(engine->lock);
    if (engine->rov_handle) {
        agentos_memoryrov_destroy(engine->rov_handle);
    }
    agentos_mutex_unlock(engine->lock);
    agentos_mutex_destroy(engine->lock);
    if (engine->config_path) free(engine->config_path);
    free(engine);
}

agentos_error_t agentos_memory_write(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    char** out_record_id) {

    if (!engine || !record || !out_record) return AGENTOS_EINVAL;

    char metadata[512];
    int len = snprintf(metadata, sizeof(metadata),
        "{\"source\":\"%s\",\"trace\":\"%s\",\"type\":%d}",
        record->source_agent ? record->source_agent : "",
        record->trace_id ? record->trace_id : "",
        (int)record->type);

    if (len < 0 || len >= (int)sizeof(metadata)) {
        return AGENTOS_EOVERFLOW;
    }

    return agentos_memoryrov_write_raw(
        engine->rov_handle,
        record->data,
        record->data_len,
        metadata,
        out_record_id);
}

agentos_error_t agentos_memory_query(
    agentos_memory_engine_t* engine,
    const agentos_memory_query_t* query,
    agentos_memory_result_t** out_result) {

    if (!engine || !query || !out_result) return AGENTOS_EINVAL;

    char** results = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_memoryrov_query(
        engine->rov_handle,
        query->text,
        query->limit,
        &results,
        &count);

    if (err != AGENTOS_SUCCESS) return err;

    agentos_memory_result_t* res = (agentos_memory_result_t*)malloc(sizeof(agentos_memory_result_t));
    if (!res) {
        for (size_t i = 0; i < count; i++) free(results[i]);
        free(results);
        return AGENTOS_ENOMEM;
    }
    memset(res, 0, sizeof(agentos_memory_result_t));

    res->items = (agentos_memory_result_item_t**)malloc(count * sizeof(agentos_memory_result_item_t*));
    if (!res->items) {
        for (size_t i = 0; i < count; i++) free(results[i]);
        free(results);
        free(res);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < count; i++) {
        res->items[i] = (agentos_memory_result_item_t*)malloc(sizeof(agentos_memory_result_item_t));
        if (!res->items[i]) {
            for (size_t j = 0; j < i; j++) {
                free(res->items[j]->record_id);
                free(res->items[j]);
            }
            for (size_t j = 0; j < count; j++) free(results[j]);
            free(results);
            free(res->items);
            free(res);
            return AGENTOS_ENOMEM;
        }
        memset(res->items[i], 0, sizeof(agentos_memory_result_item_t));
        res->items[i]->record_id = results[i];
        res->items[i]->score = 0.0f;
        if (query->include_raw) {
            res->items[i]->record = NULL;
        }
    }
    res->count = count;
    res->query_time_ns = 0;

    free(results);
    *out_result = res;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memory_get(
    agentos_memory_engine_t* engine,
    const char* record_id,
    int include_raw,
    agentos_memory_record_t** out_record) {

    if (!engine || !record_id || !out_record) return AGENTOS_EINVAL;

    void* data = NULL;
    size_t len = 0;
    agentos_error_t err = agentos_memoryrov_get_raw(engine->rov_handle, record_id, &data, &len);
    if (err != AGENTOS_SUCCESS) return err;

    agentos_memory_record_t* rec = (agentos_memory_record_t*)malloc(sizeof(agentos_memory_record_t));
    if (!rec) {
        free(data);
        return AGENTOS_ENOMEM;
    }
    memset(rec, 0, sizeof(agentos_memory_record_t));

    rec->record_id = strdup(record_id);
    rec->data = data;
    rec->data_len = len;
    rec->type = MEMORY_TYPE_RAW;

    *out_record = rec;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memory_mount(
    agentos_memory_engine_t* engine,
    const char* record_id,
    const char* context) {

    if (!engine || !record_id) return AGENTOS_EINVAL;
    return agentos_memoryrov_mount(engine->rov_handle, record_id, context);
}

void agentos_memory_result_free(agentos_memory_result_t* result) {
    if (!result) return;
    for (size_t i = 0; i < result->count; i++) {
        if (result->items[i]) {
            if (result->items[i]->record_id) free(result->items[i]->record_id);
            if (result->items[i]->record) agentos_memory_record_free(result->items[i]->record);
            free(result->items[i]);
        }
    }
    free(result->items);
    free(result);
}

void agentos_memory_record_free(agentos_memory_record_t* record) {
    if (!record) return;
    if (record->record_id) free(record->record_id);
    if (record->source_agent) free(record->source_agent);
    if (record->trace_id) free(record->trace_id);
    if (record->data) free(record->data);
    free(record);
}

agentos_error_t agentos_memory_evolve(
    agentos_memory_engine_t* engine,
    int force) {
    return agentos_memoryrov_evolve(engine->rov_handle, force);
}

/* ==================== 健康检查实现 ==================== */

agentos_error_t agentos_memory_health_check(
    agentos_memory_engine_t* engine,
    char** out_json) {

    if (!engine || !out_json) return AGENTOS_EINVAL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;

    cJSON_AddStringToObject(root, "status", "healthy");

    // 可添加更多状态信息，如统计
    char* stats = NULL;
    if (agentos_memoryrov_stats(engine->rov_handle, &stats) == AGENTOS_SUCCESS) {
        cJSON_AddRawToObject(root, "stats", stats);
        free(stats);
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return AGENTOS_ENOMEM;

    *out_json = json;
    return AGENTOS_SUCCESS;
}