/**
 * @file engine.c
 * @brief 记忆引擎实现，通过可拔插提供商接口访问记忆系统
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 架构：engine.c → agentrt_memory_provider_t* → builtin_provider (免费) / MemoryRovol (商业)
 * 不再直接调用 agentrt_memoryrov_* 函数，完全通过 provider 接口解耦。
 */

#include "agentrt_memory.h"
#include "memory.h"
#include "memory_compat.h"
#include "memory_provider.h"
#include "string_compat.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>

struct agentrt_memory_engine {
    agentrt_memory_provider_t *provider;
    agentrt_mutex_t *lock;
    char *config_path;
    int provider_borrowed; /* P3.11-C9: 1=provider 为 borrowed（如 bridge active），destroy 时不销毁 */
};

agentrt_error_t agentrt_memory_create(const char *config_path, agentrt_memory_engine_t **out_engine)
{

    if (!out_engine)
        return AGENTRT_EINVAL;

    AGENTRT_LOG_INFO("MemoryEngine: agentrt_memory_create START (config=%s)",
                     config_path ? config_path : "default");

    agentrt_memory_engine_t *engine =
        (agentrt_memory_engine_t *)AGENTRT_CALLOC(1, sizeof(agentrt_memory_engine_t));
    if (!engine)
        return AGENTRT_ENOMEM;

    if (config_path) {
        engine->config_path = AGENTRT_STRDUP(config_path);
        if (!engine->config_path) {
            AGENTRT_FREE(engine);
            return AGENTRT_ENOMEM;
        }
    }

    engine->lock = agentrt_mutex_create();
    if (!engine->lock) {
        if (engine->config_path)
            AGENTRT_FREE(engine->config_path);
        AGENTRT_FREE(engine);
        return AGENTRT_ENOMEM;
    }

    engine->provider = agentrt_memory_provider_get_active();
    if (!engine->provider) {
        AGENTRT_LOG_INFO("MemoryEngine: no active provider, initializing builtin");
        agentrt_error_t err = agentrt_builtin_memory_provider_init(NULL);
        if (err != AGENTRT_SUCCESS) {
            AGENTRT_LOG_ERROR("MemoryEngine: builtin provider init FAILED (err=%d)", err);
            agentrt_mutex_free(engine->lock);
            if (engine->config_path)
                AGENTRT_FREE(engine->config_path);
            AGENTRT_FREE(engine);
            return err;
        }
        engine->provider = agentrt_memory_provider_get_active();
    }

    if (!engine->provider) {
        AGENTRT_LOG_ERROR("MemoryEngine: no provider available after init");
        agentrt_mutex_free(engine->lock);
        if (engine->config_path)
            AGENTRT_FREE(engine->config_path);
        AGENTRT_FREE(engine);
        return AGENTRT_ENOTINIT;
    }

    if (engine->provider->init) {
        agentrt_error_t err = engine->provider->init(engine->provider, config_path);
        if (err != AGENTRT_SUCCESS) {
            AGENTRT_LOG_ERROR("MemoryEngine: provider init FAILED (err=%d)", err);
            agentrt_mutex_free(engine->lock);
            if (engine->config_path)
                AGENTRT_FREE(engine->config_path);
            AGENTRT_FREE(engine);
            return err;
        }
    }

    *out_engine = engine;
    AGENTRT_LOG_INFO("MemoryEngine: created OK (provider=%s)",
                     engine->provider->name ? engine->provider->name : "builtin");
    return AGENTRT_SUCCESS;
}

void agentrt_memory_destroy(agentrt_memory_engine_t *engine)
{
    if (!engine)
        return;

    AGENTRT_LOG_INFO("MemoryEngine: destroy (provider=%s, borrowed=%d)",
                     engine->provider && engine->provider->name ? engine->provider->name : "none",
                     engine->provider_borrowed);

    agentrt_mutex_lock(engine->lock);
    /* P3.11-C9: borrowed provider（如 bridge active）不销毁，由所有方（bridge）负责 */
    if (!engine->provider_borrowed && engine->provider && engine->provider->destroy) {
        engine->provider->destroy(engine->provider);
    }
    engine->provider = NULL;
    agentrt_mutex_unlock(engine->lock);
    agentrt_mutex_free(engine->lock);
    if (engine->config_path)
        AGENTRT_FREE(engine->config_path);
    AGENTRT_FREE(engine);
}

agentrt_error_t agentrt_memory_set_provider(agentrt_memory_engine_t *engine,
                                            agentrt_memory_provider_t *provider)
{
    if (!engine || !provider)
        return AGENTRT_EINVAL;

    agentrt_mutex_lock(engine->lock);

    /* P3.11-C9: 切换 provider。
     * 旧 provider（通常为 builtin）不销毁——它可能被全局 registry 或 bridge 持有。
     * 新 provider（bridge active）为 borrowed 语义，engine 不负责销毁。
     * 新 provider 应已被调用方 init 过（bridge_create 中已 init），此处不重复 init。 */
    agentrt_memory_provider_t *old = engine->provider;
    int old_borrowed = engine->provider_borrowed;
    engine->provider = provider;
    engine->provider_borrowed = 1; /* bridge provider 为 borrowed */
    agentrt_mutex_unlock(engine->lock);

    AGENTRT_LOG_INFO("P3.11-C9: MemoryEngine provider switched "
                     "(old=%s borrowed=%d → new=%s)",
                     (old && old->name) ? old->name : "(none)", old_borrowed,
                     provider->name ? provider->name : "(unknown)");

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_memory_write(agentrt_memory_engine_t *engine,
                                     const agentrt_memory_record_t *record, char **out_record_id)
{

    if (!engine || !record || !out_record_id)
        return AGENTRT_EINVAL;
    if (!engine->provider || !engine->provider->write_raw)
        return AGENTRT_ENOTINIT;

    AGENTRT_LOG_DEBUG("MemoryEngine: write (type=%d, data_len=%zu, source=%s)",
                      (int)record->memory_record_type,
                      record->memory_record_data_len,
                      record->memory_record_source_agent ? record->memory_record_source_agent : "(none)");

    char metadata[1024];
    int len =
        snprintf(metadata, sizeof(metadata), "{\"source\":\"%s\",\"trace\":\"%s\",\"type\":%d}",
                 record->memory_record_source_agent ? record->memory_record_source_agent : "",
                 record->memory_record_trace_id ? record->memory_record_trace_id : "",
                 (int)record->memory_record_type);

    if (len < 0 || len >= (int)sizeof(metadata)) {
        return AGENTRT_EOVERFLOW;
    }

    agentrt_mutex_lock(engine->lock);
    agentrt_error_t err =
        engine->provider->write_raw(engine->provider, record->memory_record_data,
                                    record->memory_record_data_len, metadata, out_record_id);
    agentrt_mutex_unlock(engine->lock);

    if (err == AGENTRT_SUCCESS && *out_record_id) {
        AGENTRT_LOG_DEBUG("MemoryEngine: write OK (record_id=%s)", *out_record_id);
    } else {
        AGENTRT_LOG_WARN("MemoryEngine: write FAILED (err=%d)", err);
    }

    return err;
}

agentrt_error_t agentrt_memory_query(agentrt_memory_engine_t *engine,
                                     const agentrt_memory_query_t *query,
                                     agentrt_memory_result_ext_t **out_result)
{

    if (!engine || !query || !out_result)
        return AGENTRT_EINVAL;
    if (!engine->provider || !engine->provider->query)
        return AGENTRT_ENOTINIT;

    AGENTRT_LOG_DEBUG("MemoryEngine: query (text_len=%zu, limit=%u)",
                      query->memory_query_text_len, query->memory_query_limit);

    char **results = NULL;
    float *scores = NULL;
    size_t count = 0;

    agentrt_mutex_lock(engine->lock);
    agentrt_error_t err =
        engine->provider->query(engine->provider, query->memory_query_text,
                                query->memory_query_limit, &results, &scores, &count);
    agentrt_mutex_unlock(engine->lock);

    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("MemoryEngine: query FAILED (err=%d)", err);
        return err;
    }

    AGENTRT_LOG_DEBUG("MemoryEngine: query OK (results=%zu)", count);

    agentrt_memory_result_ext_t *res =
        (agentrt_memory_result_ext_t *)AGENTRT_CALLOC(1, sizeof(agentrt_memory_result_ext_t));
    if (!res) {
        agentrt_memory_provider_free_query_results(results, scores, count);
        return AGENTRT_ENOMEM;
    }

    if (count > 0) {
        res->memory_result_items = (agentrt_memory_result_item_t **)AGENTRT_CALLOC(
            count, sizeof(agentrt_memory_result_item_t *));
        if (!res->memory_result_items) {
            agentrt_memory_provider_free_query_results(results, scores, count);
            AGENTRT_FREE(res);
            return AGENTRT_ENOMEM;
        }

        for (size_t i = 0; i < count; i++) {
            res->memory_result_items[i] = (agentrt_memory_result_item_t *)AGENTRT_CALLOC(
                1, sizeof(agentrt_memory_result_item_t));
            if (!res->memory_result_items[i]) {
                for (size_t j = 0; j < i; j++) {
                    res->memory_result_items[j]->memory_result_item_record_id = NULL;
                    AGENTRT_FREE(res->memory_result_items[j]);
                }
                agentrt_memory_provider_free_query_results(results, scores, count);
                AGENTRT_FREE(res->memory_result_items);
                AGENTRT_FREE(res);
                return AGENTRT_ENOMEM;
            }
            res->memory_result_items[i]->memory_result_item_record_id = results[i];
            res->memory_result_items[i]->memory_result_item_score = scores ? scores[i] : 0.0f;

            /* P3.11-C1: 当 include_raw 为真时，通过 get_raw 获取记录内容填充
             * memory_result_item_record。此前 query 仅返回 record_id + score，
             * thinking_chain prepopulate 期望 rec->memory_record_data 但永远拿到 NULL，
             * 导致记忆内容从未注入 context window — 数据流断裂。 */
            if (query->memory_query_include_raw && results[i] && engine->provider->get_raw) {
                void *data = NULL;
                size_t data_len = 0;
                agentrt_mutex_lock(engine->lock);
                agentrt_error_t get_err =
                    engine->provider->get_raw(engine->provider, results[i], &data, &data_len);
                agentrt_mutex_unlock(engine->lock);

                if (get_err == AGENTRT_SUCCESS && data && data_len > 0) {
                    agentrt_memory_record_t *rec =
                        (agentrt_memory_record_t *)AGENTRT_CALLOC(1, sizeof(agentrt_memory_record_t));
                    if (rec) {
                        rec->memory_record_id = AGENTRT_STRDUP(results[i]);
                        rec->memory_record_data = data;
                        rec->memory_record_data_len = data_len;
                        rec->memory_record_type = AGENTRT_MEMTYPE_TEXT;
                        if (!rec->memory_record_id) {
                            AGENTRT_FREE(data);
                            AGENTRT_FREE(rec);
                        } else {
                            res->memory_result_items[i]->memory_result_item_record = rec;
                        }
                    } else {
                        AGENTRT_FREE(data);
                    }
                } else {
                    /* get_raw 失败（记录可能已被删除或 provider 不支持），释放 data */
                    if (data)
                        AGENTRT_FREE(data);
                    AGENTRT_LOG_DEBUG("MemoryEngine: query get_raw failed for "
                                      "record_id=%s (err=%d) — item will have no content",
                                      results[i], (int)get_err);
                }
            }
        }
    }

    res->memory_result_count = count;
    res->memory_result_query_time_ns = 0;

    AGENTRT_FREE(results);
    AGENTRT_FREE(scores);
    *out_result = res;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_memory_get(agentrt_memory_engine_t *engine, const char *record_id,
                                   int include_raw, agentrt_memory_record_t **out_record)
{

    if (!engine || !record_id || !out_record)
        return AGENTRT_EINVAL;
    if (!engine->provider || !engine->provider->get_raw)
        return AGENTRT_ENOTINIT;

    AGENTRT_LOG_DEBUG("MemoryEngine: get (record_id=%s)", record_id);

    void *data = NULL;
    size_t len = 0;

    agentrt_mutex_lock(engine->lock);
    agentrt_error_t err = engine->provider->get_raw(engine->provider, record_id, &data, &len);
    agentrt_mutex_unlock(engine->lock);

    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_DEBUG("MemoryEngine: get NOT FOUND (record_id=%s, err=%d)", record_id, err);
        return err;
    }

    AGENTRT_LOG_DEBUG("MemoryEngine: get OK (record_id=%s, data_len=%zu)", record_id, len);

    agentrt_memory_record_t *rec =
        (agentrt_memory_record_t *)AGENTRT_CALLOC(1, sizeof(agentrt_memory_record_t));
    if (!rec) {
        AGENTRT_FREE(data);
        return AGENTRT_ENOMEM;
    }

    rec->memory_record_id = AGENTRT_STRDUP(record_id);
    if (!rec->memory_record_id) {
        AGENTRT_FREE(rec);
        AGENTRT_FREE(data);
        return AGENTRT_ENOMEM;
    }

    rec->memory_record_data = data;
    rec->memory_record_data_len = len;
    rec->memory_record_type = AGENTRT_MEMTYPE_TEXT;

    *out_record = rec;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_memory_mount(agentrt_memory_engine_t *engine, const char *record_id,
                                     const char *context)
{

    if (!engine || !record_id)
        return AGENTRT_EINVAL;
    if (!engine->provider || !engine->provider->mount)
        return AGENTRT_ENOTINIT;

    agentrt_mutex_lock(engine->lock);
    agentrt_error_t err = engine->provider->mount(engine->provider, record_id, context);
    agentrt_mutex_unlock(engine->lock);

    return err;
}

void agentrt_memory_result_free(agentrt_memory_result_ext_t *result)
{
    if (!result)
        return;
    for (size_t i = 0; i < result->memory_result_count; i++) {
        if (result->memory_result_items[i]) {
            if (result->memory_result_items[i]->memory_result_item_record_id)
                AGENTRT_FREE(result->memory_result_items[i]->memory_result_item_record_id);
            if (result->memory_result_items[i]->memory_result_item_record)
                agentrt_memory_record_free(
                    result->memory_result_items[i]->memory_result_item_record);
            AGENTRT_FREE(result->memory_result_items[i]);
        }
    }
    AGENTRT_FREE(result->memory_result_items);
    AGENTRT_FREE(result);
}

void agentrt_memory_record_free(agentrt_memory_record_t *record)
{
    if (!record)
        return;
    if (record->memory_record_id)
        AGENTRT_FREE(record->memory_record_id);
    if (record->memory_record_source_agent)
        AGENTRT_FREE(record->memory_record_source_agent);
    if (record->memory_record_trace_id)
        AGENTRT_FREE(record->memory_record_trace_id);
    if (record->memory_record_data)
        AGENTRT_FREE(record->memory_record_data);
    AGENTRT_FREE(record);
}

agentrt_error_t agentrt_memory_evolve(agentrt_memory_engine_t *engine, int force)
{
    if (!engine)
        return AGENTRT_EINVAL;
    if (!engine->provider || !engine->provider->evolve)
        return AGENTRT_ENOTINIT;

    AGENTRT_LOG_INFO("MemoryEngine: evolve START (force=%d)", force);

    agentrt_mutex_lock(engine->lock);
    agentrt_error_t err = engine->provider->evolve(engine->provider, force);
    agentrt_mutex_unlock(engine->lock);

    if (err == AGENTRT_SUCCESS) {
        AGENTRT_LOG_INFO("MemoryEngine: evolve OK");
    } else {
        AGENTRT_LOG_WARN("MemoryEngine: evolve FAILED (err=%d)", err);
    }

    return err;
}

agentrt_error_t agentrt_memory_health_check(agentrt_memory_engine_t *engine, char **out_json)
{

    if (!engine || !out_json)
        return AGENTRT_EINVAL;
    if (!engine->provider || !engine->provider->health_check)
        return AGENTRT_ENOTINIT;

    agentrt_mutex_lock(engine->lock);
    agentrt_error_t err = engine->provider->health_check(engine->provider, out_json);
    agentrt_mutex_unlock(engine->lock);

    return err;
}
