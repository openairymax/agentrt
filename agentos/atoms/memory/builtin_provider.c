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
#include <time.h>

typedef struct builtin_storage builtin_storage_t;
typedef struct builtin_index builtin_index_t;
typedef struct builtin_retrieval builtin_retrieval_t;

extern builtin_storage_t* builtin_storage_create(const char* base_path);
extern void builtin_storage_destroy(builtin_storage_t* st);
extern agentos_error_t builtin_storage_write(builtin_storage_t* st, const void* data, size_t len, const char* metadata_json, char** out_record_id);
extern agentos_error_t builtin_storage_get(const builtin_storage_t* st, const char* record_id, void** out_data, size_t* out_len);
extern agentos_error_t builtin_storage_delete(builtin_storage_t* st, const char* record_id);
extern size_t builtin_storage_count(const builtin_storage_t* st);
extern const char* builtin_storage_get_record_id(const builtin_storage_t* st, size_t index);
extern time_t builtin_storage_get_updated_at(const builtin_storage_t* st, size_t index);

extern builtin_index_t* builtin_index_create(void);
extern void builtin_index_destroy(builtin_index_t* idx);
extern void builtin_index_add(builtin_index_t* idx, const char* record_id, const char* metadata_json, size_t data_len);
extern void builtin_index_remove(builtin_index_t* idx, const char* record_id);
extern agentos_error_t builtin_index_search(const builtin_index_t* idx, const char* query_text, uint32_t limit, char*** out_record_ids, float** out_scores, size_t* out_count);
extern size_t builtin_index_total_docs(const builtin_index_t* idx);
extern void builtin_index_compact(builtin_index_t* idx);

extern builtin_retrieval_t* builtin_retrieval_create(void);
extern void builtin_retrieval_destroy(builtin_retrieval_t* ret);
extern agentos_error_t builtin_retrieval_find(const builtin_retrieval_t* ret, const char* query, uint32_t limit, char*** out_ids, float** out_scores, size_t* out_count);

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
        impl->storage = NULL;
        return AGENTOS_ENOMEM;
    }

    impl->retrieval = builtin_retrieval_create();
    if (!impl->retrieval) {
        builtin_storage_destroy(impl->storage);
        builtin_index_destroy(impl->index);
        impl->storage = NULL;
        impl->index = NULL;
        return AGENTOS_ENOMEM;
    }

    memset(&impl->stats, 0, sizeof(impl->stats));
    snprintf(impl->stats.provider_name, sizeof(impl->stats.provider_name), "builtin");
    snprintf(impl->stats.provider_version, sizeof(impl->stats.provider_version), "0.0.5");

    printf("[AgentOS] using built-in provider (free) - storage: %s\n", path);

    return AGENTOS_SUCCESS;
}

static void builtin_destroy(agentos_memory_provider_t* provider) {
    if (!provider || !provider->impl) return;

    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;
    if (impl->storage) builtin_storage_destroy(impl->storage);
    if (impl->index) builtin_index_destroy(impl->index);
    if (impl->retrieval) builtin_retrieval_destroy(impl->retrieval);
    impl->storage = NULL;
    impl->index = NULL;
    impl->retrieval = NULL;
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
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    if (impl->index) {
        builtin_index_compact(impl->index);
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t builtin_forget(
    agentos_memory_provider_t* provider) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    size_t total = builtin_storage_count(impl->storage);
    if (total == 0) return AGENTOS_SUCCESS;

    double forget_ratio = 0.1;
    size_t forget_count = (size_t)(total * forget_ratio);
    if (forget_count < 1) forget_count = 1;
    if (forget_count > total) forget_count = total;

    typedef struct {
        size_t index;
        time_t updated_at;
    } record_age_t;

    record_age_t* ages = (record_age_t*)malloc(total * sizeof(record_age_t));
    if (!ages) return AGENTOS_ENOMEM;

    for (size_t i = 0; i < total; i++) {
        ages[i].index = i;
        ages[i].updated_at = builtin_storage_get_updated_at(impl->storage, i);
    }

    for (size_t i = 0; i < forget_count && i < total; i++) {
        size_t min_idx = i;
        for (size_t j = i + 1; j < total; j++) {
            if (ages[j].updated_at < ages[min_idx].updated_at) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            record_age_t tmp = ages[i];
            ages[i] = ages[min_idx];
            ages[min_idx] = tmp;
        }
    }

    size_t actually_forgotten = 0;
    for (size_t i = 0; i < forget_count; i++) {
        const char* rid_ptr = builtin_storage_get_record_id(impl->storage, ages[i].index);
        if (!rid_ptr) continue;

        char record_id[64];
        snprintf(record_id, sizeof(record_id), "%s", rid_ptr);

        agentos_error_t del_err = builtin_storage_delete(impl->storage, record_id);
        if (del_err == AGENTOS_SUCCESS) {
            builtin_index_remove(impl->index, record_id);
            actually_forgotten++;
        }
    }

    free(ages);

    impl->stats.total_records = builtin_storage_count(impl->storage);
    impl->stats.l1_count = impl->stats.total_records;
    impl->stats.l2_indexed = builtin_index_total_docs(impl->index);

    (void)actually_forgotten;
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
    if (!provider || !provider->impl || !record_id) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    if (!impl->storage) return AGENTOS_ENOTINIT;

    void* data = NULL;
    size_t len = 0;
    agentos_error_t err = builtin_storage_get(impl->storage, record_id, &data, &len);
    if (err != AGENTOS_SUCCESS) return err;
    if (data) free(data);

    return AGENTOS_SUCCESS;
}

static agentos_error_t builtin_health_check(
    agentos_memory_provider_t* provider,
    char** out_json) {
    if (!provider || !out_json) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"status\":\"%s\",\"provider\":\"%s\",\"records\":%llu}",
        impl && impl->storage ? "healthy" : "degraded",
        provider->name ? provider->name : "builtin",
        (unsigned long long)(impl ? impl->stats.total_records : 0));

    *out_json = strdup(buf);
    return *out_json ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

static agentos_error_t builtin_add_memory(
    agentos_memory_provider_t* provider,
    const char* content,
    size_t content_len) {
    if (!provider || !provider->impl || !content) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;

    char* record_id = NULL;
    agentos_error_t err = builtin_storage_write(impl->storage, content, content_len, NULL, &record_id);
    if (err != AGENTOS_SUCCESS) return err;

    builtin_index_add(impl->index, record_id, "", content_len);
    impl->stats.total_records++;
    impl->stats.l1_count++;
    impl->stats.total_bytes += content_len;

    free(record_id);
    return AGENTOS_SUCCESS;
}

static agentos_error_t builtin_retrieve(
    agentos_memory_provider_t* provider,
    const char* query,
    size_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {
    if (!provider || !provider->impl || !query) return AGENTOS_EINVAL;
    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)provider->impl;
    return builtin_index_search(impl->index, query, (uint32_t)limit, out_record_ids, out_scores, out_count);
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

void agentos_memory_provider_unregister(void) {
    if (g_active_provider) {
        if (g_active_provider->destroy) {
            g_active_provider->destroy(g_active_provider);
        }
        free(g_active_provider);
        g_active_provider = NULL;
    }
}

static void setup_provider_vtable(agentos_memory_provider_t* provider) {
    provider->init = builtin_init;
    provider->destroy = builtin_destroy;
    provider->write_raw = builtin_write_raw;
    provider->get_raw = builtin_get_raw;
    provider->delete_raw = builtin_delete_raw;
    provider->query = builtin_query;
    provider->retrieve = builtin_retrieve;
    provider->evolve = builtin_evolve;
    provider->forget = builtin_forget;
    provider->stats = builtin_stats;
    provider->mount = builtin_mount;
    provider->health_check = builtin_health_check;
    provider->add_memory = builtin_add_memory;
}

static void setup_provider_capabilities(agentos_memory_provider_t* provider) {
    provider->capabilities.l1_raw = 1;
    provider->capabilities.l2_feature = 1;
    provider->capabilities.l3_structure = 0;
    provider->capabilities.l4_pattern = 0;
    provider->capabilities.forgetting = 1;
    provider->capabilities.attractor = 0;
    provider->capabilities.persistence = 1;
    provider->capabilities.faiss = 0;
    provider->capabilities.async_ops = 0;
    provider->capabilities.llm_integration = 0;
}

agentos_error_t agentos_builtin_memory_provider_init(const char* storage_path) {
    if (g_active_provider) {
        return AGENTOS_SUCCESS;
    }

    agentos_memory_provider_t* provider = (agentos_memory_provider_t*)calloc(1, sizeof(agentos_memory_provider_t));
    if (!provider) return AGENTOS_ENOMEM;

    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)calloc(1, sizeof(builtin_provider_impl_t));
    if (!impl) {
        free(provider);
        return AGENTOS_ENOMEM;
    }

    provider->name = "builtin";
    provider->version = "0.0.5";
    provider->impl = impl;

    setup_provider_capabilities(provider);
    setup_provider_vtable(provider);

    agentos_error_t err = provider->init(provider, storage_path);
    if (err != AGENTOS_SUCCESS) {
        free(impl);
        free(provider);
        return err;
    }

    g_active_provider = provider;
    return AGENTOS_SUCCESS;
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
    agentos_memory_provider_t* provider = (agentos_memory_provider_t*)calloc(1, sizeof(agentos_memory_provider_t));
    if (!provider) return NULL;

    builtin_provider_impl_t* impl = (builtin_provider_impl_t*)calloc(1, sizeof(builtin_provider_impl_t));
    if (!impl) {
        free(provider);
        return NULL;
    }

    provider->name = "builtin";
    provider->version = "0.0.5";
    provider->impl = impl;

    setup_provider_capabilities(provider);
    setup_provider_vtable(provider);

    return provider;
}

void agentos_memory_provider_free_query_results(
    char** record_ids, float* scores, size_t count) {
    if (!record_ids && !scores) return;
    if (record_ids) {
        for (size_t i = 0; i < count; i++) {
            if (record_ids[i]) free(record_ids[i]);
        }
        free(record_ids);
    }
    if (scores) free(scores);
}
