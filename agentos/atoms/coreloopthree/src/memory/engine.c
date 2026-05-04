/**
 * @file engine.c
 * @brief 记忆引擎实现，通过可拔插提供商接口访问记忆系统
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 架构：engine.c → agentos_memory_provider_t* → builtin_provider (免费) / MemoryRovol (商业)
 * 不再直接调用 agentos_memoryrov_* 函数，完全通过 provider 接口解耦。
 */

#include "memory.h"
#include "agentos_memory.h"
#include "memory_provider.h"
#include <stdlib.h>

#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

struct agentos_memory_engine {
    agentos_memory_provider_t* provider;
    agentos_mutex_t* lock;
    char* config_path;
};

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

    engine->lock = agentos_mutex_create();
    if (!engine->lock) {
        if (engine->config_path) AGENTOS_FREE(engine->config_path);
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

    engine->provider = agentos_memory_provider_get_active();
    if (!engine->provider) {
        agentos_error_t err = agentos_builtin_memory_provider_init(NULL);
        if (err != AGENTOS_SUCCESS) {
            agentos_mutex_destroy(engine->lock);
            if (engine->config_path) AGENTOS_FREE(engine->config_path);
            AGENTOS_FREE(engine);
            return err;
        }
        engine->provider = agentos_memory_provider_get_active();
    }

    if (!engine->provider) {
        agentos_mutex_destroy(engine->lock);
        if (engine->config_path) AGENTOS_FREE(engine->config_path);
        AGENTOS_FREE(engine);
        return AGENTOS_ENOTINIT;
    }

    if (engine->provider->init) {
        agentos_error_t err = engine->provider->init(engine->provider, config_path);
        if (err != AGENTOS_SUCCESS) {
            agentos_mutex_destroy(engine->lock);
            if (engine->config_path) AGENTOS_FREE(engine->config_path);
            AGENTOS_FREE(engine);
            return err;
        }
    }

    *out_engine = engine;
    return AGENTOS_SUCCESS;
}

void agentos_memory_destroy(agentos_memory_engine_t* engine) {
    if (!engine) return;
    agentos_mutex_lock(engine->lock);
    if (engine->provider && engine->provider->destroy) {
        engine->provider->destroy(engine->provider);
    }
    engine->provider = NULL;
    agentos_mutex_unlock(engine->lock);
    agentos_mutex_destroy(engine->lock);
    if (engine->config_path) AGENTOS_FREE(engine->config_path);
    AGENTOS_FREE(engine);
}

agentos_error_t agentos_memory_write(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    char** out_record_id) {

    if (!engine || !record || !out_record_id) return AGENTOS_EINVAL;
    if (!engine->provider || !engine->provider->write_raw) return AGENTOS_ENOTINIT;

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
    agentos_error_t err = engine->provider->write_raw(
        engine->provider,
        record->memory_record_data,
        record->memory_record_data_len,
        metadata,
        out_record_id);
    agentos_mutex_unlock(engine->lock);

    return err;
}

agentos_error_t agentos_memory_query(
    agentos_memory_engine_t* engine,
    const agentos_memory_query_t* query,
    agentos_memory_result_ext_t** out_result) {

    if (!engine || !query || !out_result) return AGENTOS_EINVAL;
    if (!engine->provider || !engine->provider->query) return AGENTOS_ENOTINIT;

    char** results = NULL;
    float* scores = NULL;
    size_t count = 0;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = engine->provider->query(
        engine->provider,
        query->memory_query_text,
        query->memory_query_limit,
        &results,
        &scores,
        &count);
    agentos_mutex_unlock(engine->lock);

    if (err != AGENTOS_SUCCESS) return err;

    agentos_memory_result_ext_t* res = (agentos_memory_result_ext_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_result_ext_t));
    if (!res) {
        agentos_memory_provider_free_query_results(results, scores, count);
        return AGENTOS_ENOMEM;
    }

    if (count > 0) {
        res->memory_result_items = (agentos_memory_result_item_t**)AGENTOS_CALLOC(count, sizeof(agentos_memory_result_item_t*));
        if (!res->memory_result_items) {
            agentos_memory_provider_free_query_results(results, scores, count);
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
                agentos_memory_provider_free_query_results(results, scores, count);
                AGENTOS_FREE(res->memory_result_items);
                AGENTOS_FREE(res);
                return AGENTOS_ENOMEM;
            }
            res->memory_result_items[i]->memory_result_item_record_id = results[i];
            res->memory_result_items[i]->memory_result_item_score = scores ? scores[i] : 0.0f;
        }
    }

    res->memory_result_count = count;
    res->memory_result_query_time_ns = 0;

    AGENTOS_FREE(results);
    AGENTOS_FREE(scores);
    *out_result = res;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memory_get(
    agentos_memory_engine_t* engine,
    const char* record_id,
    int include_raw,
    agentos_memory_record_t** out_record) {

    if (!engine || !record_id || !out_record) return AGENTOS_EINVAL;
    if (!engine->provider || !engine->provider->get_raw) return AGENTOS_ENOTINIT;

    void* data = NULL;
    size_t len = 0;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = engine->provider->get_raw(engine->provider, record_id, &data, &len);
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
    rec->memory_record_type = AGENTOS_MEMTYPE_TEXT;

    *out_record = rec;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memory_mount(
    agentos_memory_engine_t* engine,
    const char* record_id,
    const char* context) {

    if (!engine || !record_id) return AGENTOS_EINVAL;
    if (!engine->provider || !engine->provider->mount) return AGENTOS_ENOTINIT;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = engine->provider->mount(engine->provider, record_id, context);
    agentos_mutex_unlock(engine->lock);

    return err;
}

void agentos_memory_result_free(agentos_memory_result_ext_t* result) {
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

void agentos_memory_record_free(agentos_memory_record_t* record) {
    if (!record) return;
    if (record->memory_record_id) AGENTOS_FREE(record->memory_record_id);
    if (record->memory_record_source_agent) AGENTOS_FREE(record->memory_record_source_agent);
    if (record->memory_record_trace_id) AGENTOS_FREE(record->memory_record_trace_id);
    if (record->memory_record_data) AGENTOS_FREE(record->memory_record_data);
    AGENTOS_FREE(record);
}

agentos_error_t agentos_memory_evolve(
    agentos_memory_engine_t* engine,
    int force) {
    if (!engine) return AGENTOS_EINVAL;
    if (!engine->provider || !engine->provider->evolve) return AGENTOS_ENOTINIT;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = engine->provider->evolve(engine->provider, force);
    agentos_mutex_unlock(engine->lock);
    return err;
}

agentos_error_t agentos_memory_health_check(
    agentos_memory_engine_t* engine,
    char** out_json) {

    if (!engine || !out_json) return AGENTOS_EINVAL;
    if (!engine->provider || !engine->provider->health_check) return AGENTOS_ENOTINIT;

    agentos_mutex_lock(engine->lock);
    agentos_error_t err = engine->provider->health_check(engine->provider, out_json);
    agentos_mutex_unlock(engine->lock);

    return err;
}
