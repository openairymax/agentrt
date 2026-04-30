/**
 * @file builtin_provider.c
 * @brief AgentOS 内置免费内存提供商实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 agentos_memory_provider_t 接口，提供 L1+L2 基础功能。
 * 无 MemoryRovol / FAISS / SQLite 依赖。
 * 启动日志显示 "using built-in provider (free)"。
 */

#include "memory_provider.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct builtin_provider_impl {
    void* storage;
    void* index;
    void* retrieval;
    agentos_memory_stats_t stats;
} builtin_provider_impl_t;

static agentos_error_t builtin_init(
    agentos_memory_provider_t* provider, const char* config_path) {

    if (!provider || !provider->impl) return AGENTOS_EINVAL;

    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    const char* path = config_path ? config_path : "./data/agentos/memory";
    impl->storage = builtin_storage_create(path);
    if (!impl->storage) return AGENTOS_ENOMEM;

    impl->index = builtin_index_create();
    if (!impl->index) {
        builtin_storage_destroy(impl->storage);
        return AGENTOS_ENOMEM;
    }

    impl->retrieval = builtin_retrieval_create();
    if (!impl->retrieval) {
        builtin_storage_destroy(impl->storage);
        builtin_index_destroy(impl->index);
        return AGENTOS_ENOMEM;
    }

    memset(&impl->stats, 0, sizeof(impl->stats));
    snprintf(impl->stats.provider_name, sizeof(impl->stats.provider_name), "builtin");
    snprintf(impl->stats.provider_version, sizeof(impl->stats.provider_version), "1.0.0");

    printf("[AgentOS] using built-in provider (free) - storage: %s\n", path);

    return AGENTOS_SUCCESS;
}

static void builtin_destroy(agentos_memory_provider_t* provider) {
    if (!provider || !provider->impl) return;

    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;
    if (impl->storage) builtin_storage_destroy(impl->storage);
    if (impl->index) builtin_index_destroy(impl->index);
    if (impl->retrieval) builtin_retrieval_destroy(impl->retrieval);
    free(impl);
    provider->impl = NULL;
}

static agentos_error_t builtin_write_raw(
    agentos_memory_provider_t* provider,
    const void* data, size_t len,
    const char* metadata_json,
    char** out_record_id) {

    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    agentos_error_t err = builtin_storage_write(impl->storage, data, len, metadata_json, out_record_id);
    if (err != AGENTOS_SUCCESS) return err;

    builtin_index_add(impl->index, *out_record_id, metadata_json ? metadata_json : "", len);

    impl->stats.total_records++;
    impl->stats.l1_count++;
    impl->stats.total_bytes += len;

    return AGENTOS_SUCCESS;
}

static agentos_error_t builtin_get_raw(
    agentos_memory_provider_t* provider,
    const char* record_id,
    void** out_data, size_t* out_len) {

    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;
    return builtin_storage_get(impl->storage, record_id, out_data, out_len);
}

static agentos_error_t builtin_delete_raw(
    agentos_memory_provider_t* provider,
    const char* record_id) {

    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    agentos_error_t err = builtin_storage_delete(impl->storage, record_id);
    if (err == AGENTOS_SUCCESS) {
        builtin_index_remove(impl->index, record_id);
        if (impl->stats.total_records > 0) impl->stats.total_records--;
        if (impl->stats.l1_count > 0) impl->stats.l1_count--;
    }
    return err;
}

static agentos_error_t builtin_query(
    agentos_memory_provider_t* provider,
    const char* query_text,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {

    if (!provider || !provider->impl || !query_text) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;
    return builtin_index_search(impl->index, query_text, limit, out_record_ids, out_scores, out_count);
}

static agentos_error_t builtin_retrieve_fn(
    agentos_memory_provider_t* provider,
    const char* query_text,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {

    if (!provider || !provider->impl || !query_text) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;
    return builtin_index_search(impl->index, query_text, limit, out_record_ids, out_scores, out_count);
}

static agentos_error_t builtin_evolve(
    agentos_memory_provider_t* provider,
    int force) {
    (void)force;
    if (!provider) return AGENTOS_EINVAL;
    return AGENTOS_SUCCESS;
}

static agentos_error_t builtin_forget(
    agentos_memory_provider_t* provider) {
    if (!provider) return AGENTOS_EINVAL;
    return AGENTOS_SUCCESS;
}

static agentos_error_t builtin_stats(
    agentos_memory_provider_t* provider,
    agentos_memory_stats_t* out_stats) {

    if (!provider || !provider->impl || !out_stats) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    impl->stats.l2_indexed = builtin_index_total_docs(impl->index);
    memcpy(out_stats, &impl->stats, sizeof(agentos_memory_stats_t));
    return AGENTOS_SUCCESS;
}

static agentos_error_t builtin_mount(
    agentos_memory_provider_t* provider,
    const char* record_id,
    const char* context) {
    (void)record_id; (void)context;
    if (!provider) return AGENTOS_EINVAL;
    return AGENTOS_SUCCESS;
}

/* ========== 全局提供商注册 ========== */

static agentos_memory_provider_t* g_active_provider = NULL;

agentos_error_t agentos_memory_provider_register(agentos_memory_provider_t* provider) {
    if (!provider) return AGENTOS_EINVAL;
    g_active_provider = provider;
    return AGENTOS_SUCCESS;
}

agentos_memory_provider_t* agentos_memory_provider_get_active(void) {
    return g_active_provider;
}

agentos_error_t agentos_memory_provider_set_active(agentos_memory_provider_t* provider) {
    if (!provider) return AGENTOS_EINVAL;
    g_active_provider = provider;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_builtin_memory_provider_init(const char* storage_path) {
    static agentos_memory_provider_t provider;
    static builtin_provider_impl_t impl;

    memset(&provider, 0, sizeof(provider));
    memset(&impl, 0, sizeof(impl));

    provider.name = "builtin";
    provider.version = "1.0.0";
    provider.impl = &impl;

    provider.capabilities.l1_raw = 1;
    provider.capabilities.l2_feature = 1;
    provider.capabilities.l3_structure = 0;
    provider.capabilities.l4_pattern = 0;
    provider.capabilities.forgetting = 0;
    provider.capabilities.attractor = 0;
    provider.capabilities.persistence = 0;
    provider.capabilities.faiss = 0;
    provider.capabilities.async_ops = 0;
    provider.capabilities.llm_integration = 0;

    provider.init = builtin_init;
    provider.destroy = builtin_destroy;
    provider.write_raw = builtin_write_raw;
    provider.get_raw = builtin_get_raw;
    provider.delete_raw = builtin_delete_raw;
    provider.query = builtin_query;
    provider.retrieve = builtin_retrieve_fn;
    provider.evolve = builtin_evolve;
    provider.forget = builtin_forget;
    provider.stats = builtin_stats;
    provider.mount = builtin_mount;

    agentos_error_t err = provider.init(&provider, storage_path);
    if (err != AGENTOS_SUCCESS) return err;

    return agentos_memory_provider_register(&provider);
}

void agentos_memory_query_result_free(agentos_memory_query_result_t* result) {
    if (!result) return;
    if (result->record_ids) {
        for (size_t i = 0; i < result->count; i++) {
            if (result->record_ids[i]) free(result->record_ids[i]);
        }
        free(result->record_ids);
    }
    if (result->scores) free(result->scores);
    result->record_ids = NULL;
    result->scores = NULL;
    result->count = 0;
}

agentos_memory_provider_t* agentos_builtin_provider_create(void) {
    agentos_memory_provider_t* provider = (agentos_memory_provider_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_provider_t));
    if (!provider) return NULL;

    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)AGENTOS_CALLOC(1, sizeof(builtin_provider_impl_t));
    if (!impl) {
        AGENTOS_FREE(provider);
        return NULL;
    }

    provider->name = "builtin";
    provider->version = "1.0.0";
    provider->impl = impl;

    provider->capabilities.l1_raw = 1;
    provider->capabilities.l2_feature = 1;
    provider->capabilities.l3_structure = 0;
    provider->capabilities.l4_pattern = 0;
    provider->capabilities.forgetting = 0;
    provider->capabilities.attractor = 0;
    provider->capabilities.persistence = 1;
    provider->capabilities.faiss = 0;
    provider->capabilities.async_ops = 0;
    provider->capabilities.llm_integration = 0;

    provider->init = builtin_init;
    provider->destroy = builtin_destroy;
    provider->write_raw = builtin_write_raw;
    provider->get_raw = builtin_get_raw;
    provider->delete_raw = builtin_delete_raw;
    provider->query = builtin_query;
    provider->retrieve = builtin_retrieve_fn;
    provider->evolve = builtin_evolve;
    provider->forget = builtin_forget;
    provider->stats = builtin_stats;
    provider->mount = builtin_mount;

    return provider;
}

void agentos_memory_provider_free_query_results(
    char** record_ids, float* scores, size_t count) {
    if (!record_ids && !scores) return;
    if (record_ids) {
        for (size_t i = 0; i < count; i++) {
            if (record_ids[i]) AGENTOS_FREE(record_ids[i]);
        }
        AGENTOS_FREE(record_ids);
    }
    if (scores) AGENTOS_FREE(scores);
}
