/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file memoryrovol_bridge.c
 * @brief C-L12: CoreLoopThree → MemoryRovol 提供商桥接实现
 *
 * 实现：
 *   P1.11.1: 加载 MemoryRovol → 填充 agentrt_memory_provider_t
 *   P1.11.2: 提供商切换：内置 ↔ MemoryRovol（根据 agentrt.yaml）
 *   P1.11.3: 三种构建模式支持
 *       - AGENTRT_MEMORY_BUILTIN: 仅内置提供商
 *       - AGENTRT_MEMORY_MEMORYROVOL: 仅 MemoryRovol
 *       - AGENTRT_MEMORY_HYBRID: 混合模式（配置切换 + 双向同步）
 *   P1.11.4: L1 写入 → L2 嵌入 → L3 实体绑定 → 检索 完整数据流
 *
 * 公共 API 完整实现头文件 memoryrovol_bridge.h 声明的全部接口，
 * 消除命名不一致技术债（原 mrb_bridge_* 内部命名已对齐为
 * memoryrovol_bridge_* 公共命名）。
 */

#include "memoryrovol_bridge.h"

#include "memory_compat.h"
#include "platform.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#ifdef AGENTRT_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* ==================== 构建模式检测 ==================== */

/* 默认行为：如果没有任何模式定义，使用 HYBRID */
#if !AGENTRT_MEMORY_BUILTIN && !AGENTRT_MEMORY_MEMORYROVOL && !AGENTRT_MEMORY_HYBRID
#undef AGENTRT_MEMORY_HYBRID
#define AGENTRT_MEMORY_HYBRID 1
#endif

/* ==================== MemoryRovol 外部函数声明（弱符号） ==================== */

/*
 * MemoryRovol 库提供的 API。使用弱符号链接，在 MemoryRovol 不可用时
 * 这些函数返回 NULL 或错误，由桥接层优雅降级到内置提供商。
 */

/* MemoryRovol 初始化/销毁 */
extern int memoryrovol_init(const char *config_path, void **out_ctx)
    __attribute__((weak));
extern void memoryrovol_shutdown(void *ctx)
    __attribute__((weak));

/* L1: 原始数据写入/读取/删除 */
extern int memoryrovol_l1_write(void *ctx, const void *data, size_t len,
                                const char *metadata_json, char **out_record_id)
    __attribute__((weak));
extern int memoryrovol_l1_read(void *ctx, const char *record_id, void **out_data,
                               size_t *out_len)
    __attribute__((weak));
extern int memoryrovol_l1_delete(void *ctx, const char *record_id)
    __attribute__((weak));

/* L2: 特征提取/向量索引 */
extern int memoryrovol_l2_index(void *ctx, const char *record_id, const void *data,
                                size_t len, const char *metadata_json)
    __attribute__((weak));
extern int memoryrovol_l2_search(void *ctx, const char *query_text, uint32_t limit,
                                 char ***out_ids, float **out_scores, size_t *out_count)
    __attribute__((weak));

/* L3: 结构绑定/知识图谱 */
extern int memoryrovol_l3_bind_entity(void *ctx, const char *record_id,
                                      const char *entity_type, const char *relations_json)
    __attribute__((weak));
extern int memoryrovol_l3_query_relations(void *ctx, const char *entity_id,
                                          char **out_json)
    __attribute__((weak));

/* L4: 模式识别/进化 */
extern int memoryrovol_l4_evolve(void *ctx, int force)
    __attribute__((weak));
extern int memoryrovol_l4_forget(void *ctx)
    __attribute__((weak));

/* 统计/健康检查 */
extern int memoryrovol_get_stats(void *ctx, void *out_stats, size_t stats_size)
    __attribute__((weak));
extern int memoryrovol_health_check(void *ctx, char **out_json)
    __attribute__((weak));

/* 同步操作（HYBRID模式） */
extern int memoryrovol_sync_push(void *ctx, const char *record_id)
    __attribute__((weak));
extern int memoryrovol_sync_pull(void *ctx, const char *filter_json, char ***out_ids,
                                 size_t *out_count)
    __attribute__((weak));
extern int memoryrovol_has_active_sync(void *ctx)
    __attribute__((weak));

/* ==================== 内部结构 ==================== */

typedef struct {
    void *mr_ctx;                              /* MemoryRovol 上下文 */
    agentrt_memory_provider_t *builtin;        /* 内置提供商（HYBRID模式） */
    agentrt_memory_provider_t *rovol;          /* Rovol 提供商 */
    agentrt_memory_provider_t *active;         /* 当前活跃提供商 */
    mrb_provider_type_t active_type;
    memoryrovol_bridge_config_t config;        /* 完整配置副本 */
    bool initialized;
    bool memoryrovol_available;
    bool sync_active;                          /* 同步是否活跃 */
} memoryrovol_bridge_impl_t;

struct memoryrovol_bridge_s {
    memoryrovol_bridge_impl_t impl;
};

/* ==================== 辅助函数 ==================== */

/* 前向声明 */
static void mrb_bridge_destroy_internal(memoryrovol_bridge_t *bridge);

static bool is_memoryrovol_linked(void)
{
    /* 检查弱符号是否被实际链接 */
    return (memoryrovol_init != NULL);
}

/* 从配置提取能力位掩码 */
static void apply_capability_flags(agentrt_memory_provider_t *provider,
                                   const memoryrovol_bridge_config_t *cfg)
{
    if (!provider || !cfg)
        return;
    provider->capabilities.l1_raw = cfg->enable_l1_raw ? 1 : 0;
    provider->capabilities.l2_feature = cfg->enable_l2_feature ? 1 : 0;
    provider->capabilities.l3_structure = cfg->enable_l3_structure ? 1 : 0;
    provider->capabilities.l4_pattern = cfg->enable_l4_pattern ? 1 : 0;
    provider->capabilities.forgetting = cfg->enable_forgetting ? 1 : 0;
    provider->capabilities.attractor = cfg->enable_attractor ? 1 : 0;
    provider->capabilities.persistence = cfg->enable_persistence ? 1 : 0;
    provider->capabilities.faiss = cfg->enable_faiss ? 1 : 0;
    provider->capabilities.async_ops = cfg->enable_async_ops ? 1 : 0;
    provider->capabilities.llm_integration = cfg->enable_llm_integration ? 1 : 0;
}

/* ==================== MemoryRovol Provider 函数指针实现 ==================== */

#if AGENTRT_MEMORY_MEMORYROVOL || AGENTRT_MEMORY_HYBRID

static agentrt_error_t mr_init(agentrt_memory_provider_t *provider, const char *config_path)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!impl->memoryrovol_available || !memoryrovol_init) {
        SVC_LOG_ERROR("C-L12: MemoryRovol not available");
        return AGENTRT_ENOTSUP;
    }

    const char *cfg = config_path ? config_path : impl->config.config_path;
    int ret = memoryrovol_init(cfg, &impl->mr_ctx);
    if (ret != 0 || !impl->mr_ctx) {
        SVC_LOG_ERROR("C-L12: MemoryRovol init failed (ret=%d)", ret);
        return AGENTRT_EIO;
    }

    /* 根据配置能力标记填充 capabilities */
    apply_capability_flags(provider, &impl->config);

    SVC_LOG_INFO("C-L12: MemoryRovol provider initialized (ctx=%p)", impl->mr_ctx);
    return AGENTRT_SUCCESS;
}

static void mr_destroy(agentrt_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;
    if (impl->mr_ctx && memoryrovol_shutdown) {
        memoryrovol_shutdown(impl->mr_ctx);
        impl->mr_ctx = NULL;
        SVC_LOG_INFO("C-L12: MemoryRovol provider destroyed");
    }
}

static agentrt_error_t mr_write_raw(agentrt_memory_provider_t *provider, const void *data,
                                    size_t len, const char *metadata_json, char **out_record_id)
{
    if (!provider || !provider->impl || !data || !out_record_id)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l1_write)
        return AGENTRT_ENOTSUP;

    int ret = memoryrovol_l1_write(impl->mr_ctx, data, len, metadata_json, out_record_id);
    if (ret != 0)
        return AGENTRT_EIO;

    /* P1.11.4: L1 写入后触发 L2 嵌入 */
    if (impl->config.enable_l2_feature && memoryrovol_l2_index && *out_record_id) {
        int l2_ret = memoryrovol_l2_index(impl->mr_ctx, *out_record_id, data, len, metadata_json);
        if (l2_ret != 0) {
            SVC_LOG_WARN("C-L12: L2 index failed for record %s (ret=%d)", *out_record_id, l2_ret);
        }
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t mr_get_raw(agentrt_memory_provider_t *provider, const char *record_id,
                                  void **out_data, size_t *out_len)
{
    if (!provider || !provider->impl || !record_id || !out_data || !out_len)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l1_read)
        return AGENTRT_ENOTSUP;

    int ret = memoryrovol_l1_read(impl->mr_ctx, record_id, out_data, out_len);
    return (ret == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
}

static agentrt_error_t mr_delete_raw(agentrt_memory_provider_t *provider, const char *record_id)
{
    if (!provider || !provider->impl || !record_id)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l1_delete)
        return AGENTRT_ENOTSUP;

    int ret = memoryrovol_l1_delete(impl->mr_ctx, record_id);
    return (ret == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
}

#ifdef AGENTRT_HAS_CJSON
/* 从 L3 关系查询返回的 JSON 中提取记录 ID。
 *
 * 防御性解析，兼容多种合理 schema：
 *   {"relations":[{"to_id":"id1","from_id":"id2"}, ...]}
 *   {"record_ids":["id1","id2",...]} / {"ids":[...]}
 *   ["id1","id2",...]（顶层字符串数组）
 * 提取 to_id/from_id/id/record_id 字段值与字符串数组元素，
 * 去重后填充 *out_ids（受 limit 约束）。返回提取的数量。
 * 调用方拥有 *out_ids 及其中每个字符串的所有权。失败/无匹配返回 0。
 */
static size_t extract_ids_from_relations_json(const char *json, uint32_t limit,
                                               char ***out_ids)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return 0;

    cJSON *arr = NULL;
    if (cJSON_IsArray(root)) {
        arr = root;
    } else if (cJSON_IsObject(root)) {
        arr = cJSON_GetObjectItem(root, "relations");
        if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "record_ids");
        if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "ids");
    }

    uint32_t id_limit = (limit > 0) ? limit : 32;
    size_t cap = (id_limit < 256) ? id_limit : 256;
    if (cap == 0) cap = 8;
    char **ids = (char **)AGENTRT_CALLOC(cap, sizeof(char *));
    if (!ids) { cJSON_Delete(root); return 0; }

    size_t count = 0;
    if (arr) {
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (count >= id_limit) break;
            const char *id_str = NULL;
            if (cJSON_IsString(item)) {
                id_str = item->valuestring;
            } else if (cJSON_IsObject(item)) {
                cJSON *f = cJSON_GetObjectItem(item, "to_id");
                if (!cJSON_IsString(f)) f = cJSON_GetObjectItem(item, "from_id");
                if (!cJSON_IsString(f)) f = cJSON_GetObjectItem(item, "id");
                if (!cJSON_IsString(f)) f = cJSON_GetObjectItem(item, "record_id");
                if (cJSON_IsString(f)) id_str = f->valuestring;
            }
            if (id_str && id_str[0]) {
                bool dup = false;
                for (size_t i = 0; i < count; i++) {
                    if (strcmp(ids[i], id_str) == 0) { dup = true; break; }
                }
                if (!dup && count < cap) {
                    ids[count] = AGENTRT_STRDUP(id_str);
                    if (!ids[count]) break;
                    count++;
                }
            }
        }
    }

    cJSON_Delete(root);

    if (count == 0) {
        AGENTRT_FREE(ids);
        return 0;
    }
    *out_ids = ids;
    return count;
}
#endif /* AGENTRT_HAS_CJSON */

static agentrt_error_t mr_query(agentrt_memory_provider_t *provider, const char *query_text,
                                uint32_t limit, char ***out_record_ids, float **out_scores,
                                size_t *out_count)
{
    if (!provider || !provider->impl || !query_text)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    /* 应用配置默认 limit */
    if (limit == 0 && impl->config.query_default_limit > 0)
        limit = impl->config.query_default_limit;

    /* P1.11.4: 检索优先使用 L2 向量索引 */
    if (impl->config.enable_l2_feature && memoryrovol_l2_search) {
        int ret = memoryrovol_l2_search(impl->mr_ctx, query_text, limit,
                                        out_record_ids, out_scores, out_count);
        if (ret == 0 && *out_count > 0)
            return AGENTRT_SUCCESS;
    }

    /* 降级：使用 L3 关系查询，从返回的关系 JSON 中提取真实记录 ID */
    if (impl->config.enable_l3_structure && memoryrovol_l3_query_relations) {
        char *relations_json = NULL;
        int ret = memoryrovol_l3_query_relations(impl->mr_ctx, query_text, &relations_json);
        if (ret == 0 && relations_json) {
#ifdef AGENTRT_HAS_CJSON
            char **ids = NULL;
            size_t id_count = extract_ids_from_relations_json(relations_json, limit, &ids);
            AGENTRT_FREE(relations_json);
            if (id_count > 0 && ids) {
                *out_record_ids = ids;
                *out_scores = (float *)AGENTRT_CALLOC(id_count, sizeof(float));
                if (*out_scores) {
                    for (size_t i = 0; i < id_count; i++) (*out_scores)[i] = 1.0f;
                }
                *out_count = id_count;
                return AGENTRT_SUCCESS;
            }
            /* JSON 解析未提取到 ID：诚实返回空结果 */
#else
            AGENTRT_FREE(relations_json);
#endif
        }
    }

    *out_record_ids = NULL;
    *out_scores = NULL;
    *out_count = 0;
    return AGENTRT_SUCCESS;
}

static agentrt_error_t mr_retrieve(agentrt_memory_provider_t *provider, const char *query_text,
                                   uint32_t limit, char ***out_record_ids, float **out_scores,
                                   size_t *out_count)
{
    /* retrieve 等同于 query（在 MemoryRovol 中语义相同） */
    return mr_query(provider, query_text, limit, out_record_ids, out_scores, out_count);
}

static agentrt_error_t mr_evolve(agentrt_memory_provider_t *provider, int force)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!impl->config.enable_l4_pattern || !memoryrovol_l4_evolve)
        return AGENTRT_ENOTSUP;

    /* P1.11.4: L4 模式识别进化 */
    int ret = memoryrovol_l4_evolve(impl->mr_ctx, force);
    SVC_LOG_INFO("C-L12: MemoryRovol evolve %s (force=%d, ret=%d)",
                 ret == 0 ? "completed" : "failed", force, ret);
    return (ret == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
}

static agentrt_error_t mr_forget(agentrt_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!impl->config.enable_forgetting || !memoryrovol_l4_forget)
        return AGENTRT_ENOTSUP;

    int ret = memoryrovol_l4_forget(impl->mr_ctx);
    return (ret == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
}

static agentrt_error_t mr_stats(agentrt_memory_provider_t *provider, agentrt_memory_stats_t *out_stats)
{
    if (!provider || !provider->impl || !out_stats)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    __builtin_memset(out_stats, 0, sizeof(*out_stats));
    snprintf(out_stats->provider_name, sizeof(out_stats->provider_name),
             "%s", impl->config.provider_name ? impl->config.provider_name : "MemoryRovol");
    snprintf(out_stats->provider_version, sizeof(out_stats->provider_version),
             "%s", impl->config.provider_version ? impl->config.provider_version : "2.0.0");

    if (memoryrovol_get_stats && impl->mr_ctx) {
        memoryrovol_get_stats(impl->mr_ctx, out_stats, sizeof(*out_stats));
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t mr_mount(agentrt_memory_provider_t *provider, const char *record_id,
                                const char *context)
{
    if (!provider || !provider->impl || !record_id)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    /* P1.11.4: L3 实体绑定 */
    if (impl->config.enable_l3_structure && memoryrovol_l3_bind_entity && context) {
        int ret = memoryrovol_l3_bind_entity(impl->mr_ctx, record_id, "memory_record", context);
        if (ret != 0) {
            SVC_LOG_WARN("C-L12: L3 entity bind failed for %s (ret=%d)", record_id, ret);
        }
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t mr_health_check(agentrt_memory_provider_t *provider, char **out_json)
{
    if (!provider || !provider->impl || !out_json)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (memoryrovol_health_check && impl->mr_ctx) {
        int ret = memoryrovol_health_check(impl->mr_ctx, out_json);
        return (ret == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
    }

    /* health_check 弱符号未链接或上下文未初始化：无法执行实际健康验证，
     * 诚实报告 degraded 状态而非虚假声称 healthy。 */
    const char *reason = (!impl->mr_ctx)
        ? "context not initialized"
        : "native health_check unavailable (MemoryRovol provider not linked)";
    char json_buf[192];
    snprintf(json_buf, sizeof(json_buf),
             "{\"status\":\"degraded\",\"provider\":\"memoryrovol\",\"reason\":\"%s\"}",
             reason);
    *out_json = AGENTRT_STRDUP(json_buf);
    return (*out_json) ? AGENTRT_SUCCESS : AGENTRT_ENOMEM;
}

static agentrt_error_t mr_add_memory(agentrt_memory_provider_t *provider, const char *content,
                                     size_t content_len)
{
    /* 简化实现：通过 write_raw 写入 */
    return mr_write_raw(provider, content, content_len, NULL, NULL);
}

static agentrt_error_t mr_sync_push(agentrt_memory_provider_t *provider, const char *record_id)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!memoryrovol_sync_push)
        return AGENTRT_ENOTSUP;

    int ret = memoryrovol_sync_push(impl->mr_ctx, record_id);
    return (ret == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
}

static agentrt_error_t mr_sync_pull(agentrt_memory_provider_t *provider, const char *filter_json,
                                    char ***out_record_ids, size_t *out_count)
{
    if (!provider || !provider->impl)
        return AGENTRT_EINVAL;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (!memoryrovol_sync_pull)
        return AGENTRT_ENOTSUP;

    int ret = memoryrovol_sync_pull(impl->mr_ctx, filter_json, out_record_ids, out_count);
    return (ret == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
}

static int mr_has_active_sync(agentrt_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return 0;

    memoryrovol_bridge_impl_t *impl = (memoryrovol_bridge_impl_t *)provider->impl;

    if (memoryrovol_has_active_sync && impl->mr_ctx)
        return memoryrovol_has_active_sync(impl->mr_ctx);

    return 0;
}

#endif /* AGENTRT_MEMORY_MEMORYROVOL || AGENTRT_MEMORY_HYBRID */

/* ==================== MemoryRovol Provider 创建 ==================== */

#if AGENTRT_MEMORY_MEMORYROVOL || AGENTRT_MEMORY_HYBRID

static agentrt_memory_provider_t *memoryrovol_provider_create(memoryrovol_bridge_impl_t *impl)
{
    agentrt_memory_provider_t *provider =
        (agentrt_memory_provider_t *)AGENTRT_CALLOC(1, sizeof(agentrt_memory_provider_t));
    if (!provider)
        return NULL;

    /* 使用配置中的名称/版本，回退到默认 */
    provider->name = impl->config.provider_name ? impl->config.provider_name : "MemoryRovol";
    provider->version = impl->config.provider_version ? impl->config.provider_version : "2.0.0";
    provider->impl = impl;

    /* 填充函数指针表 */
    provider->init = mr_init;
    provider->destroy = mr_destroy;
    provider->write_raw = mr_write_raw;
    provider->get_raw = mr_get_raw;
    provider->delete_raw = mr_delete_raw;
    provider->query = mr_query;
    provider->retrieve = mr_retrieve;
    provider->evolve = mr_evolve;
    provider->forget = mr_forget;
    provider->stats = mr_stats;
    provider->mount = mr_mount;
    provider->health_check = mr_health_check;
    provider->add_memory = mr_add_memory;
    provider->sync_push = mr_sync_push;
    provider->sync_pull = mr_sync_pull;
    provider->has_active_sync = mr_has_active_sync;

    /* 根据配置能力标记填充 capabilities（init 后会再次确认） */
    apply_capability_flags(provider, &impl->config);

    SVC_LOG_INFO("C-L12: MemoryRovol provider created (name=%s, version=%s)",
                 provider->name, provider->version);
    return provider;
}

#endif

/* ==================== 桥接实现：公共 API ==================== */

memoryrovol_bridge_t *memoryrovol_bridge_create(const memoryrovol_bridge_config_t *config)
{
    memoryrovol_bridge_t *bridge =
        (memoryrovol_bridge_t *)AGENTRT_CALLOC(1, sizeof(memoryrovol_bridge_t));
    if (!bridge) {
        SVC_LOG_ERROR("C-L12: memoryrovol_bridge_create OOM");
        return NULL;
    }

    memoryrovol_bridge_impl_t *impl = &bridge->impl;

    /* 存储完整配置副本（NULL 使用零初始化默认值） */
    if (config) {
        impl->config = *config;
    } else {
        __builtin_memset(&impl->config, 0, sizeof(impl->config));
    }

    /* 检测 MemoryRovol 可用性 */
    impl->memoryrovol_available = is_memoryrovol_linked();

    /* 根据构建模式创建提供商 */
#if AGENTRT_MEMORY_BUILTIN
    /* 仅内置模式 */
    impl->builtin = agentrt_builtin_provider_create();
    if (!impl->builtin) {
        SVC_LOG_ERROR("C-L12: Failed to create builtin provider");
        AGENTRT_FREE(bridge);
        return NULL;
    }
    impl->active = impl->builtin;
    impl->active_type = MRB_PROVIDER_BUILTIN;
    SVC_LOG_INFO("C-L12: Builtin-only mode (no MemoryRovol)");

#elif AGENTRT_MEMORY_MEMORYROVOL
    /* 仅 MemoryRovol 模式 */
    if (!impl->memoryrovol_available) {
        SVC_LOG_ERROR("C-L12: MemoryRovol mode but library not linked");
        AGENTRT_FREE(bridge);
        return NULL;
    }
    impl->rovol = memoryrovol_provider_create(impl);
    if (!impl->rovol) {
        SVC_LOG_ERROR("C-L12: Failed to create MemoryRovol provider");
        AGENTRT_FREE(bridge);
        return NULL;
    }
    impl->active = impl->rovol;
    impl->active_type = MRB_PROVIDER_MEMORYROVOL;
    SVC_LOG_INFO("C-L12: MemoryRovol-only mode");

#else
    /* HYBRID 模式（默认） */
    /* 创建内置提供商 */
    impl->builtin = agentrt_builtin_provider_create();
    if (!impl->builtin) {
        SVC_LOG_ERROR("C-L12: Failed to create builtin provider (hybrid mode)");
        AGENTRT_FREE(bridge);
        return NULL;
    }

    if (impl->memoryrovol_available) {
        /* MemoryRovol 可用 */
        impl->rovol = memoryrovol_provider_create(impl);
        if (impl->rovol) {
            /* 设置双向同步链路（sync_active 控制是否启用） */
            impl->builtin->sync_target = impl->rovol;
            impl->rovol->sync_target = impl->builtin;

            /* 根据配置选择活跃提供商 */
            if (impl->config.provider_name &&
                strcmp(impl->config.provider_name, "memoryrovol") == 0) {
                impl->active = impl->rovol;
                impl->active_type = MRB_PROVIDER_MEMORYROVOL;
            } else {
                /* 默认使用内置，MemoryRovol 作为高级后端 */
                impl->active = impl->builtin;
                impl->active_type = MRB_PROVIDER_BUILTIN;
            }
            SVC_LOG_INFO("C-L12: Hybrid mode — builtin=%p, rovol=%p, active=%s",
                         (void *)impl->builtin, (void *)impl->rovol,
                         impl->active_type == MRB_PROVIDER_MEMORYROVOL ? "MemoryRovol" : "builtin");
        } else {
            SVC_LOG_WARN("C-L12: Hybrid mode — MemoryRovol provider creation failed, "
                         "falling back to builtin");
            impl->active = impl->builtin;
            impl->active_type = MRB_PROVIDER_BUILTIN;
        }
    } else {
        /* MemoryRovol 不可用，降级到内置 */
        impl->active = impl->builtin;
        impl->active_type = MRB_PROVIDER_BUILTIN;
        SVC_LOG_INFO("C-L12: Hybrid mode — MemoryRovol not available, "
                     "using builtin provider only");
    }
#endif

    impl->initialized = true;

    /* 初始化活跃提供商 */
    if (impl->active && impl->active->init) {
        const char *cfg_path = impl->config.config_path;
        agentrt_error_t err = impl->active->init(impl->active, cfg_path);
        if (err != AGENTRT_SUCCESS) {
            SVC_LOG_ERROR("C-L12: Provider init failed (err=%d)", err);
            mrb_bridge_destroy_internal(bridge);
            return NULL;
        }
    }

    {
        const char *mode =
#if AGENTRT_MEMORY_BUILTIN
            "BUILTIN";
#elif AGENTRT_MEMORY_MEMORYROVOL
            "MEMORYROVOL";
#else
            "HYBRID";
#endif
        SVC_LOG_INFO("C-L12: MemoryRovol bridge created (%s mode, memoryrovol=%s)",
                     mode, impl->memoryrovol_available ? "available" : "N/A");
    }

    return bridge;
}

static void mrb_bridge_destroy_internal(memoryrovol_bridge_t *bridge)
{
    if (!bridge)
        return;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;

    if (impl->rovol) {
        if (impl->rovol->destroy)
            impl->rovol->destroy(impl->rovol);
        AGENTRT_FREE(impl->rovol);
        impl->rovol = NULL;
    }

    if (impl->builtin) {
        if (impl->builtin->destroy)
            impl->builtin->destroy(impl->builtin);
        AGENTRT_FREE(impl->builtin);
        impl->builtin = NULL;
    }

    impl->active = NULL;
    impl->initialized = false;
    impl->sync_active = false;
}

void memoryrovol_bridge_destroy(memoryrovol_bridge_t *bridge)
{
    if (!bridge)
        return;

    mrb_bridge_destroy_internal(bridge);
    AGENTRT_FREE(bridge);
    SVC_LOG_INFO("C-L12: MemoryRovol bridge destroyed");
}

/* ==================== 提供商获取 ==================== */

agentrt_memory_provider_t *memoryrovol_bridge_get_provider(memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return NULL;
    return bridge->impl.active;
}

/* ==================== 提供商切换 ==================== */

int memoryrovol_bridge_switch_mode(memoryrovol_bridge_t *bridge, const char *mode)
{
    if (!bridge || !bridge->impl.initialized || !mode)
        return -1;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;
    agentrt_memory_provider_t *target = NULL;
    mrb_provider_type_t target_type = MRB_PROVIDER_BUILTIN;

    if (strcmp(mode, "builtin") == 0) {
        target = impl->builtin;
        target_type = MRB_PROVIDER_BUILTIN;
    } else if (strcmp(mode, "memoryrovol") == 0) {
        if (!impl->memoryrovol_available || !impl->rovol) {
            SVC_LOG_WARN("C-L12: Cannot switch to MemoryRovol — not available");
            return -1;
        }
        target = impl->rovol;
        target_type = MRB_PROVIDER_MEMORYROVOL;
    } else if (strcmp(mode, "hybrid") == 0) {
        /* HYBRID: 优先 MemoryRovol，降级到内置 */
        if (impl->memoryrovol_available && impl->rovol) {
            target = impl->rovol;
            target_type = MRB_PROVIDER_MEMORYROVOL;
        } else {
            target = impl->builtin;
            target_type = MRB_PROVIDER_BUILTIN;
        }
    } else {
        SVC_LOG_ERROR("C-L12: Unknown mode '%s'", mode);
        return -1;
    }

    if (!target) {
        SVC_LOG_ERROR("C-L12: Switch mode — no target available for mode=%s", mode);
        return -1;
    }

    if (target == impl->active) {
        SVC_LOG_DEBUG("C-L12: Provider already active (mode=%s)", mode);
        return 0;
    }

    /* 初始化目标提供商（如果尚未初始化） */
    if (target->init && target != impl->active) {
        agentrt_error_t err = target->init(target, impl->config.config_path);
        if (err != AGENTRT_SUCCESS) {
            SVC_LOG_ERROR("C-L12: Failed to init target provider (err=%d)", err);
            return -1;
        }
    }

    impl->active = target;
    impl->active_type = target_type;

    SVC_LOG_INFO("C-L12: Provider switched to %s",
                 impl->active_type == MRB_PROVIDER_MEMORYROVOL ? "MemoryRovol" : "builtin");
    return 0;
}

const char *memoryrovol_bridge_get_mode(memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return NULL;

    switch (bridge->impl.active_type) {
    case MRB_PROVIDER_MEMORYROVOL:
        return "memoryrovol";
    case MRB_PROVIDER_BUILTIN:
        return "builtin";
    case MRB_PROVIDER_HYBRID:
        return "hybrid";
    default:
        return "unknown";
    }
}

/* ==================== 同步控制 ==================== */

int memoryrovol_bridge_start_sync(memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return -1;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;

    /* HYBRID 模式下需要两个提供商都存在才能同步 */
    if (!impl->builtin || !impl->rovol) {
        SVC_LOG_WARN("C-L12: Cannot start sync — hybrid providers not available");
        return -1;
    }

    /* 连接双向同步链路 */
    impl->builtin->sync_target = impl->rovol;
    impl->rovol->sync_target = impl->builtin;
    impl->sync_active = true;

    SVC_LOG_INFO("C-L12: Sync started (interval=%ums)",
                 impl->config.sync_interval_ms);
    return 0;
}

void memoryrovol_bridge_stop_sync(memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;

    /* 断开同步链路 */
    if (impl->builtin)
        impl->builtin->sync_target = NULL;
    if (impl->rovol)
        impl->rovol->sync_target = NULL;

    impl->sync_active = false;
    SVC_LOG_INFO("C-L12: Sync stopped");
}

bool memoryrovol_bridge_has_active_sync(memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return false;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;

    /* 检查本地标志 + MemoryRovol 库的同步状态 */
    if (impl->sync_active) {
        if (impl->active && impl->active->has_active_sync) {
            return impl->active->has_active_sync(impl->active) != 0;
        }
        return true;
    }
    return false;
}

/* ==================== 状态查询 ==================== */

int memoryrovol_bridge_get_stats(memoryrovol_bridge_t *bridge,
                                 agentrt_memory_stats_t *out_stats)
{
    if (!bridge || !out_stats)
        return -1;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;

    if (impl->active && impl->active->stats) {
        agentrt_error_t err = impl->active->stats(impl->active, out_stats);
        return (err == AGENTRT_SUCCESS) ? 0 : -1;
    }

    __builtin_memset(out_stats, 0, sizeof(*out_stats));
    return 0;
}

int memoryrovol_bridge_health_check(memoryrovol_bridge_t *bridge, char **out_json)
{
    if (!bridge || !out_json)
        return -1;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;

    if (impl->active && impl->active->health_check) {
        agentrt_error_t err = impl->active->health_check(impl->active, out_json);
        return (err == AGENTRT_SUCCESS) ? 0 : -1;
    }

    *out_json = AGENTRT_STRDUP("{\"status\":\"unknown\",\"provider\":\"none\"}");
    return (*out_json) ? 0 : -1;
}

bool memoryrovol_bridge_is_ready(memoryrovol_bridge_t *bridge)
{
    return bridge && bridge->impl.initialized && bridge->impl.active != NULL;
}

void memoryrovol_bridge_dump_stats(memoryrovol_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return;

    memoryrovol_bridge_impl_t *impl = &bridge->impl;
    agentrt_memory_stats_t stats;
    __builtin_memset(&stats, 0, sizeof(stats));

    if (impl->active && impl->active->stats) {
        impl->active->stats(impl->active, &stats);
    }

    const char *mode = memoryrovol_bridge_get_mode(bridge);
    SVC_LOG_INFO("C-L12: BRIDGE-STATS mode=%s provider=%s sync=%s "
                 "records=%llu bytes=%llu l1=%llu l2=%llu l3=%llu l4=%llu util=%.2f",
                 mode ? mode : "?",
                 stats.provider_name[0] ? stats.provider_name : "?",
                 impl->sync_active ? "on" : "off",
                 (unsigned long long)stats.total_records,
                 (unsigned long long)stats.total_bytes,
                 (unsigned long long)stats.l1_count,
                 (unsigned long long)stats.l2_indexed,
                 (unsigned long long)stats.l3_relations,
                 (unsigned long long)stats.l4_patterns,
                 stats.index_utilization);
}
