/**
 * @file memoryrovol_bridge.c
 * @brief C-L12: CoreLoopThree → MemoryRovol 提供商桥接实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 agentos_memory_provider_t 函数指针表，
 * 将 MemoryRovol API 封装为 AgentRT 标准内存提供商接口。
 *
 * 数据流：
 *   CoreLoopThree cognition → agentos_memory_provider_t::write_raw()
 *     → memoryrovol_bridge → agentos_memoryrov_write_raw()
 *     → MemoryRovol L1 存储 → L2 特征提取 → L3 结构绑定 → L4 模式识别
 *
 *   CoreLoopThree retrieval → agentos_memory_provider_t::query()
 *     → memoryrovol_bridge → agentos_memoryrov_query()
 *     → FAISS 向量检索 → 返回语义相似结果
 */

#include "memoryrovol_bridge.h"
#include "memory_provider.h"
#include "memoryrovol.h"

#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 默认配置 ==================== */

#define DEFAULT_STORAGE_PATH     "/var/lib/agentos/memoryrovol"
#define DEFAULT_CONFIG_PATH      "/etc/agentos/memoryrovol.yaml"
#define DEFAULT_QUERY_LIMIT      10
#define DEFAULT_SYNC_INTERVAL_MS 5000

/* ==================== 内部结构 ==================== */

struct memoryrovol_bridge_s {
    memoryrovol_bridge_config_t config;
    agentos_memoryrovol_handle_t *rov_handle;  /* MemoryRovol 引擎句柄 */
    agentos_memory_provider_t provider;         /* 填充的 provider 接口 */
    agentos_memory_provider_t *builtin_provider;/* 内置提供商（混合模式） */
    char current_mode[32];                      /* 当前模式 */
    bool initialized;
    bool sync_active;

    /* C-L12: 统计 */
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_queries;
    uint64_t total_errors;
    uint64_t total_deletes;
    uint64_t total_evolves;
    uint64_t total_forgets;
    uint64_t total_mounts;

    /* C-L12: 延迟统计 */
    uint64_t write_latency_total_us;
    uint64_t read_latency_total_us;
    uint64_t query_latency_total_us;
    uint64_t max_write_latency_us;
    uint64_t max_read_latency_us;
    uint64_t max_query_latency_us;
    uint64_t min_write_latency_us;
    uint64_t min_read_latency_us;
    uint64_t min_query_latency_us;

    /* C-L12: 吞吐量 */
    uint64_t total_bytes_written;
    uint64_t total_bytes_read;
    uint64_t total_query_results;
};

/* ==================== 内部辅助函数 ==================== */

static void fill_capabilities(agentos_memory_capabilities_t *caps,
                              const memoryrovol_bridge_config_t *config)
{
    __builtin_memset(caps, 0, sizeof(*caps));
    caps->l1_raw = config->enable_l1_raw ? 1 : 0;
    caps->l2_feature = config->enable_l2_feature ? 1 : 0;
    caps->l3_structure = config->enable_l3_structure ? 1 : 0;
    caps->l4_pattern = config->enable_l4_pattern ? 1 : 0;
    caps->forgetting = config->enable_forgetting ? 1 : 0;
    caps->attractor = config->enable_attractor ? 1 : 0;
    caps->persistence = config->enable_persistence ? 1 : 0;
    caps->faiss = config->enable_faiss ? 1 : 0;
    caps->async_ops = config->enable_async_ops ? 1 : 0;
    caps->llm_integration = config->enable_llm_integration ? 1 : 0;
}

/* ──── provider 回调实现 ──── */

static agentos_error_t bridge_init(struct agentos_memory_provider *provider,
                                   const char *config_path)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge) return AGENTOS_ERR_INVALID_PARAM;

    agentos_memoryrov_config_t rov_cfg;
    __builtin_memset(&rov_cfg, 0, sizeof(rov_cfg));

    agentos_error_t ret = agentos_memoryrov_init(
        &rov_cfg, &bridge->rov_handle);
    if (ret != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("C-L12: MemoryRovol init failed: ret=%d", ret);
        return ret;
    }

    AGENTOS_LOG_INFO("C-L12: MemoryRovol engine initialized");
    return AGENTOS_SUCCESS;
}

static void bridge_destroy(struct agentos_memory_provider *provider)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge) return;

    if (bridge->rov_handle) {
        agentos_memoryrov_cleanup(bridge->rov_handle);
        bridge->rov_handle = NULL;
    }
}

static agentos_error_t bridge_write_raw(struct agentos_memory_provider *provider,
                                        const void *data, size_t len,
                                        const char *metadata_json,
                                        char **out_record_id)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    uint64_t start_us = agentos_time_ns() / 1000;

    agentos_error_t ret = agentos_memoryrov_write_raw(
        bridge->rov_handle, data, len, metadata_json, out_record_id);

    uint64_t latency_us = agentos_time_ns() / 1000 - start_us;

    if (ret == AGENTOS_SUCCESS) {
        bridge->total_writes++;
        bridge->total_bytes_written += len;
        bridge->write_latency_total_us += latency_us;
        if (latency_us > bridge->max_write_latency_us)
            bridge->max_write_latency_us = latency_us;
        if (bridge->min_write_latency_us == 0 || latency_us < bridge->min_write_latency_us)
            bridge->min_write_latency_us = latency_us;

        AGENTOS_LOG_DEBUG("C-L12: WRITE-OK id=%s len=%zu latency=%lluus "
                          "(total_writes=%llu bytes=%llu)",
                          out_record_id && *out_record_id ? *out_record_id : "?",
                          len, (unsigned long long)latency_us,
                          (unsigned long long)bridge->total_writes,
                          (unsigned long long)bridge->total_bytes_written);
    } else {
        bridge->total_errors++;
        AGENTOS_LOG_WARN("C-L12: WRITE-FAIL len=%zu ret=%d latency=%lluus",
                         len, ret, (unsigned long long)latency_us);
    }
    return ret;
}

static agentos_error_t bridge_get_raw(struct agentos_memory_provider *provider,
                                      const char *record_id,
                                      void **out_data, size_t *out_len)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    uint64_t start_us = agentos_time_ns() / 1000;

    agentos_error_t ret = agentos_memoryrov_get_raw(
        bridge->rov_handle, record_id, out_data, out_len);

    uint64_t latency_us = agentos_time_ns() / 1000 - start_us;

    if (ret == AGENTOS_SUCCESS) {
        bridge->total_reads++;
        if (out_len && *out_len) bridge->total_bytes_read += *out_len;
        bridge->read_latency_total_us += latency_us;
        if (latency_us > bridge->max_read_latency_us)
            bridge->max_read_latency_us = latency_us;
        if (bridge->min_read_latency_us == 0 || latency_us < bridge->min_read_latency_us)
            bridge->min_read_latency_us = latency_us;

        AGENTOS_LOG_DEBUG("C-L12: READ-OK id=%s len=%zu latency=%lluus "
                          "(total_reads=%llu bytes=%llu)",
                          record_id ? record_id : "?",
                          out_len && *out_len ? *out_len : 0,
                          (unsigned long long)latency_us,
                          (unsigned long long)bridge->total_reads,
                          (unsigned long long)bridge->total_bytes_read);
    } else {
        bridge->total_errors++;
        AGENTOS_LOG_DEBUG("C-L12: READ-FAIL id=%s ret=%d latency=%lluus",
                          record_id ? record_id : "?", ret,
                          (unsigned long long)latency_us);
    }
    return ret;
}

static agentos_error_t bridge_delete_raw(struct agentos_memory_provider *provider,
                                         const char *record_id)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    agentos_error_t ret = agentos_memoryrov_delete_raw(bridge->rov_handle, record_id);
    if (ret == AGENTOS_SUCCESS) {
        bridge->total_deletes++;
        AGENTOS_LOG_DEBUG("C-L12: DELETE-OK id=%s (total_deletes=%llu)",
                          record_id ? record_id : "?",
                          (unsigned long long)bridge->total_deletes);
    } else {
        bridge->total_errors++;
        AGENTOS_LOG_WARN("C-L12: DELETE-FAIL id=%s ret=%d",
                         record_id ? record_id : "?", ret);
    }
    return ret;
}

static agentos_error_t bridge_query(struct agentos_memory_provider *provider,
                                    const char *query_text, uint32_t limit,
                                    char ***out_record_ids, float **out_scores,
                                    size_t *out_count)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    if (limit == 0) limit = (uint32_t)bridge->config.query_default_limit;

    uint64_t start_us = agentos_time_ns() / 1000;

    agentos_error_t ret = agentos_memoryrov_query(
        bridge->rov_handle, query_text, limit,
        out_record_ids, out_scores, out_count);

    uint64_t latency_us = agentos_time_ns() / 1000 - start_us;

    if (ret == AGENTOS_SUCCESS) {
        bridge->total_queries++;
        if (out_count) bridge->total_query_results += *out_count;
        bridge->query_latency_total_us += latency_us;
        if (latency_us > bridge->max_query_latency_us)
            bridge->max_query_latency_us = latency_us;
        if (bridge->min_query_latency_us == 0 || latency_us < bridge->min_query_latency_us)
            bridge->min_query_latency_us = latency_us;

        AGENTOS_LOG_DEBUG("C-L12: QUERY-OK "
                          "q=\"%.64s\" limit=%u results=%zu latency=%lluus "
                          "(total_queries=%llu results=%llu)",
                          query_text ? query_text : "?",
                          limit,
                          out_count ? *out_count : 0,
                          (unsigned long long)latency_us,
                          (unsigned long long)bridge->total_queries,
                          (unsigned long long)bridge->total_query_results);
    } else {
        bridge->total_errors++;
        AGENTOS_LOG_WARN("C-L12: QUERY-FAIL q=\"%.64s\" limit=%u ret=%d latency=%lluus",
                         query_text ? query_text : "?", limit, ret,
                         (unsigned long long)latency_us);
    }
    return ret;
}

static agentos_error_t bridge_retrieve(struct agentos_memory_provider *provider,
                                       const char *query_text, uint32_t limit,
                                       char ***out_record_ids, float **out_scores,
                                       size_t *out_count)
{
    /* retrieve 和 query 在当前实现中相同 */
    return bridge_query(provider, query_text, limit,
                        out_record_ids, out_scores, out_count);
}

static agentos_error_t bridge_evolve(struct agentos_memory_provider *provider,
                                     int force)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    AGENTOS_LOG_INFO("C-L12: EVOLVE force=%d (total_evolves=%llu)",
                     force, (unsigned long long)bridge->total_evolves);

    agentos_error_t ret = agentos_memoryrov_evolve(bridge->rov_handle, force);
    if (ret == AGENTOS_SUCCESS) {
        bridge->total_evolves++;
    } else {
        bridge->total_errors++;
        AGENTOS_LOG_WARN("C-L12: EVOLVE-FAIL force=%d ret=%d", force, ret);
    }
    return ret;
}

static agentos_error_t bridge_forget(struct agentos_memory_provider *provider)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    AGENTOS_LOG_INFO("C-L12: FORGET (total_forgets=%llu)",
                     (unsigned long long)bridge->total_forgets);

    agentos_error_t ret = agentos_memoryrov_forget(bridge->rov_handle);
    if (ret == AGENTOS_SUCCESS) {
        bridge->total_forgets++;
    } else {
        bridge->total_errors++;
        AGENTOS_LOG_WARN("C-L12: FORGET-FAIL ret=%d", ret);
    }
    return ret;
}

static agentos_error_t bridge_stats(struct agentos_memory_provider *provider,
                                    agentos_memory_stats_t *out_stats)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !out_stats) return AGENTOS_ERR_INVALID_PARAM;

    __builtin_memset(out_stats, 0, sizeof(*out_stats));

    char *stats_json = NULL;
    agentos_error_t ret = agentos_memoryrov_stats(
        bridge->rov_handle, &stats_json);
    if (ret != AGENTOS_SUCCESS) return ret;

    safe_strcpy(out_stats->provider_name,
                bridge->config.provider_name ? bridge->config.provider_name
                                             : "MemoryRovol",
                sizeof(out_stats->provider_name));
    safe_strcpy(out_stats->provider_version,
                bridge->config.provider_version ? bridge->config.provider_version
                                                : "v0.1.1",
                sizeof(out_stats->provider_version));

    if (stats_json) AGENTOS_FREE(stats_json);
    return AGENTOS_SUCCESS;
}

static agentos_error_t bridge_mount(struct agentos_memory_provider *provider,
                                    const char *record_id, const char *context)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    agentos_error_t ret = agentos_memoryrov_mount(bridge->rov_handle, record_id, context);
    if (ret == AGENTOS_SUCCESS) {
        bridge->total_mounts++;
        AGENTOS_LOG_DEBUG("C-L12: MOUNT-OK id=%s ctx=%.32s (total_mounts=%llu)",
                          record_id ? record_id : "?",
                          context ? context : "(none)",
                          (unsigned long long)bridge->total_mounts);
    } else {
        bridge->total_errors++;
        AGENTOS_LOG_WARN("C-L12: MOUNT-FAIL id=%s ret=%d",
                         record_id ? record_id : "?", ret);
    }
    return ret;
}

static agentos_error_t bridge_health_check(struct agentos_memory_provider *provider,
                                           char **out_json)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !out_json) return AGENTOS_ERR_INVALID_PARAM;

    const char *health_fmt =
        "{\"status\":\"ok\",\"provider\":\"%s\",\"version\":\"%s\","
        "\"reads\":%llu,\"writes\":%llu,\"queries\":%llu,\"errors\":%llu}";

    size_t needed = snprintf(NULL, 0, health_fmt,
                             bridge->config.provider_name ? bridge->config.provider_name : "MemoryRovol",
                             bridge->config.provider_version ? bridge->config.provider_version : "v0.1.1",
                             (unsigned long long)bridge->total_reads,
                             (unsigned long long)bridge->total_writes,
                             (unsigned long long)bridge->total_queries,
                             (unsigned long long)bridge->total_errors);

    *out_json = (char *)AGENTOS_MALLOC(needed + 1);
    if (!*out_json) return AGENTOS_ERR_OUT_OF_MEMORY;

    snprintf(*out_json, needed + 1, health_fmt,
             bridge->config.provider_name ? bridge->config.provider_name : "MemoryRovol",
             bridge->config.provider_version ? bridge->config.provider_version : "v0.1.1",
             (unsigned long long)bridge->total_reads,
             (unsigned long long)bridge->total_writes,
             (unsigned long long)bridge->total_queries,
             (unsigned long long)bridge->total_errors);

    return AGENTOS_SUCCESS;
}

static agentos_error_t bridge_add_memory(struct agentos_memory_provider *provider,
                                         const char *content, size_t content_len)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge || !bridge->rov_handle) return AGENTOS_ERR_INVALID_STATE;

    return agentos_memoryrov_add_memory(
        bridge->rov_handle, content, content_len);
}

static agentos_error_t bridge_sync_push(struct agentos_memory_provider *provider,
                                        const char *record_id)
{
    /* 混合模式：将内置提供商的数据推送到 MemoryRovol */
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge) return AGENTOS_ERR_INVALID_STATE;

    if (!bridge->builtin_provider) return AGENTOS_ERR_NOT_SUPPORTED;

    /* 从内置提供商读取数据 */
    void *data = NULL;
    size_t len = 0;
    agentos_error_t ret = bridge->builtin_provider->get_raw(
        bridge->builtin_provider, record_id, &data, &len);
    if (ret != AGENTOS_SUCCESS) return ret;

    /* 写入 MemoryRovol */
    ret = agentos_memoryrov_write_raw(
        bridge->rov_handle, data, len, NULL, NULL);
    if (data) AGENTOS_FREE(data);

    return ret;
}

static agentos_error_t bridge_sync_pull(struct agentos_memory_provider *provider,
                                        const char *filter_json,
                                        char ***out_record_ids, size_t *out_count)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    if (!bridge) return AGENTOS_ERR_INVALID_STATE;

    if (!bridge->builtin_provider) return AGENTOS_ERR_NOT_SUPPORTED;

    /* 从 MemoryRovol 查询 */
    uint32_t limit = bridge->config.query_default_limit;
    float *scores = NULL;
    agentos_error_t ret = agentos_memoryrov_query(
        bridge->rov_handle, filter_json ? filter_json : "*",
        limit, out_record_ids, &scores, out_count);
    if (scores) AGENTOS_FREE(scores);

    return ret;
}

static int bridge_has_active_sync(struct agentos_memory_provider *provider)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)provider->impl;
    return bridge ? bridge->sync_active : 0;
}

/* ==================== 生命周期实现 ==================== */

memoryrovol_bridge_t *memoryrovol_bridge_create(
    const memoryrovol_bridge_config_t *config)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)AGENTOS_CALLOC(1, sizeof(memoryrovol_bridge_t));
    if (!bridge) {
        AGENTOS_LOG_ERROR("C-L12: memoryrovol_bridge_create: OOM");
        return NULL;
    }

    /* 应用配置 */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config.config_path = DEFAULT_CONFIG_PATH;
        bridge->config.storage_path = DEFAULT_STORAGE_PATH;
        bridge->config.provider_name = "MemoryRovol";
        bridge->config.provider_version = "v0.1.1";
        bridge->config.enable_l1_raw = true;
        bridge->config.enable_l2_feature = true;
        bridge->config.enable_l3_structure = true;
        bridge->config.enable_l4_pattern = true;
        bridge->config.enable_forgetting = true;
        bridge->config.enable_attractor = false;
        bridge->config.enable_persistence = false;
        bridge->config.enable_faiss = true;
        bridge->config.enable_async_ops = true;
        bridge->config.enable_llm_integration = true;
        bridge->config.query_default_limit = DEFAULT_QUERY_LIMIT;
        bridge->config.sync_interval_ms = DEFAULT_SYNC_INTERVAL_MS;
    }

    /* 填充 agentos_memory_provider_t 函数指针表 */
    agentos_memory_provider_t *p = &bridge->provider;
    __builtin_memset(p, 0, sizeof(*p));

    p->name = bridge->config.provider_name
                  ? bridge->config.provider_name : "MemoryRovol";
    p->version = bridge->config.provider_version
                     ? bridge->config.provider_version : "v0.1.1";
    fill_capabilities(&p->capabilities, &bridge->config);
    p->impl = bridge;

    /* 绑定函数指针 */
    p->init = bridge_init;
    p->destroy = bridge_destroy;
    p->write_raw = bridge_write_raw;
    p->get_raw = bridge_get_raw;
    p->delete_raw = bridge_delete_raw;
    p->query = bridge_query;
    p->retrieve = bridge_retrieve;
    p->evolve = bridge_evolve;
    p->forget = bridge_forget;
    p->stats = bridge_stats;
    p->mount = bridge_mount;
    p->health_check = bridge_health_check;
    p->add_memory = bridge_add_memory;
    p->sync_push = bridge_sync_push;
    p->sync_pull = bridge_sync_pull;
    p->has_active_sync = bridge_has_active_sync;

    /* 初始化 MemoryRovol 引擎 */
    agentos_memoryrov_config_t rov_cfg;
    __builtin_memset(&rov_cfg, 0, sizeof(rov_cfg));

    agentos_error_t ret = agentos_memoryrov_init(
        &rov_cfg, &bridge->rov_handle);
    if (ret != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("C-L12: MemoryRovol init failed (ret=%d), "
                         "bridge created but engine not active", ret);
        /* 非致命 — 桥接器创建成功，引擎可后续初始化 */
    }

    bridge->current_mode[0] = '\0';
    safe_strcpy(bridge->current_mode, "memoryrovol",
                sizeof(bridge->current_mode));
    bridge->initialized = true;
    bridge->sync_active = false;
    bridge->builtin_provider = NULL;

    AGENTOS_LOG_INFO("C-L12: MemoryRovol bridge created "
                     "(provider=%s v%s, L1=%d L2=%d L3=%d L4=%d "
                     "FAISS=%d async=%d)",
                     p->name, p->version,
                     bridge->config.enable_l1_raw,
                     bridge->config.enable_l2_feature,
                     bridge->config.enable_l3_structure,
                     bridge->config.enable_l4_pattern,
                     bridge->config.enable_faiss,
                     bridge->config.enable_async_ops);
    return bridge;
}

void memoryrovol_bridge_destroy(memoryrovol_bridge_t *bridge)
{
    if (!bridge) return;

    AGENTOS_LOG_INFO("C-L12: MemoryRovol bridge destroyed "
                     "(reads=%llu writes=%llu queries=%llu errors=%llu)",
                     (unsigned long long)bridge->total_reads,
                     (unsigned long long)bridge->total_writes,
                     (unsigned long long)bridge->total_queries,
                     (unsigned long long)bridge->total_errors);

    if (bridge->rov_handle) {
        agentos_memoryrov_cleanup(bridge->rov_handle);
        bridge->rov_handle = NULL;
    }

    AGENTOS_FREE(bridge);
}

/* ==================== 提供商接口实现 ==================== */

agentos_memory_provider_t *memoryrovol_bridge_get_provider(
    memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->initialized) return NULL;
    return &bridge->provider;
}

/* ==================== 提供商切换实现 ==================== */

int memoryrovol_bridge_switch_mode(memoryrovol_bridge_t *bridge,
                                   const char *mode)
{
    if (!bridge || !mode) return -1;

    if (strcmp(mode, "builtin") == 0) {
        /* 切换到内置提供商 */
        if (!bridge->builtin_provider) {
            bridge->builtin_provider = agentos_builtin_provider_create();
        }
        if (bridge->builtin_provider) {
            agentos_memory_provider_set_active(bridge->builtin_provider);
        }
        safe_strcpy(bridge->current_mode, "builtin",
                    sizeof(bridge->current_mode));
    } else if (strcmp(mode, "memoryrovol") == 0) {
        agentos_memory_provider_set_active(&bridge->provider);
        safe_strcpy(bridge->current_mode, "memoryrovol",
                    sizeof(bridge->current_mode));
    } else if (strcmp(mode, "hybrid") == 0) {
        if (!bridge->builtin_provider) {
            bridge->builtin_provider = agentos_builtin_provider_create();
        }
        /* 混合模式：活跃提供商是 MemoryRovol，sync_target 是内置 */
        bridge->provider.sync_target = bridge->builtin_provider;
        agentos_memory_provider_set_active(&bridge->provider);
        safe_strcpy(bridge->current_mode, "hybrid",
                    sizeof(bridge->current_mode));
    } else {
        AGENTOS_LOG_ERROR("C-L12: Unknown mode '%s'", mode);
        return -1;
    }

    AGENTOS_LOG_INFO("C-L12: Switched to mode '%s'", bridge->current_mode);
    return 0;
}

const char *memoryrovol_bridge_get_mode(memoryrovol_bridge_t *bridge)
{
    if (!bridge) return NULL;
    return bridge->current_mode;
}

/* ==================== 同步控制实现 ==================== */

int memoryrovol_bridge_start_sync(memoryrovol_bridge_t *bridge)
{
    if (!bridge) return -1;

    if (strcmp(bridge->current_mode, "hybrid") != 0) {
        AGENTOS_LOG_WARN("C-L12: Sync only available in hybrid mode "
                         "(current=%s)", bridge->current_mode);
        return -1;
    }

    bridge->sync_active = true;
    AGENTOS_LOG_INFO("C-L12: Sync started");
    return 0;
}

void memoryrovol_bridge_stop_sync(memoryrovol_bridge_t *bridge)
{
    if (!bridge) return;
    bridge->sync_active = false;
    AGENTOS_LOG_INFO("C-L12: Sync stopped");
}

bool memoryrovol_bridge_has_active_sync(memoryrovol_bridge_t *bridge)
{
    return bridge ? bridge->sync_active : false;
}

/* ==================== 状态查询实现 ==================== */

int memoryrovol_bridge_get_stats(memoryrovol_bridge_t *bridge,
                                 agentos_memory_stats_t *out_stats)
{
    if (!bridge || !out_stats) return -1;

    __builtin_memset(out_stats, 0, sizeof(*out_stats));

    if (bridge->rov_handle) {
        char *stats_json = NULL;
        agentos_memoryrov_stats(bridge->rov_handle, &stats_json);
        if (stats_json) AGENTOS_FREE(stats_json);
    }

    safe_strcpy(out_stats->provider_name,
                bridge->config.provider_name ? bridge->config.provider_name
                                             : "MemoryRovol",
                sizeof(out_stats->provider_name));
    safe_strcpy(out_stats->provider_version,
                bridge->config.provider_version ? bridge->config.provider_version
                                                : "v0.1.1",
                sizeof(out_stats->provider_version));

    return 0;
}

int memoryrovol_bridge_health_check(memoryrovol_bridge_t *bridge,
                                    char **out_json)
{
    if (!bridge || !out_json) return -1;

    return bridge_health_check(&bridge->provider, out_json);
}

bool memoryrovol_bridge_is_ready(memoryrovol_bridge_t *bridge)
{
    return bridge ? bridge->initialized : false;
}

void memoryrovol_bridge_dump_stats(memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->initialized) {
        AGENTOS_LOG_WARN("C-L12: BRIDGE-STATS unavailable");
        return;
    }

    uint64_t avg_write = bridge->total_writes > 0
        ? bridge->write_latency_total_us / bridge->total_writes : 0;
    uint64_t avg_read = bridge->total_reads > 0
        ? bridge->read_latency_total_us / bridge->total_reads : 0;
    uint64_t avg_query = bridge->total_queries > 0
        ? bridge->query_latency_total_us / bridge->total_queries : 0;

    AGENTOS_LOG_INFO("C-L12: BRIDGE-STATS mode=%s "
                     "reads=%llu writes=%llu queries=%llu "
                     "errors=%llu deletes=%llu evolves=%llu forgets=%llu mounts=%llu "
                     "write_lat=%llu/%llu/%lluus "
                     "read_lat=%llu/%llu/%lluus "
                     "query_lat=%llu/%llu/%lluus "
                     "bytes_written=%llu bytes_read=%llu query_results=%llu",
                     bridge->current_mode[0] ? bridge->current_mode : "?",
                     (unsigned long long)bridge->total_reads,
                     (unsigned long long)bridge->total_writes,
                     (unsigned long long)bridge->total_queries,
                     (unsigned long long)bridge->total_errors,
                     (unsigned long long)bridge->total_deletes,
                     (unsigned long long)bridge->total_evolves,
                     (unsigned long long)bridge->total_forgets,
                     (unsigned long long)bridge->total_mounts,
                     (unsigned long long)avg_write,
                     (unsigned long long)bridge->max_write_latency_us,
                     (unsigned long long)bridge->min_write_latency_us,
                     (unsigned long long)avg_read,
                     (unsigned long long)bridge->max_read_latency_us,
                     (unsigned long long)bridge->min_read_latency_us,
                     (unsigned long long)avg_query,
                     (unsigned long long)bridge->max_query_latency_us,
                     (unsigned long long)bridge->min_query_latency_us,
                     (unsigned long long)bridge->total_bytes_written,
                     (unsigned long long)bridge->total_bytes_read,
                     (unsigned long long)bridge->total_query_results);
}