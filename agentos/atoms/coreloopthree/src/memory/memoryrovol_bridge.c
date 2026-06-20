/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file memoryrovol_bridge.c
 * @brief C-L12: CoreLoopThree → MemoryRovol 提供商桥接实现
 *
 * 实现：
 *   P1.11.1: 加载 MemoryRovol → 填充 agentos_memory_provider_t
 *   P1.11.2: 提供商切换：内置 ↔ MemoryRovol（根据 agentos.yaml）
 *   P1.11.3: 三种构建模式支持
 *       - AGENTOS_MEMORY_BUILTIN: 仅内置提供商
 *       - AGENTOS_MEMORY_MEMORYROVOL: 仅 MemoryRovol
 *       - AGENTOS_MEMORY_HYBRID: 混合模式（配置切换 + 双向同步）
 *   P1.11.4: L1 写入 → L2 嵌入 → L3 实体绑定 → 检索 完整数据流
 */

#include "memoryrovol_bridge.h"

#include "memory_compat.h"
#include "platform.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

/* ==================== 构建模式检测 ==================== */

/* 默认行为：如果没有任何模式定义，使用 HYBRID */
#if !AGENTOS_MEMORY_BUILTIN && !AGENTOS_MEMORY_MEMORYROVOL && !AGENTOS_MEMORY_HYBRID
#undef AGENTOS_MEMORY_HYBRID
#define AGENTOS_MEMORY_HYBRID 1
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
    void *mr_ctx;                      /* MemoryRovol 上下文 */
    agentos_memory_provider_t *builtin; /* 内置提供商（HYBRID模式） */
    agentos_memory_provider_t *rovol;   /* Rovol 提供商 */
    agentos_memory_provider_t *active;  /* 当前活跃提供商 */
    mrb_provider_type_t active_type;
    mrb_config_t config;
    bool initialized;
    bool memoryrovol_available;
} mrb_bridge_impl_t;

struct memoryrovol_bridge_s {
    mrb_bridge_impl_t impl;
};

/* ==================== 辅助函数 ==================== */

/* 前向声明 */
static void mrb_bridge_destroy(mrb_bridge_t *bridge);

static bool is_memoryrovol_linked(void)
{
    /* 检查弱符号是否被实际链接 */
    return (memoryrovol_init != NULL);
}

/* ==================== MemoryRovol Provider 函数指针实现 ==================== */

#if AGENTOS_MEMORY_MEMORYROVOL || AGENTOS_MEMORY_HYBRID

static agentos_error_t mr_init(agentos_memory_provider_t *provider, const char *config_path)
{
    if (!provider || !provider->impl)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!impl->memoryrovol_available || !memoryrovol_init) {
        SVC_LOG_ERROR("C-L12: MemoryRovol not available");
        return AGENTOS_ENOTSUP;
    }

    const char *cfg = config_path ? config_path : impl->config.memoryrovol_config_path;
    int ret = memoryrovol_init(cfg, &impl->mr_ctx);
    if (ret != 0 || !impl->mr_ctx) {
        SVC_LOG_ERROR("C-L12: MemoryRovol init failed (ret=%d)", ret);
        return AGENTOS_EIO;
    }

    provider->capabilities.l1_raw = 1;
    provider->capabilities.l2_feature = 1;
    provider->capabilities.l3_structure = 1;
    provider->capabilities.l4_pattern = 1;
    provider->capabilities.forgetting = 1;
    provider->capabilities.faiss = 1;
    provider->capabilities.async_ops = 1;
    provider->capabilities.llm_integration = 1;

    SVC_LOG_INFO("C-L12: MemoryRovol provider initialized (ctx=%p)", impl->mr_ctx);
    return AGENTOS_SUCCESS;
}

static void mr_destroy(agentos_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;
    if (impl->mr_ctx && memoryrovol_shutdown) {
        memoryrovol_shutdown(impl->mr_ctx);
        impl->mr_ctx = NULL;
        SVC_LOG_INFO("C-L12: MemoryRovol provider destroyed");
    }
}

static agentos_error_t mr_write_raw(agentos_memory_provider_t *provider, const void *data,
                                    size_t len, const char *metadata_json, char **out_record_id)
{
    if (!provider || !provider->impl || !data || !out_record_id)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l1_write)
        return AGENTOS_ENOTSUP;

    int ret = memoryrovol_l1_write(impl->mr_ctx, data, len, metadata_json, out_record_id);
    if (ret != 0)
        return AGENTOS_EIO;

    /* P1.11.4: L1 写入后触发 L2 嵌入 */
    if (memoryrovol_l2_index && *out_record_id) {
        int l2_ret = memoryrovol_l2_index(impl->mr_ctx, *out_record_id, data, len, metadata_json);
        if (l2_ret != 0) {
            SVC_LOG_WARN("C-L12: L2 index failed for record %s (ret=%d)", *out_record_id, l2_ret);
        }
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t mr_get_raw(agentos_memory_provider_t *provider, const char *record_id,
                                  void **out_data, size_t *out_len)
{
    if (!provider || !provider->impl || !record_id || !out_data || !out_len)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l1_read)
        return AGENTOS_ENOTSUP;

    int ret = memoryrovol_l1_read(impl->mr_ctx, record_id, out_data, out_len);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

static agentos_error_t mr_delete_raw(agentos_memory_provider_t *provider, const char *record_id)
{
    if (!provider || !provider->impl || !record_id)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l1_delete)
        return AGENTOS_ENOTSUP;

    int ret = memoryrovol_l1_delete(impl->mr_ctx, record_id);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

static agentos_error_t mr_query(agentos_memory_provider_t *provider, const char *query_text,
                                uint32_t limit, char ***out_record_ids, float **out_scores,
                                size_t *out_count)
{
    if (!provider || !provider->impl || !query_text)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    /* P1.11.4: 检索优先使用 L2 向量索引 */
    if (memoryrovol_l2_search) {
        int ret = memoryrovol_l2_search(impl->mr_ctx, query_text, limit,
                                        out_record_ids, out_scores, out_count);
        if (ret == 0 && *out_count > 0)
            return AGENTOS_SUCCESS;
    }

    /* 降级：使用 L3 关系查询 */
    if (memoryrovol_l3_query_relations) {
        char *relations_json = NULL;
        int ret = memoryrovol_l3_query_relations(impl->mr_ctx, query_text, &relations_json);
        if (ret == 0 && relations_json) {
            /* 从关系 JSON 中提取记录 ID */
            *out_record_ids = (char **)AGENTOS_CALLOC(1, sizeof(char *));
            if (*out_record_ids) {
                (*out_record_ids)[0] = relations_json; /* 简化：直接返回关系JSON */
                *out_scores = (float *)AGENTOS_CALLOC(1, sizeof(float));
                if (*out_scores) (*out_scores)[0] = 1.0f;
                *out_count = 1;
                return AGENTOS_SUCCESS;
            }
            AGENTOS_FREE(relations_json);
        }
    }

    *out_record_ids = NULL;
    *out_scores = NULL;
    *out_count = 0;
    return AGENTOS_SUCCESS;
}

static agentos_error_t mr_retrieve(agentos_memory_provider_t *provider, const char *query_text,
                                   uint32_t limit, char ***out_record_ids, float **out_scores,
                                   size_t *out_count)
{
    /* retrieve 等同于 query（在 MemoryRovol 中语义相同） */
    return mr_query(provider, query_text, limit, out_record_ids, out_scores, out_count);
}

static agentos_error_t mr_evolve(agentos_memory_provider_t *provider, int force)
{
    if (!provider || !provider->impl)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l4_evolve)
        return AGENTOS_ENOTSUP;

    /* P1.11.4: L4 模式识别进化 */
    int ret = memoryrovol_l4_evolve(impl->mr_ctx, force);
    SVC_LOG_INFO("C-L12: MemoryRovol evolve %s (force=%d, ret=%d)",
                 ret == 0 ? "completed" : "failed", force, ret);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

static agentos_error_t mr_forget(agentos_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!memoryrovol_l4_forget)
        return AGENTOS_ENOTSUP;

    int ret = memoryrovol_l4_forget(impl->mr_ctx);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

static agentos_error_t mr_stats(agentos_memory_provider_t *provider, agentos_memory_stats_t *out_stats)
{
    if (!provider || !provider->impl || !out_stats)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    __builtin_memset(out_stats, 0, sizeof(*out_stats));
    snprintf(out_stats->provider_name, sizeof(out_stats->provider_name), "MemoryRovol");
    snprintf(out_stats->provider_version, sizeof(out_stats->provider_version), "2.0.0");

    if (memoryrovol_get_stats && impl->mr_ctx) {
        memoryrovol_get_stats(impl->mr_ctx, out_stats, sizeof(*out_stats));
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t mr_mount(agentos_memory_provider_t *provider, const char *record_id,
                                const char *context)
{
    if (!provider || !provider->impl || !record_id)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    /* P1.11.4: L3 实体绑定 */
    if (memoryrovol_l3_bind_entity && context) {
        int ret = memoryrovol_l3_bind_entity(impl->mr_ctx, record_id, "memory_record", context);
        if (ret != 0) {
            SVC_LOG_WARN("C-L12: L3 entity bind failed for %s (ret=%d)", record_id, ret);
        }
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t mr_health_check(agentos_memory_provider_t *provider, char **out_json)
{
    if (!provider || !provider->impl || !out_json)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (memoryrovol_health_check && impl->mr_ctx) {
        int ret = memoryrovol_health_check(impl->mr_ctx, out_json);
        return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
    }

    *out_json = AGENTOS_STRDUP("{\"status\":\"healthy\",\"provider\":\"memoryrovol\"}");
    return (*out_json) ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

static agentos_error_t mr_add_memory(agentos_memory_provider_t *provider, const char *content,
                                     size_t content_len)
{
    /* 简化实现：通过 write_raw 写入 */
    return mr_write_raw(provider, content, content_len, NULL, NULL);
}

static agentos_error_t mr_sync_push(agentos_memory_provider_t *provider, const char *record_id)
{
    if (!provider || !provider->impl)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!memoryrovol_sync_push)
        return AGENTOS_ENOTSUP;

    int ret = memoryrovol_sync_push(impl->mr_ctx, record_id);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

static agentos_error_t mr_sync_pull(agentos_memory_provider_t *provider, const char *filter_json,
                                    char ***out_record_ids, size_t *out_count)
{
    if (!provider || !provider->impl)
        return AGENTOS_EINVAL;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (!memoryrovol_sync_pull)
        return AGENTOS_ENOTSUP;

    int ret = memoryrovol_sync_pull(impl->mr_ctx, filter_json, out_record_ids, out_count);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

static int mr_has_active_sync(agentos_memory_provider_t *provider)
{
    if (!provider || !provider->impl)
        return 0;

    mrb_bridge_impl_t *impl = (mrb_bridge_impl_t *)provider->impl;

    if (memoryrovol_has_active_sync && impl->mr_ctx)
        return memoryrovol_has_active_sync(impl->mr_ctx);

    return 0;
}

#endif /* AGENTOS_MEMORY_MEMORYROVOL || AGENTOS_MEMORY_HYBRID */

/* ==================== MemoryRovol Provider 创建 ==================== */

#if AGENTOS_MEMORY_MEMORYROVOL || AGENTOS_MEMORY_HYBRID

static agentos_memory_provider_t *memoryrovol_provider_create(mrb_bridge_impl_t *impl)
{
    agentos_memory_provider_t *provider =
        (agentos_memory_provider_t *)AGENTOS_CALLOC(1, sizeof(agentos_memory_provider_t));
    if (!provider)
        return NULL;

    provider->name = "MemoryRovol";
    provider->version = "2.0.0";
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

    SVC_LOG_INFO("C-L12: MemoryRovol provider created (name=%s, version=%s)",
                 provider->name, provider->version);
    return provider;
}

#endif

/* ==================== 桥接实现 ==================== */

mrb_bridge_t *mrb_bridge_create(const mrb_config_t *config)
{
    mrb_bridge_t *bridge = (mrb_bridge_t *)AGENTOS_CALLOC(1, sizeof(mrb_bridge_t));
    if (!bridge) {
        SVC_LOG_ERROR("C-L12: mrb_bridge_create OOM");
        return NULL;
    }

    mrb_bridge_impl_t *impl = &bridge->impl;

    if (config) {
        impl->config = *config;
    } else {
        impl->config = (mrb_config_t)MRB_CONFIG_DEFAULTS;
    }

    /* 检测 MemoryRovol 可用性 */
    impl->memoryrovol_available = is_memoryrovol_linked();

    /* 根据构建模式创建提供商 */
#if AGENTOS_MEMORY_BUILTIN
    /* 仅内置模式 */
    impl->builtin = agentos_builtin_provider_create();
    if (!impl->builtin) {
        SVC_LOG_ERROR("C-L12: Failed to create builtin provider");
        AGENTOS_FREE(bridge);
        return NULL;
    }
    impl->active = impl->builtin;
    impl->active_type = MRB_PROVIDER_BUILTIN;
    SVC_LOG_INFO("C-L12: Builtin-only mode (no MemoryRovol)");

#elif AGENTOS_MEMORY_MEMORYROVOL
    /* 仅 MemoryRovol 模式 */
    if (!impl->memoryrovol_available) {
        SVC_LOG_ERROR("C-L12: MemoryRovol mode but library not linked");
        AGENTOS_FREE(bridge);
        return NULL;
    }
    impl->rovol = memoryrovol_provider_create(impl);
    if (!impl->rovol) {
        SVC_LOG_ERROR("C-L12: Failed to create MemoryRovol provider");
        AGENTOS_FREE(bridge);
        return NULL;
    }
    impl->active = impl->rovol;
    impl->active_type = MRB_PROVIDER_MEMORYROVOL;
    SVC_LOG_INFO("C-L12: MemoryRovol-only mode");

#else
    /* HYBRID 模式（默认） */
    /* 创建内置提供商 */
    impl->builtin = agentos_builtin_provider_create();
    if (!impl->builtin) {
        SVC_LOG_ERROR("C-L12: Failed to create builtin provider (hybrid mode)");
        AGENTOS_FREE(bridge);
        return NULL;
    }

    if (impl->memoryrovol_available) {
        /* MemoryRovol 可用 */
        impl->rovol = memoryrovol_provider_create(impl);
        if (impl->rovol) {
            /* 设置双向同步 */
            impl->builtin->sync_target = impl->rovol;
            impl->rovol->sync_target = impl->builtin;

            /* 根据配置选择活跃提供商 */
            if (impl->config.provider_type == MRB_PROVIDER_MEMORYROVOL) {
                impl->active = impl->rovol;
                impl->active_type = MRB_PROVIDER_MEMORYROVOL;
            } else {
                /* 默认使用内置，MemoryRovol 作为高级后端 */
                impl->active = impl->builtin;
                impl->active_type = MRB_PROVIDER_BUILTIN;
            }
            SVC_LOG_INFO("C-L12: Hybrid mode — builtin=%p, rovol=%p, active=%s, sync=%s",
                         (void *)impl->builtin, (void *)impl->rovol,
                         impl->active_type == MRB_PROVIDER_MEMORYROVOL ? "MemoryRovol" : "builtin",
                         impl->config.enable_sync ? "enabled" : "disabled");
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
        const char *cfg_path = impl->config.storage_path;
        agentos_error_t err = impl->active->init(impl->active, cfg_path);
        if (err != AGENTOS_SUCCESS) {
            SVC_LOG_ERROR("C-L12: Provider init failed (err=%d)", err);
            mrb_bridge_destroy(bridge);
            return NULL;
        }
    }

    SVC_LOG_INFO("C-L12: MemoryRovol bridge created (%s mode, memoryrovol=%s)",
#if AGENTOS_MEMORY_BUILTIN
                 "BUILTIN",
#elif AGENTOS_MEMORY_MEMORYROVOL
                 "MEMORYROVOL",
#else
                 "HYBRID",
#endif
                 impl->memoryrovol_available ? "available" : "N/A");

    return bridge;
}

void mrb_bridge_destroy(mrb_bridge_t *bridge)
{
    if (!bridge)
        return;

    mrb_bridge_impl_t *impl = &bridge->impl;

    if (impl->rovol) {
        if (impl->rovol->destroy)
            impl->rovol->destroy(impl->rovol);
        AGENTOS_FREE(impl->rovol);
        impl->rovol = NULL;
    }

    if (impl->builtin) {
        if (impl->builtin->destroy)
            impl->builtin->destroy(impl->builtin);
        AGENTOS_FREE(impl->builtin);
        impl->builtin = NULL;
    }

    impl->active = NULL;
    impl->initialized = false;

    AGENTOS_FREE(bridge);
    SVC_LOG_INFO("C-L12: MemoryRovol bridge destroyed");
}

/* ==================== 提供商获取 ==================== */

agentos_memory_provider_t *mrb_bridge_get_provider(mrb_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return NULL;
    return bridge->impl.active;
}

agentos_memory_provider_t *mrb_bridge_get_builtin(mrb_bridge_t *bridge)
{
    if (!bridge || !bridge->impl.initialized)
        return NULL;
    return bridge->impl.builtin;
}

/* ==================== 提供商切换 ==================== */

int mrb_bridge_switch_provider(mrb_bridge_t *bridge, mrb_provider_type_t type)
{
    if (!bridge || !bridge->impl.initialized)
        return -1;

    mrb_bridge_impl_t *impl = &bridge->impl;

    agentos_memory_provider_t *target = NULL;

    switch (type) {
    case MRB_PROVIDER_BUILTIN:
        target = impl->builtin;
        break;
    case MRB_PROVIDER_MEMORYROVOL:
        if (!impl->memoryrovol_available || !impl->rovol) {
            SVC_LOG_WARN("C-L12: Cannot switch to MemoryRovol — not available");
            return -1;
        }
        target = impl->rovol;
        break;
    case MRB_PROVIDER_HYBRID:
        /* HYBRID: 优先 MemoryRovol，降级到内置 */
        if (impl->memoryrovol_available && impl->rovol)
            target = impl->rovol;
        else
            target = impl->builtin;
        break;
    case MRB_PROVIDER_AUTO:
        /* AUTO: HYBRID模式下优先MemoryRovol，否则内置 */
        if (impl->memoryrovol_available && impl->rovol)
            target = impl->rovol;
        else
            target = impl->builtin;
        break;
    }

    if (!target) {
        SVC_LOG_ERROR("C-L12: Switch provider — no target available for type=%d", type);
        return -1;
    }

    if (target == impl->active) {
        SVC_LOG_DEBUG("C-L12: Provider already active (type=%d)", type);
        return 0;
    }

    /* 初始化目标提供商（如果尚未初始化） */
    if (target->init && target != impl->active) {
        agentos_error_t err = target->init(target, impl->config.storage_path);
        if (err != AGENTOS_SUCCESS) {
            SVC_LOG_ERROR("C-L12: Failed to init target provider (err=%d)", err);
            return -1;
        }
    }

    impl->active = target;
    impl->active_type = (target == impl->rovol) ? MRB_PROVIDER_MEMORYROVOL
                                                : MRB_PROVIDER_BUILTIN;

    SVC_LOG_INFO("C-L12: Provider switched to %s",
                 impl->active_type == MRB_PROVIDER_MEMORYROVOL ? "MemoryRovol" : "builtin");
    return 0;
}

int mrb_bridge_select_from_config(mrb_bridge_t *bridge, const char *config_yaml)
{
    if (!bridge || !config_yaml)
        return -1;

    __attribute__((unused))
    mrb_bridge_impl_t *impl = &bridge->impl;

    /* 解析 memory.provider 配置 */
    if (strstr(config_yaml, "\"provider\":\"memoryrovol\"") ||
        strstr(config_yaml, "provider: memoryrovol")) {
        SVC_LOG_INFO("C-L12: Config selects MemoryRovol provider");
        return mrb_bridge_switch_provider(bridge, MRB_PROVIDER_MEMORYROVOL);
    } else if (strstr(config_yaml, "\"provider\":\"builtin\"") ||
               strstr(config_yaml, "provider: builtin")) {
        SVC_LOG_INFO("C-L12: Config selects builtin provider");
        return mrb_bridge_switch_provider(bridge, MRB_PROVIDER_BUILTIN);
    } else {
        SVC_LOG_INFO("C-L12: Config has no explicit provider, using AUTO");
        return mrb_bridge_switch_provider(bridge, MRB_PROVIDER_AUTO);
    }
}

/* ==================== 状态查询 ==================== */

mrb_provider_type_t mrb_bridge_get_active_type(mrb_bridge_t *bridge)
{
    if (!bridge)
        return MRB_PROVIDER_BUILTIN;
    return bridge->impl.active_type;
}

bool mrb_bridge_is_memoryrovol_available(mrb_bridge_t *bridge)
{
    if (!bridge)
        return false;
    return bridge->impl.memoryrovol_available && bridge->impl.rovol != NULL;
}

bool mrb_bridge_is_healthy(mrb_bridge_t *bridge)
{
    return bridge && bridge->impl.initialized && bridge->impl.active != NULL;
}

int mrb_bridge_get_stats(mrb_bridge_t *bridge, agentos_memory_stats_t *stats)
{
    if (!bridge || !stats)
        return -1;

    mrb_bridge_impl_t *impl = &bridge->impl;

    if (impl->active && impl->active->stats) {
        return (int)impl->active->stats(impl->active, stats);
    }

    __builtin_memset(stats, 0, sizeof(*stats));
    return 0;
}