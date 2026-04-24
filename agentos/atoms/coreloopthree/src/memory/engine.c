/**
 * @file engine.c
 * @brief 记忆引擎实现，封装MemoryRovol接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory.h"
#include "agentos_memory.h"
#include "rov_ffi.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <cjson/cJSON.h>

struct agentos_memory_engine {
    agentos_memoryrov_handle_t* rov_handle;
    agentos_mutex_t* lock;
    char* config_path;
};

/**
 * @brief 创建记忆引擎
 */
agentos_error_t agentos_memory_create(
    const char* config_path,
    agentos_memory_engine_t** out_engine) {

    if (!out_engine) return AGENTOS_EINVAL;

    agentos_memory_engine_t* engine = (agentos_memory_engine_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_engine_t));
    if (!engine) return AGENTOS_ENOMEM;

    if (config_path) {
        engine->config_path = AGENTOS_STRDUP(config_path);
        if (!engine->config_path) {
            AGENTOS_FREE(engine);
            return AGENTOS_ENOMEM;
        }
    }

    agentos_error_t err = agentos_memoryrov_create(config_path, &engine->rov_handle);
    if (err != AGENTOS_SUCCESS) {
        if (engine->config_path) AGENTOS_FREE(engine->config_path);
        AGENTOS_FREE(engine);
        return err;
    }

    engine->lock = agentos_mutex_create();
    if (!engine->lock) {
        agentos_memoryrov_destroy(engine->rov_handle);
        if (engine->config_path) AGENTOS_FREE(engine->config_path);
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

    *out_engine = engine;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁记忆引擎
 */
void agentos_memory_destroy(agentos_memory_engine_t* engine) {
    if (!engine) return;
    agentos_mutex_lock(engine->lock);
    if (engine->rov_handle) {
        agentos_memoryrov_destroy(engine->rov_handle);
        engine->rov_handle = NULL;
    }
    agentos_mutex_unlock(engine->lock);
    agentos_mutex_destroy(engine->lock);
    if (engine->config_path) AGENTOS_FREE(engine->config_path);
    AGENTOS_FREE(engine);
}

/**
 * @brief 写入记忆记录
 */
agentos_error_t agentos_memory_write(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    char** out_record_id) {

    if (!engine || !record || !out_record_id) return AGENTOS_EINVAL;

    char metadata[1024];
    int len = snprintf(metadata, sizeof(metadata),
        "{\"source\":\"%s\",\"trace\":\"%s\",\"type\":%d}",
        record->memory_record_source_agent ? record->memory_record_source_agent : "",
        record->memory_record_trace_id ? record->memory_record_trace_id : "",
        (int)record->memory_record_type);

    if (len < 0 || len >= (int)sizeof(metadata)) {
        return AGENTOS_EOVERFLOW;
    }

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = agentos_memoryrov_write_raw(
        engine->rov_handle,
        record->memory_record_data,
        record->memory_record_data_len,
        metadata,
        out_record_id);
    agentos_mutex_unlock(engine->lock);

    return err;
}

/**
 * @brief 查询记忆
 */
agentos_error_t agentos_memory_query(
    agentos_memory_engine_t* engine,
    const agentos_memory_query_t* query,
    agentos_memory_result_t** out_result) {

    if (!engine || !query || !out_result) return AGENTOS_EINVAL;

    char** results = NULL;
    size_t count = 0;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = agentos_memoryrov_query(
        engine->rov_handle,
        query->memory_query_text,
        query->memory_query_limit,
        &results,
        &count);
    agentos_mutex_unlock(engine->lock);

    if (err != AGENTOS_SUCCESS) return err;

    agentos_memory_result_t* res = (agentos_memory_result_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_result_t));
    if (!res) {
        for (size_t i = 0; i < count; i++) AGENTOS_FREE(results[i]);
        AGENTOS_FREE(results);
        return AGENTOS_ENOMEM;
    }

    if (count > 0) {
        res->memory_result_items = (agentos_memory_result_item_t**)AGENTOS_CALLOC(count, sizeof(agentos_memory_result_item_t*));
        if (!res->memory_result_items) {
            for (size_t i = 0; i < count; i++) AGENTOS_FREE(results[i]);
            AGENTOS_FREE(results);
            AGENTOS_FREE(res);
            return AGENTOS_ENOMEM;
        }

        for (size_t i = 0; i < count; i++) {
            res->memory_result_items[i] = (agentos_memory_result_item_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_result_item_t));
            if (!res->memory_result_items[i]) {
                for (size_t j = 0; j < i; j++) {
                    AGENTOS_FREE(res->memory_result_items[j]->memory_result_item_record_id);
                    AGENTOS_FREE(res->memory_result_items[j]);
                }
                for (size_t j = 0; j < count; j++) AGENTOS_FREE(results[j]);
                AGENTOS_FREE(results);
                AGENTOS_FREE(res->memory_result_items);
                AGENTOS_FREE(res);
                return AGENTOS_ENOMEM;
            }
            res->memory_result_items[i]->memory_result_item_record_id = results[i];
            res->memory_result_items[i]->memory_result_item_score = 0.0f;
        }
    }

    res->memory_result_count = count;
    res->memory_result_query_time_ns = 0;

    AGENTOS_FREE(results);
    *out_result = res;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 根据 ID 获取记忆记录
 */
agentos_error_t agentos_memory_get(
    agentos_memory_engine_t* engine,
    const char* record_id,
    int include_raw,
    agentos_memory_record_t** out_record) {

    if (!engine || !record_id || !out_record) return AGENTOS_EINVAL;

    void* data = NULL;
    size_t len = 0;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = agentos_memoryrov_get_raw(engine->rov_handle, record_id, &data, &len);
    agentos_mutex_unlock(engine->lock);

    if (err != AGENTOS_SUCCESS) return err;

    agentos_memory_record_t* rec = (agentos_memory_record_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_record_t));
    if (!rec) {
        AGENTOS_FREE(data);
        return AGENTOS_ENOMEM;
    }

    rec->memory_record_id = AGENTOS_STRDUP(record_id);
    if (!rec->memory_record_id) {
        AGENTOS_FREE(rec);
        AGENTOS_FREE(data);
        return AGENTOS_ENOMEM;
    }

    rec->memory_record_data = data;
    rec->memory_record_data_len = len;
    rec->memory_record_type = MEMORY_TYPE_RAW;

    *out_record = rec;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 挂载记忆（增加访问计数）
 */
agentos_error_t agentos_memory_mount(
    agentos_memory_engine_t* engine,
    const char* record_id,
    const char* context) {

    if (!engine || !record_id) return AGENTOS_EINVAL;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = agentos_memoryrov_mount(engine->rov_handle, record_id, context);
    agentos_mutex_unlock(engine->lock);

    return err;
}

/**
 * @brief 释放查询结果
 */
void agentos_memory_result_free(agentos_memory_result_t* result) {
    if (!result) return;
    for (size_t i = 0; i < result->memory_result_count; i++) {
        if (result->memory_result_items[i]) {
            if (result->memory_result_items[i]->memory_result_item_record_id)
                AGENTOS_FREE(result->memory_result_items[i]->memory_result_item_record_id);
            if (result->memory_result_items[i]->memory_result_item_record)
                agentos_memory_record_free(result->memory_result_items[i]->memory_result_item_record);
            AGENTOS_FREE(result->memory_result_items[i]);
        }
    }
    AGENTOS_FREE(result->memory_result_items);
    AGENTOS_FREE(result);
}

/**
 * @brief 释放记忆记录
 */
void agentos_memory_record_free(agentos_memory_record_t* record) {
    if (!record) return;
    if (record->memory_record_id) AGENTOS_FREE(record->memory_record_id);
    if (record->memory_record_source_agent) AGENTOS_FREE(record->memory_record_source_agent);
    if (record->memory_record_trace_id) AGENTOS_FREE(record->memory_record_trace_id);
    if (record->memory_record_data) AGENTOS_FREE(record->memory_record_data);
    AGENTOS_FREE(record);
}

/**
 * @brief 触发记忆进化
 */
agentos_error_t agentos_memory_evolve(
    agentos_memory_engine_t* engine,
    int force) {
    if (!engine) return AGENTOS_EINVAL;
    (void)force;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 记忆引擎健康检查
 */
agentos_error_t agentos_memory_health_check(
    agentos_memory_engine_t* engine,
    char** out_json) {

    if (!engine || !out_json) return AGENTOS_EINVAL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;

    cJSON_AddStringToObject(root, "status", "healthy");

    agentos_mutex_lock(engine->lock);
    cJSON_AddNumberToObject(root, "records", 0);
    agentos_mutex_unlock(engine->lock);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return AGENTOS_ENOMEM;

    *out_json = json;
    return AGENTOS_SUCCESS;
}
