/**
 * @file builtin_provider.c
 * @brief AgentRT 内置免费内存提供商实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 agentrt_memory_provider_t 接口，提供 L1+L2 基础功能。
 * 无 MemoryRovol / FAISS / SQLite 依赖。
 * 启动日志显示 "using built-in provider (free)"。
 */

#include "error.h"
#include "logging_compat.h"
#include "memory_compat.h"
#include "memory_provider.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


typedef struct builtin_storage builtin_storage_t;
typedef struct builtin_index builtin_index_t;
typedef struct builtin_retrieval builtin_retrieval_t;

extern builtin_storage_t *builtin_storage_create(const char *base_path);
extern void builtin_storage_destroy(builtin_storage_t *st);
extern agentrt_error_t builtin_storage_write(builtin_storage_t *st, const void *data, size_t len,
                                             const char *metadata_json, char **out_record_id);
extern agentrt_error_t builtin_storage_get(const builtin_storage_t *st, const char *record_id,
                                           void **out_data, size_t *out_len);
extern agentrt_error_t builtin_storage_delete(builtin_storage_t *st, const char *record_id);
extern size_t builtin_storage_count(const builtin_storage_t *st);
extern const char *builtin_storage_get_record_id(const builtin_storage_t *st, size_t index);
extern time_t builtin_storage_get_updated_at(const builtin_storage_t *st, size_t index);
extern agentrt_error_t builtin_storage_touch(builtin_storage_t *st, const char *record_id);

extern builtin_index_t *builtin_index_create(void);
extern void builtin_index_destroy(builtin_index_t *idx);
extern void builtin_index_add(builtin_index_t *idx, const char *record_id,
                              const char *metadata_json, size_t data_len);
extern void builtin_index_remove(builtin_index_t *idx, const char *record_id);
extern agentrt_error_t builtin_index_search(const builtin_index_t *idx, const char *query_text,
                                            uint32_t limit, char ***out_record_ids,
                                            float **out_scores, size_t *out_count);
extern size_t builtin_index_total_docs(const builtin_index_t *idx);
extern agentrt_error_t builtin_index_compact(builtin_index_t *idx);

extern builtin_retrieval_t *builtin_retrieval_create(void);
extern void builtin_retrieval_destroy(builtin_retrieval_t *ret);
extern agentrt_error_t builtin_retrieval_find(const builtin_retrieval_t *ret, const char *query,
                                              uint32_t limit, char ***out_ids, float **out_scores,
                                              size_t *out_count);

typedef struct builtin_provider_impl {
    void *storage;
    void *index;
    void *retrieval;
    agentrt_memory_stats_t stats;
} builtin_provider_impl_t;

/* Forward declarations */
static void setup_provider_capabilities(agentrt_memory_provider_t *provider);
static void setup_provider_vtable(agentrt_memory_provider_t *provider);

static agentrt_error_t builtin_init(agentrt_memory_provider_t *provider, const char *config_path)
{
    if (!provider)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to init builtin provider: null provider");

    /* Re-allocate impl if destroyed by previous cleanup */
    if (!provider->impl) {
        AGENTRT_LOG_INFO("C-L07: Memory: BuiltinProvider RE-INIT (impl was destroyed, re-allocating)");
        builtin_provider_impl_t *new_impl =
            (builtin_provider_impl_t *)AGENTRT_CALLOC(1, sizeof(builtin_provider_impl_t));
        if (!new_impl)
            return AGENTRT_ENOMEM;
        provider->impl = new_impl;
        /* Re-setup vtable and capabilities since they were lost */
        setup_provider_capabilities(provider);
        setup_provider_vtable(provider);
    }

    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    /* 幂等性保护：如果 storage/index/retrieval 已存在（重复 init 调用），
     * 先销毁旧对象，避免直接覆盖指针导致内存泄漏 */
    if (impl->storage) {
        builtin_storage_destroy(impl->storage);
        impl->storage = NULL;
    }
    if (impl->index) {
        builtin_index_destroy(impl->index);
        impl->index = NULL;
    }
    if (impl->retrieval) {
        builtin_retrieval_destroy(impl->retrieval);
        impl->retrieval = NULL;
    }

    const char *path = config_path ? config_path : "./data/agentos/memory";
    impl->storage = builtin_storage_create(path);
    if (!impl->storage)
        return AGENTRT_ENOMEM;

    impl->index = builtin_index_create();
    if (!impl->index) {
        builtin_storage_destroy(impl->storage);
        impl->storage = NULL;
        return AGENTRT_ENOMEM;
    }

    impl->retrieval = builtin_retrieval_create();
    if (!impl->retrieval) {
        builtin_storage_destroy(impl->storage);
        builtin_index_destroy(impl->index);
        impl->storage = NULL;
        impl->index = NULL;
        return AGENTRT_ENOMEM;
    }

    __builtin_memset(&impl->stats, 0, sizeof(impl->stats));
    snprintf(impl->stats.provider_name, sizeof(impl->stats.provider_name), "builtin");
    snprintf(impl->stats.provider_version, sizeof(impl->stats.provider_version), "0.1.0");

    AGENTRT_LOG_INFO("[AgentRT] using built-in provider (free) - storage: %s", path);

    return AGENTRT_SUCCESS;
}

static void builtin_destroy(agentrt_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return;

    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;
    if (impl->storage)
        builtin_storage_destroy(impl->storage);
    if (impl->index)
        builtin_index_destroy(impl->index);
    if (impl->retrieval)
        builtin_retrieval_destroy(impl->retrieval);
    impl->storage = NULL;
    impl->index = NULL;
    impl->retrieval = NULL;
    AGENTRT_FREE(impl);
    provider->impl = NULL;
}

static agentrt_error_t builtin_write_raw(agentrt_memory_provider_t *provider, const void *data,
                                         size_t len, const char *metadata_json,
                                         char **out_record_id)
{

    if (!provider || !provider->impl)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to write raw memory: null provider or impl");
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    agentrt_error_t err =
        builtin_storage_write(impl->storage, data, len, metadata_json, out_record_id);
    if (err != AGENTRT_SUCCESS)
        return err;

    builtin_index_add(impl->index, *out_record_id, metadata_json ? metadata_json : "", len);

    impl->stats.total_records++;
    impl->stats.l1_count++;
    impl->stats.total_bytes += len;

    return AGENTRT_SUCCESS;
}

static agentrt_error_t builtin_get_raw(agentrt_memory_provider_t *provider, const char *record_id,
                                       void **out_data, size_t *out_len)
{

    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;
    return builtin_storage_get(impl->storage, record_id, out_data, out_len);
}

static agentrt_error_t builtin_delete_raw(agentrt_memory_provider_t *provider,
                                          const char *record_id)
{

    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    agentrt_error_t err = builtin_storage_delete(impl->storage, record_id);
    if (err == AGENTRT_SUCCESS) {
        builtin_index_remove(impl->index, record_id);
        if (impl->stats.total_records > 0)
            impl->stats.total_records--;
        if (impl->stats.l1_count > 0)
            impl->stats.l1_count--;
    }
    return err;
}

static agentrt_error_t builtin_query(agentrt_memory_provider_t *provider, const char *query_text,
                                     uint32_t limit, char ***out_record_ids, float **out_scores,
                                     size_t *out_count)
{

    if (!provider || !provider->impl || !query_text)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to query memory: null provider, impl, or query_text");
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;
    return builtin_index_search(impl->index, query_text, limit, out_record_ids, out_scores,
                                out_count);
}

static agentrt_error_t __attribute__((unused))
builtin_retrieve_fn(agentrt_memory_provider_t *provider, const char *query_text, uint32_t limit,
                    char ***out_record_ids, float **out_scores, size_t *out_count)
{

    if (!provider || !provider->impl || !query_text)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;
    return builtin_index_search(impl->index, query_text, limit, out_record_ids, out_scores,
                                out_count);
}

static agentrt_error_t builtin_evolve(agentrt_memory_provider_t *provider, int force)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    if (impl->index) {
        agentrt_error_t err = builtin_index_compact(impl->index);
        if (err != AGENTRT_SUCCESS)
            return err;
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t builtin_forget(agentrt_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    size_t total = builtin_storage_count(impl->storage);
    if (total == 0)
        return AGENTRT_SUCCESS;

    double forget_ratio = 0.1;
    size_t forget_count = (size_t)(total * forget_ratio);
    if (forget_count < 1)
        forget_count = 1;
    if (forget_count > total)
        forget_count = total;

    typedef struct {
        size_t index;
        time_t updated_at;
    } record_age_t;

    record_age_t *ages;
    SAFE_MALLOC_ARRAY(ages, total, sizeof(record_age_t));
    if (!ages)
        return AGENTRT_ENOMEM;

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
        const char *rid_ptr = builtin_storage_get_record_id(impl->storage, ages[i].index);
        if (!rid_ptr)
            continue;

        char record_id[64];
        snprintf(record_id, sizeof(record_id), "%s", rid_ptr);

        agentrt_error_t del_err = builtin_storage_delete(impl->storage, record_id);
        if (del_err == AGENTRT_SUCCESS) {
            builtin_index_remove(impl->index, record_id);
            actually_forgotten++;
        }
    }

    AGENTRT_FREE(ages);

    impl->stats.total_records = builtin_storage_count(impl->storage);
    impl->stats.l1_count = impl->stats.total_records;
    impl->stats.l2_indexed = builtin_index_total_docs(impl->index);

    (void)actually_forgotten;
    return AGENTRT_SUCCESS;
}

static agentrt_error_t builtin_stats(agentrt_memory_provider_t *provider,
                                     agentrt_memory_stats_t *out_stats)
{

    if (!provider || !provider->impl || !out_stats)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to get memory stats: null provider, impl, or out_stats");
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    impl->stats.l2_indexed = builtin_index_total_docs(impl->index);
    __builtin_memcpy(out_stats, &impl->stats, sizeof(agentrt_memory_stats_t));
    return AGENTRT_SUCCESS;
}

static agentrt_error_t builtin_mount(agentrt_memory_provider_t *provider, const char *record_id,
                                     const char *context)
{
    if (!provider || !provider->impl || !record_id)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    if (!impl->storage)
        return AGENTRT_ENOTINIT;

    void *data = NULL;
    size_t len = 0;
    agentrt_error_t err = builtin_storage_get(impl->storage, record_id, &data, &len);
    if (err != AGENTRT_SUCCESS)
        return err;
    if (data)
        AGENTRT_FREE(data);

    builtin_storage_touch(impl->storage, record_id);

    return AGENTRT_SUCCESS;
}

static agentrt_error_t builtin_health_check(agentrt_memory_provider_t *provider, char **out_json)
{
    if (!provider || !out_json)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to check memory health: null provider or out_json");
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    char buf[512];
    snprintf(buf, sizeof(buf), "{\"status\":\"%s\",\"provider\":\"%s\",\"records\":%llu}",
             impl && impl->storage ? "healthy" : "degraded",
             provider->name ? provider->name : "builtin",
             (unsigned long long)(impl ? impl->stats.total_records : 0));

    *out_json = AGENTRT_STRDUP(buf);
    return *out_json ? AGENTRT_SUCCESS : AGENTRT_ENOMEM;
}

static agentrt_error_t builtin_add_memory(agentrt_memory_provider_t *provider, const char *content,
                                          size_t content_len)
{
    if (!provider || !provider->impl || !content)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    char *record_id = NULL;
    agentrt_error_t err =
        builtin_storage_write(impl->storage, content, content_len, NULL, &record_id);
    if (err != AGENTRT_SUCCESS)
        return err;

    builtin_index_add(impl->index, record_id, "", content_len);
    impl->stats.total_records++;
    impl->stats.l1_count++;
    impl->stats.total_bytes += content_len;

    AGENTRT_FREE(record_id);
    return AGENTRT_SUCCESS;
}

static agentrt_error_t builtin_sync_push(agentrt_memory_provider_t *provider, const char *record_id)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;
    if (!record_id)
        return AGENTRT_EINVAL;
    if (!provider->sync_target)
        return AGENTRT_ENOTSUP;

    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    void *data = NULL;
    size_t data_len = 0;
    agentrt_error_t err = builtin_storage_get(impl->storage, record_id, &data, &data_len);
    if (err != AGENTRT_SUCCESS || !data)
        return err;

    char *target_id = NULL;
    err = provider->sync_target->write_raw(provider->sync_target, data, data_len, NULL, &target_id);
    AGENTRT_FREE(data);
    AGENTRT_FREE(target_id);
    return err;
}

static agentrt_error_t builtin_sync_pull(agentrt_memory_provider_t *provider,
                                         const char *filter_json, char ***out_record_ids,
                                         size_t *out_count)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;
    if (!out_record_ids || !out_count)
        return AGENTRT_EINVAL;
    if (!provider->sync_target)
        return AGENTRT_ENOTSUP;

    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;

    char **source_ids = NULL;
    float *scores = NULL;
    size_t source_count = 0;
    agentrt_error_t err =
        provider->sync_target->query(provider->sync_target, filter_json ? filter_json : "*", 100,
                                     &source_ids, &scores, &source_count);
    if (err != AGENTRT_SUCCESS) {
        agentrt_memory_provider_free_query_results(source_ids, scores, source_count);
        return err;
    }

    size_t pulled = 0;
    char **pulled_ids = (char **)AGENTRT_CALLOC(source_count, sizeof(char *));
    if (!pulled_ids) {
        agentrt_memory_provider_free_query_results(source_ids, scores, source_count);
        return AGENTRT_ENOMEM;
    }

    for (size_t i = 0; i < source_count; i++) {
        void *data = NULL;
        size_t data_len = 0;
        err =
            provider->sync_target->get_raw(provider->sync_target, source_ids[i], &data, &data_len);
        if (err != AGENTRT_SUCCESS || !data) {
            AGENTRT_FREE(data);
            continue;
        }
        char *new_id = NULL;
        err = builtin_storage_write(impl->storage, data, data_len, NULL, &new_id);
        if (err == AGENTRT_SUCCESS && new_id) {
            builtin_index_add(impl->index, new_id, "", data_len);
            pulled_ids[pulled] = source_ids[i];
            source_ids[i] = NULL;
            pulled++;
        }
        AGENTRT_FREE(data);
        AGENTRT_FREE(new_id);
    }

    agentrt_memory_provider_free_query_results(source_ids, scores, source_count);
    *out_record_ids = pulled_ids;
    *out_count = pulled;
    return AGENTRT_SUCCESS;
}

static int builtin_has_active_sync(agentrt_memory_provider_t *provider)
{
    if (!provider)
        return 0;
    return provider->sync_target ? 1 : 0;
}

static agentrt_error_t builtin_retrieve(agentrt_memory_provider_t *provider, const char *query,
                                        uint32_t limit, char ***out_record_ids, float **out_scores,
                                        size_t *out_count)
{
    if (!provider || !provider->impl || !query)
        return AGENTRT_EINVAL;
    builtin_provider_impl_t *impl = (builtin_provider_impl_t *)provider->impl;
    return builtin_index_search(impl->index, query, limit, out_record_ids, out_scores, out_count);
}

/* ========== 全局提供商注册 ========== */

static agentrt_memory_provider_t *g_active_provider = NULL;

agentrt_error_t agentrt_memory_provider_register(agentrt_memory_provider_t *provider)
{
    if (!provider)
        return AGENTRT_EINVAL;
    g_active_provider = provider;
    return AGENTRT_SUCCESS;
}

agentrt_memory_provider_t *agentrt_memory_provider_get_active(void)
{
    return g_active_provider;
}

agentrt_error_t agentrt_memory_provider_set_active(agentrt_memory_provider_t *provider)
{
    if (!provider)
        return AGENTRT_EINVAL;
    g_active_provider = provider;
    return AGENTRT_SUCCESS;
}

void agentrt_memory_provider_unregister(void)
{
    if (g_active_provider) {
        if (g_active_provider->destroy) {
            g_active_provider->destroy(g_active_provider);
        }
        AGENTRT_FREE(g_active_provider);
        g_active_provider = NULL;
    }
}

static void setup_provider_vtable(agentrt_memory_provider_t *provider)
{
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
    provider->sync_push = builtin_sync_push;
    provider->sync_pull = builtin_sync_pull;
    provider->has_active_sync = builtin_has_active_sync;
    provider->sync_target = NULL;
}

static void setup_provider_capabilities(agentrt_memory_provider_t *provider)
{
    provider->capabilities.l1_raw = 1;
    provider->capabilities.l2_feature =
        0; /* builtin uses keyword inverted index only, not semantic vector search */
    provider->capabilities.l3_structure = 0;
    provider->capabilities.l4_pattern = 0;
    provider->capabilities.forgetting = 1;
    provider->capabilities.attractor = 0;
    provider->capabilities.persistence = 1;
    provider->capabilities.faiss = 0;
    provider->capabilities.async_ops = 0;
    provider->capabilities.llm_integration = 0;
}

agentrt_error_t agentrt_builtin_memory_provider_init(const char *storage_path)
{
    if (g_active_provider) {
        return AGENTRT_SUCCESS;
    }

    agentrt_memory_provider_t *provider =
        (agentrt_memory_provider_t *)AGENTRT_CALLOC(1, sizeof(agentrt_memory_provider_t));
    if (!provider)
        return AGENTRT_ENOMEM;

    builtin_provider_impl_t *impl =
        (builtin_provider_impl_t *)AGENTRT_CALLOC(1, sizeof(builtin_provider_impl_t));
    if (!impl) {
        AGENTRT_FREE(provider);
        return AGENTRT_ENOMEM;
    }

    provider->name = "builtin";
    provider->version = "0.1.0";
    provider->impl = impl;

    setup_provider_capabilities(provider);
    setup_provider_vtable(provider);

    agentrt_error_t err = provider->init(provider, storage_path);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_FREE(impl);
        AGENTRT_FREE(provider);
        return err;
    }

    g_active_provider = provider;
    return AGENTRT_SUCCESS;
}

void agentrt_memory_query_result_free(agentrt_memory_query_result_t *result)
{
    if (!result)
        return;
    if (result->record_ids) {
        for (size_t i = 0; i < result->count; i++) {
            if (result->record_ids[i])
                AGENTRT_FREE(result->record_ids[i]);
        }
        AGENTRT_FREE(result->record_ids);
    }
    if (result->scores)
        AGENTRT_FREE(result->scores);
    result->record_ids = NULL;
    result->scores = NULL;
    result->count = 0;
}

agentrt_memory_provider_t *agentrt_builtin_provider_create(void)
{
    agentrt_memory_provider_t *provider =
        (agentrt_memory_provider_t *)AGENTRT_CALLOC(1, sizeof(agentrt_memory_provider_t));
    if (!provider)
        return NULL;

    builtin_provider_impl_t *impl =
        (builtin_provider_impl_t *)AGENTRT_CALLOC(1, sizeof(builtin_provider_impl_t));
    if (!impl) {
        AGENTRT_FREE(provider);
        return NULL;
    }

    provider->name = "builtin";
    provider->version = "0.1.0";
    provider->impl = impl;

    setup_provider_capabilities(provider);
    setup_provider_vtable(provider);

    return provider;
}

void agentrt_memory_provider_free_query_results(char **record_ids, float *scores, size_t count)
{
    if (!record_ids && !scores)
        return;
    if (record_ids) {
        for (size_t i = 0; i < count; i++) {
            if (record_ids[i])
                AGENTRT_FREE(record_ids[i]);
        }
        AGENTRT_FREE(record_ids);
    }
    if (scores)
        AGENTRT_FREE(scores);
}
