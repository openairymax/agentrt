/**
 * @file memoryrovol.c
 * @brief MemoryRovol 系统主接口实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memoryrovol.h"
#include "layer1_raw.h"
#include "layer2_feature.h"
#include "retrieval.h"
#include "forgetting.h"
#include "config.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief MemoryRovol 系统句柄结构体
 */
struct agentos_memoryrov_handle {
    agentos_layer1_raw_t* l1_raw;
    agentos_layer2_feature_t* l2_feature;
    agentos_memoryrov_config_t config;
    agentos_mutex_t* lock;
    int initialized;
};

/* ==================== 公共接口实现 ==================== */

agentos_error_t agentos_memoryrov_init(
    const agentos_memoryrov_config_t* config,
    agentos_memoryrov_handle_t** out_handle)
{
    if (!out_handle) return AGENTOS_EINVAL;
    
    agentos_memoryrov_handle_t* handle = 
        (agentos_memoryrov_handle_t*)calloc(1, sizeof(agentos_memoryrov_handle_t));
    if (!handle) return AGENTOS_ENOMEM;
    
    if (config) {
        memcpy(&handle->config, config, sizeof(agentos_memoryrov_config_t));
    } else {
        memset(&handle->config, 0, sizeof(agentos_memoryrov_config_t));
        char* default_path = strdup("./memory_data");
        if (!default_path) {
            free(handle);
            return AGENTOS_ENOMEM;
        }
        handle->config.memoryrov_config_l1_path = default_path;
        handle->config.memoryrov_config_async_workers = 2;
        handle->config.memoryrov_config_queue_size = 1000;
    }
    
    handle->lock = agentos_mutex_create();
    if (!handle->lock) {
        free(handle);
        return AGENTOS_ENOMEM;
    }
    
    agentos_error_t err = agentos_layer1_raw_create_async(
        handle->config.memoryrov_config_l1_path ? handle->config.memoryrov_config_l1_path : "./memory_data",
        handle->config.memoryrov_config_queue_size > 0 ? handle->config.memoryrov_config_queue_size : 1000,
        handle->config.memoryrov_config_async_workers > 0 ? handle->config.memoryrov_config_async_workers : 2,
        &handle->l1_raw);
    
    if (err != AGENTOS_SUCCESS) {
        agentos_mutex_destroy(handle->lock);
        free(handle);
        return err;
    }
    
    handle->initialized = 1;
    *out_handle = handle;
    return AGENTOS_SUCCESS;
}

void agentos_memoryrov_cleanup(agentos_memoryrov_handle_t* handle)
{
    if (!handle) return;
    
    if (handle->l1_raw) {
        agentos_layer1_raw_flush(handle->l1_raw, 5000);
        agentos_layer1_raw_destroy(handle->l1_raw);
    }
    
    if (handle->lock) {
        agentos_mutex_destroy(handle->lock);
    }
    
    if (handle->config.memoryrov_config_l1_path) {
        free((void*)handle->config.memoryrov_config_l1_path);
    }
    
    free(handle);
}

agentos_error_t agentos_memoryrov_evolve(
    agentos_memoryrov_handle_t* handle,
    int force)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    (void)force;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memoryrov_stats(
    agentos_memoryrov_handle_t* handle,
    char** out_stats)
{
    if (!handle || !handle->initialized || !out_stats) return AGENTOS_EINVAL;
    
    const char* stats_template = "{\"status\":\"active\",\"l1\":\"enabled\"}";
    *out_stats = strdup(stats_template);
    return *out_stats ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

/* ==================== 高层封装接口实现 ==================== */

agentos_error_t agentos_memoryrov_write_raw(
    agentos_memoryrov_handle_t* handle,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!data || len == 0 || !out_record_id) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    agentos_error_t err = agentos_layer1_raw_write(handle->l1_raw, data, len, metadata, out_record_id);
    agentos_mutex_unlock(handle->lock);
    
    return err;
}

agentos_error_t agentos_memoryrov_get_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    void** out_data,
    size_t* out_len)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!record_id || !out_data || !out_len) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    agentos_error_t err = agentos_layer1_raw_read(handle->l1_raw, record_id, out_data, out_len);
    agentos_mutex_unlock(handle->lock);
    
    return err;
}

agentos_error_t agentos_memoryrov_delete_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!record_id) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    agentos_error_t err = agentos_layer1_raw_delete(handle->l1_raw, record_id);
    agentos_mutex_unlock(handle->lock);
    
    return err;
}

agentos_error_t agentos_memoryrov_query(
    agentos_memoryrov_handle_t* handle,
    const char* query,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!query || !out_record_ids || !out_scores || !out_count) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    
    char** ids = NULL;
    size_t id_count = 0;
    agentos_error_t err = agentos_layer1_raw_list_ids(handle->l1_raw, &ids, &id_count);
    
    if (err != AGENTOS_SUCCESS) {
        agentos_mutex_unlock(handle->lock);
        return err;
    }
    
    size_t result_count = (limit > 0 && id_count > limit) ? limit : id_count;
    
    char** result_ids = (char**)calloc(result_count, sizeof(char*));
    float* result_scores = (float*)calloc(result_count, sizeof(float));
    
    if (!result_ids || !result_scores) {
        if (result_ids) free(result_ids);
        if (result_scores) free(result_scores);
        agentos_free_string_array(ids, id_count);
        agentos_mutex_unlock(handle->lock);
        return AGENTOS_ENOMEM;
    }
    
    for (size_t i = 0; i < result_count; i++) {
        result_ids[i] = strdup(ids[i]);
        if (!result_ids[i]) {
            for (size_t j = 0; j < i; j++) free(result_ids[j]);
            free(result_ids);
            free(result_scores);
            agentos_free_string_array(ids, id_count);
            agentos_mutex_unlock(handle->lock);
            return AGENTOS_ENOMEM;
        }
        result_scores[i] = 1.0f;
    }
    
    agentos_free_string_array(ids, id_count);
    
    *out_record_ids = result_ids;
    *out_scores = result_scores;
    *out_count = result_count;
    
    agentos_mutex_unlock(handle->lock);
    return AGENTOS_SUCCESS;
}
