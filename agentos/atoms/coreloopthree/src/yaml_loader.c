/**
 * @file yaml_loader.c
 * @brief agentos.yaml 解析器实现 — 基于 cupolas/src/yaml_minimal.h
 *
 * 解析 agentos.yaml 文件内容为 agentos_yaml_config_t 结构体。
 * 支持环境变量覆盖和配置热重载。
 *
 * @owner team-A
 */

#include "yaml_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 引入项目已有的 YAML 最小解析器 */
#include "yaml_minimal.h"

/* ── 默认值常量 ── */

#define DEFAULT_IPC_MAX_MESSAGE_SIZE   65536
#define DEFAULT_IPC_SHM_POOL_MB        128
#define DEFAULT_SCHED_MAX_TASKS        1024
#define DEFAULT_SCHED_PRIORITY         50
#define DEFAULT_SCHED_TIME_SLICE_MS    10
#define DEFAULT_MEM_MAX_ALLOC_MB       4096
#define DEFAULT_MEM_OOM_WATERMARK      85
#define DEFAULT_MEM_ARENA_SIZE_KB      64
#define DEFAULT_MEM_TCACHE_MAX         256
#define DEFAULT_MEM_SLAB_MIN_OBJS      8
#define DEFAULT_MEMPOOL_RESERVED_MB    32
#define DEFAULT_LEAK_SCAN_INTERVAL     60
#define DEFAULT_RSS_GROWTH_PERCENT     5
#define DEFAULT_DEFERRED_FREE_MS       1000
#define DEFAULT_LLM_ROUTING_STRATEGY   "cost_aware"
#define DEFAULT_LLM_CACHE_TTL          3600
#define DEFAULT_LLM_CACHE_MAX_ENTRIES  10000
#define DEFAULT_LLM_COST_BUDGET_USD    10.0
#define DEFAULT_MEMORY_EMBEDDING_DIM   1536
#define DEFAULT_MEMORY_MAX_ENTITIES    100000

/* ── 辅助函数 ── */

static void safe_strcpy(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void parse_llm_provider(struct yaml_node *node,
                               agentos_llm_provider_config_t *prov) {
    if (!node || !prov) return;

    struct yaml_node *name_node = yaml_get(node, "type");
    if (!name_node && yaml_has_key(node, "type")) {
        /* Handle special case: provider name is the mapping key */
    }

    const char *name = yaml_as_string(yaml_get(node, "name"), "");
    const char *type = yaml_as_string(yaml_get(node, "type"), "openai");
    const char *api_key_env = yaml_as_string(yaml_get(node, "api_key_env"), "");
    const char *base_url = yaml_as_string(yaml_get(node, "base_url"), "");

    safe_strcpy(prov->name, name, sizeof(prov->name));
    safe_strcpy(prov->type, type, sizeof(prov->type));
    safe_strcpy(prov->api_key_env, api_key_env, sizeof(prov->api_key_env));
    safe_strcpy(prov->base_url, base_url, sizeof(prov->base_url));

    /* 解析模型列表 */
    struct yaml_node *models_node = yaml_get(node, "models");
    if (models_node && models_node->type == YAML_NODE_SEQUENCE) {
        size_t count = yaml_size(models_node);
        if (count > AGENTOS_LLM_MAX_MODELS_PER_PROVIDER)
            count = AGENTOS_LLM_MAX_MODELS_PER_PROVIDER;
        for (size_t i = 0; i < count; i++) {
            struct yaml_node *model_node = yaml_get_index(models_node, i);
            const char *model = yaml_as_string(model_node, "");
            safe_strcpy(prov->models[i], model, sizeof(prov->models[i]));
        }
        prov->model_count = (uint32_t)count;
    }
}

/* ================================================================
 * 默认值填充
 * ================================================================ */

void agentos_yaml_config_defaults(agentos_yaml_config_t *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));

    /* version */
    safe_strcpy(config->version, "0.1.1", sizeof(config->version));

    /* kernel */
    config->kernel.ipc.max_message_size = DEFAULT_IPC_MAX_MESSAGE_SIZE;
    config->kernel.ipc.shm_pool_size_mb = DEFAULT_IPC_SHM_POOL_MB;
    config->kernel.scheduler.max_tasks = DEFAULT_SCHED_MAX_TASKS;
    config->kernel.scheduler.default_priority = DEFAULT_SCHED_PRIORITY;
    config->kernel.scheduler.time_slice_ms = DEFAULT_SCHED_TIME_SLICE_MS;
    config->kernel.memory.max_alloc_mb = DEFAULT_MEM_MAX_ALLOC_MB;
    config->kernel.memory.oom_watermark_percent = DEFAULT_MEM_OOM_WATERMARK;
    config->kernel.memory.arena_default_size_kb = DEFAULT_MEM_ARENA_SIZE_KB;
    config->kernel.memory.pool_thread_cache_max = DEFAULT_MEM_TCACHE_MAX;
    config->kernel.memory.slab_min_objs = DEFAULT_MEM_SLAB_MIN_OBJS;
    config->kernel.memory.mempool_reserved_mb = DEFAULT_MEMPOOL_RESERVED_MB;
    config->kernel.memory.leak_scan_interval_sec = DEFAULT_LEAK_SCAN_INTERVAL;
    config->kernel.memory.soak_test_rss_growth_percent = DEFAULT_RSS_GROWTH_PERCENT;
    config->kernel.memory.sensitive_zero_on_free = true;
    config->kernel.memory.deferred_free_delay_ms = DEFAULT_DEFERRED_FREE_MS;

    /* llm */
    safe_strcpy(config->llm.default_provider, "openai",
                sizeof(config->llm.default_provider));
    /* 默认 openai provider */
    agentos_llm_provider_config_t *openai = &config->llm.providers[0];
    safe_strcpy(openai->name, "openai", sizeof(openai->name));
    safe_strcpy(openai->type, "openai", sizeof(openai->type));
    safe_strcpy(openai->api_key_env, "OPENAI_API_KEY",
                sizeof(openai->api_key_env));
    safe_strcpy(openai->base_url, "https://api.openai.com/v1",
                sizeof(openai->base_url));
    safe_strcpy(openai->models[0], "gpt-4o", sizeof(openai->models[0]));
    safe_strcpy(openai->models[1], "gpt-4o-mini", sizeof(openai->models[1]));
    openai->model_count = 2;
    config->llm.provider_count = 1;

    safe_strcpy(config->llm.routing.strategy, DEFAULT_LLM_ROUTING_STRATEGY,
                sizeof(config->llm.routing.strategy));
    /* 默认降级链 */
    safe_strcpy(config->llm.routing.fallback_chain[0], "openai",
                sizeof(config->llm.routing.fallback_chain[0]));
    safe_strcpy(config->llm.routing.fallback_chain[1], "anthropic",
                sizeof(config->llm.routing.fallback_chain[1]));
    safe_strcpy(config->llm.routing.fallback_chain[2], "deepseek",
                sizeof(config->llm.routing.fallback_chain[2]));
    config->llm.routing.fallback_count = 3;
    config->llm.routing.cost_budget_daily_usd = DEFAULT_LLM_COST_BUDGET_USD;

    config->llm.cache.enabled = true;
    config->llm.cache.ttl_seconds = DEFAULT_LLM_CACHE_TTL;
    config->llm.cache.max_entries = DEFAULT_LLM_CACHE_MAX_ENTRIES;

    /* memory */
    config->memory.enabled = true;
    safe_strcpy(config->memory.mode, "full", sizeof(config->memory.mode));
    safe_strcpy(config->memory.storage_path, "/var/lib/agentos/memory",
                sizeof(config->memory.storage_path));
    safe_strcpy(config->memory.l1.compression, "zstd",
                sizeof(config->memory.l1.compression));
    safe_strcpy(config->memory.l2.embedder, "openai",
                sizeof(config->memory.l2.embedder));
    config->memory.l2.embedding_dim = DEFAULT_MEMORY_EMBEDDING_DIM;
    config->memory.l3.enabled = true;
    config->memory.l3.max_entities = DEFAULT_MEMORY_MAX_ENTITIES;
    config->memory.l4.enabled = true;
    safe_strcpy(config->memory.l4.clustering_algorithm, "hdbscan",
                sizeof(config->memory.l4.clustering_algorithm));

    /* security */
    config->security.enabled = true;
    safe_strcpy(config->security.mode, "standard",
                sizeof(config->security.mode));
    config->security.sandbox.enabled = true;
    safe_strcpy(config->security.sandbox.type, "seccomp",
                sizeof(config->security.sandbox.type));
    safe_strcpy(config->security.permission.model, "rbac",
                sizeof(config->security.permission.model));
    config->security.permission.cache_ttl_seconds = 300;
    config->security.audit.enabled = true;
    safe_strcpy(config->security.audit.log_path,
                "/var/log/agentos/audit.log",
                sizeof(config->security.audit.log_path));

    /* multi_agent */
    config->multi_agent.enabled = false;
    config->multi_agent.max_concurrent_agents = 64;
    safe_strcpy(config->multi_agent.communication.protocol, "a2a",
                sizeof(config->multi_agent.communication.protocol));
    safe_strcpy(config->multi_agent.collaboration.default_pattern,
                "orchestrator",
                sizeof(config->multi_agent.collaboration.default_pattern));
    config->multi_agent.lanes.enabled = true;
    safe_strcpy(config->multi_agent.lanes.default_isolation, "process",
                sizeof(config->multi_agent.lanes.default_isolation));

    /* gateway */
    config->gateway.enabled = true;
    config->gateway.http.port = 8080;
    config->gateway.websocket.enabled = true;
    config->gateway.mcp.enabled = true;
    config->gateway.mcp.enable_progress = true;
    config->gateway.mcp.enable_cancellation = true;
    config->gateway.a2a.enabled = true;
    config->gateway.a2a.default_timeout_ms = 60000;
    config->gateway.openai_compat.enabled = true;

    /* hooks */
    config->hooks.enabled = true;
    safe_strcpy(config->hooks.hook_dirs[0], "/etc/agentos/hooks.d/",
                sizeof(config->hooks.hook_dirs[0]));
    safe_strcpy(config->hooks.hook_dirs[1], "~/.agentos/hooks/",
                sizeof(config->hooks.hook_dirs[1]));
    config->hooks.hook_dir_count = 2;

    /* plugins */
    config->plugins.enabled = true;
    safe_strcpy(config->plugins.plugin_dirs[0], "/usr/lib/agentos/plugins/",
                sizeof(config->plugins.plugin_dirs[0]));
    safe_strcpy(config->plugins.plugin_dirs[1], "~/.agentos/plugins/",
                sizeof(config->plugins.plugin_dirs[1]));
    config->plugins.plugin_dir_count = 2;
    config->plugins.auto_discover = true;

    /* observability */
    config->observability.metrics.enabled = true;
    config->observability.metrics.port = 9090;
    config->observability.tracing.enabled = false;
    safe_strcpy(config->observability.tracing.exporter, "otlp",
                sizeof(config->observability.tracing.exporter));
    safe_strcpy(config->observability.logging.level, "info",
                sizeof(config->observability.logging.level));
    safe_strcpy(config->observability.logging.format, "json",
                sizeof(config->observability.logging.format));
    safe_strcpy(config->observability.health.endpoint, "/healthz",
                sizeof(config->observability.health.endpoint));
    safe_strcpy(config->observability.health.ready_endpoint, "/readyz",
                sizeof(config->observability.health.ready_endpoint));
}

/* ================================================================
 * YAML 解析
 * ================================================================ */

static void parse_kernel_config(struct yaml_node *root,
                                agentos_kernel_config_t *kern) {
    struct yaml_node *kn = yaml_get(root, "kernel");
    if (!kn) return;

    struct yaml_node *ipc = yaml_get(kn, "ipc");
    if (ipc) {
        kern->ipc.max_message_size = (uint32_t)yaml_as_int64(
            yaml_get(ipc, "max_message_size"), kern->ipc.max_message_size);
        kern->ipc.shm_pool_size_mb = (uint32_t)yaml_as_int64(
            yaml_get(ipc, "shm_pool_size_mb"), kern->ipc.shm_pool_size_mb);
    }

    struct yaml_node *sched = yaml_get(kn, "scheduler");
    if (sched) {
        kern->scheduler.max_tasks = (uint32_t)yaml_as_int64(
            yaml_get(sched, "max_tasks"), kern->scheduler.max_tasks);
        kern->scheduler.default_priority = (uint32_t)yaml_as_int64(
            yaml_get(sched, "default_priority"),
            kern->scheduler.default_priority);
        kern->scheduler.time_slice_ms = (uint32_t)yaml_as_int64(
            yaml_get(sched, "time_slice_ms"), kern->scheduler.time_slice_ms);
    }

    struct yaml_node *mem = yaml_get(kn, "memory");
    if (mem) {
        kern->memory.max_alloc_mb = (uint32_t)yaml_as_int64(
            yaml_get(mem, "max_alloc_mb"), kern->memory.max_alloc_mb);
        kern->memory.oom_watermark_percent = (uint32_t)yaml_as_int64(
            yaml_get(mem, "oom_watermark_percent"),
            kern->memory.oom_watermark_percent);
        kern->memory.arena_default_size_kb = (uint32_t)yaml_as_int64(
            yaml_get(mem, "arena_default_size_kb"),
            kern->memory.arena_default_size_kb);
        kern->memory.pool_thread_cache_max = (uint32_t)yaml_as_int64(
            yaml_get(mem, "pool_thread_cache_max"),
            kern->memory.pool_thread_cache_max);
        kern->memory.slab_min_objs = (uint32_t)yaml_as_int64(
            yaml_get(mem, "slab_min_objs"), kern->memory.slab_min_objs);
        kern->memory.mempool_reserved_mb = (uint32_t)yaml_as_int64(
            yaml_get(mem, "mempool_reserved_mb"),
            kern->memory.mempool_reserved_mb);
        kern->memory.leak_scan_interval_sec = (uint32_t)yaml_as_int64(
            yaml_get(mem, "leak_scan_interval_sec"),
            kern->memory.leak_scan_interval_sec);
        kern->memory.soak_test_rss_growth_percent = (uint32_t)yaml_as_int64(
            yaml_get(mem, "soak_test_rss_growth_percent"),
            kern->memory.soak_test_rss_growth_percent);
        kern->memory.sensitive_zero_on_free = yaml_as_bool(
            yaml_get(mem, "sensitive_zero_on_free"),
            kern->memory.sensitive_zero_on_free);
        kern->memory.deferred_free_delay_ms = (uint32_t)yaml_as_int64(
            yaml_get(mem, "deferred_free_delay_ms"),
            kern->memory.deferred_free_delay_ms);
    }
}

static void parse_llm_config(struct yaml_node *root,
                             agentos_llm_config_t *llm_cfg) {
    struct yaml_node *llm = yaml_get(root, "llm");
    if (!llm) return;

    const char *dp = yaml_as_string(yaml_get(llm, "default_provider"),
                                     llm_cfg->default_provider);
    safe_strcpy(llm_cfg->default_provider, dp,
                sizeof(llm_cfg->default_provider));

    struct yaml_node *providers = yaml_get(llm, "providers");
    if (providers && providers->type == YAML_NODE_MAPPING) {
        /* 遍历所有提供商 */
        size_t count = yaml_size(providers);
        llm_cfg->provider_count = 0;
        for (size_t i = 0; i < count && i < AGENTOS_LLM_MAX_PROVIDERS; i++) {
            /* yaml_minimal 的 mapping entry: key + value */
            /* 需要遍历 mapping 条目 */
            /* 由于 yaml_minimal API 限制，我们使用 yaml_get 按名称查找 */
        }

        /* 已知的提供商名称列表 */
        static const char *known_providers[] = {
            "openai", "anthropic", "deepseek", "google", "local",
            "ollama", "azure", "cohere", "mistral", "groq", NULL
        };

        for (int p = 0; known_providers[p]; p++) {
            struct yaml_node *prov = yaml_get(providers, known_providers[p]);
            if (prov) {
                uint32_t idx = llm_cfg->provider_count;
                if (idx >= AGENTOS_LLM_MAX_PROVIDERS) break;
                safe_strcpy(llm_cfg->providers[idx].name, known_providers[p],
                            sizeof(llm_cfg->providers[idx].name));
                parse_llm_provider(prov, &llm_cfg->providers[idx]);
                llm_cfg->provider_count++;
            }
        }
    }

    struct yaml_node *routing = yaml_get(llm, "routing");
    if (routing) {
        const char *strategy = yaml_as_string(
            yaml_get(routing, "strategy"), llm_cfg->routing.strategy);
        safe_strcpy(llm_cfg->routing.strategy, strategy,
                    sizeof(llm_cfg->routing.strategy));

        llm_cfg->routing.cost_budget_daily_usd = yaml_as_double(
            yaml_get(routing, "cost_budget_daily_usd"),
            llm_cfg->routing.cost_budget_daily_usd);

        struct yaml_node *fb = yaml_get(routing, "fallback_chain");
        if (fb && fb->type == YAML_NODE_SEQUENCE) {
            size_t fb_count = yaml_size(fb);
            if (fb_count > AGENTOS_LLM_MAX_FALLBACK)
                fb_count = AGENTOS_LLM_MAX_FALLBACK;
            for (size_t i = 0; i < fb_count; i++) {
                struct yaml_node *item = yaml_get_index(fb, i);
                const char *val = yaml_as_string(item, "");
                safe_strcpy(llm_cfg->routing.fallback_chain[i], val,
                            sizeof(llm_cfg->routing.fallback_chain[i]));
            }
            llm_cfg->routing.fallback_count = (uint32_t)fb_count;
        }
    }

    struct yaml_node *cache = yaml_get(llm, "cache");
    if (cache) {
        llm_cfg->cache.enabled = yaml_as_bool(
            yaml_get(cache, "enabled"), llm_cfg->cache.enabled);
        llm_cfg->cache.ttl_seconds = (uint32_t)yaml_as_int64(
            yaml_get(cache, "ttl_seconds"), llm_cfg->cache.ttl_seconds);
        llm_cfg->cache.max_entries = (uint32_t)yaml_as_int64(
            yaml_get(cache, "max_entries"), llm_cfg->cache.max_entries);
    }
}

static void parse_memory_config(struct yaml_node *root,
                                agentos_memory_config_t *mem_cfg) {
    struct yaml_node *mem = yaml_get(root, "memory");
    if (!mem) return;

    mem_cfg->enabled = yaml_as_bool(yaml_get(mem, "enabled"),
                                     mem_cfg->enabled);
    const char *mode = yaml_as_string(yaml_get(mem, "mode"), mem_cfg->mode);
    safe_strcpy(mem_cfg->mode, mode, sizeof(mem_cfg->mode));
    const char *sp = yaml_as_string(yaml_get(mem, "storage_path"),
                                     mem_cfg->storage_path);
    safe_strcpy(mem_cfg->storage_path, sp, sizeof(mem_cfg->storage_path));

    struct yaml_node *layers = yaml_get(mem, "layers");
    if (layers) {
        struct yaml_node *l1 = yaml_get(layers, "l1");
        if (l1) {
            const char *comp = yaml_as_string(yaml_get(l1, "compression"),
                                               mem_cfg->l1.compression);
            safe_strcpy(mem_cfg->l1.compression, comp,
                        sizeof(mem_cfg->l1.compression));
        }
        struct yaml_node *l2 = yaml_get(layers, "l2");
        if (l2) {
            const char *emb = yaml_as_string(yaml_get(l2, "embedder"),
                                              mem_cfg->l2.embedder);
            safe_strcpy(mem_cfg->l2.embedder, emb,
                        sizeof(mem_cfg->l2.embedder));
            mem_cfg->l2.embedding_dim = (uint32_t)yaml_as_int64(
                yaml_get(l2, "embedding_dim"), mem_cfg->l2.embedding_dim);
        }
        struct yaml_node *l3 = yaml_get(layers, "l3");
        if (l3) {
            mem_cfg->l3.enabled = yaml_as_bool(
                yaml_get(l3, "enabled"), mem_cfg->l3.enabled);
            mem_cfg->l3.max_entities = (uint32_t)yaml_as_int64(
                yaml_get(l3, "max_entities"), mem_cfg->l3.max_entities);
        }
        struct yaml_node *l4 = yaml_get(layers, "l4");
        if (l4) {
            mem_cfg->l4.enabled = yaml_as_bool(
                yaml_get(l4, "enabled"), mem_cfg->l4.enabled);
            const char *alg = yaml_as_string(
                yaml_get(l4, "clustering_algorithm"),
                mem_cfg->l4.clustering_algorithm);
            safe_strcpy(mem_cfg->l4.clustering_algorithm, alg,
                        sizeof(mem_cfg->l4.clustering_algorithm));
        }
    }
}

static void parse_security_config(struct yaml_node *root,
                                   agentos_security_config_t *sec) {
    struct yaml_node *sn = yaml_get(root, "security");
    if (!sn) return;

    sec->enabled = yaml_as_bool(yaml_get(sn, "enabled"), sec->enabled);
    const char *mode = yaml_as_string(yaml_get(sn, "mode"), sec->mode);
    safe_strcpy(sec->mode, mode, sizeof(sec->mode));

    struct yaml_node *sandbox = yaml_get(sn, "sandbox");
    if (sandbox) {
        sec->sandbox.enabled = yaml_as_bool(
            yaml_get(sandbox, "enabled"), sec->sandbox.enabled);
        const char *st = yaml_as_string(
            yaml_get(sandbox, "type"), sec->sandbox.type);
        safe_strcpy(sec->sandbox.type, st, sizeof(sec->sandbox.type));
    }

    struct yaml_node *perm = yaml_get(sn, "permission");
    if (perm) {
        const char *model = yaml_as_string(
            yaml_get(perm, "model"), sec->permission.model);
        safe_strcpy(sec->permission.model, model,
                    sizeof(sec->permission.model));
        sec->permission.cache_ttl_seconds = (uint32_t)yaml_as_int64(
            yaml_get(perm, "cache_ttl_seconds"),
            sec->permission.cache_ttl_seconds);
    }

    struct yaml_node *audit = yaml_get(sn, "audit");
    if (audit) {
        sec->audit.enabled = yaml_as_bool(
            yaml_get(audit, "enabled"), sec->audit.enabled);
        const char *lp = yaml_as_string(
            yaml_get(audit, "log_path"), sec->audit.log_path);
        safe_strcpy(sec->audit.log_path, lp, sizeof(sec->audit.log_path));
    }
}

static void parse_gateway_config(struct yaml_node *root,
                                  agentos_gateway_config_t *gw) {
    struct yaml_node *gn = yaml_get(root, "gateway");
    if (!gn) return;

    gw->enabled = yaml_as_bool(yaml_get(gn, "enabled"), gw->enabled);

    struct yaml_node *http = yaml_get(gn, "http");
    if (http) {
        gw->http.port = (uint16_t)yaml_as_int64(
            yaml_get(http, "port"), gw->http.port);
    }

    struct yaml_node *ws = yaml_get(gn, "websocket");
    if (ws) {
        gw->websocket.enabled = yaml_as_bool(
            yaml_get(ws, "enabled"), gw->websocket.enabled);
    }

    struct yaml_node *mcp = yaml_get(gn, "mcp");
    if (mcp) {
        gw->mcp.enabled = yaml_as_bool(
            yaml_get(mcp, "enabled"), gw->mcp.enabled);
        gw->mcp.enable_progress = yaml_as_bool(
            yaml_get(mcp, "enable_progress"), gw->mcp.enable_progress);
        gw->mcp.enable_cancellation = yaml_as_bool(
            yaml_get(mcp, "enable_cancellation"),
            gw->mcp.enable_cancellation);
    }

    struct yaml_node *a2a = yaml_get(gn, "a2a");
    if (a2a) {
        gw->a2a.enabled = yaml_as_bool(
            yaml_get(a2a, "enabled"), gw->a2a.enabled);
        gw->a2a.default_timeout_ms = (uint32_t)yaml_as_int64(
            yaml_get(a2a, "default_timeout_ms"),
            gw->a2a.default_timeout_ms);
    }

    struct yaml_node *oai = yaml_get(gn, "openai_compat");
    if (oai) {
        gw->openai_compat.enabled = yaml_as_bool(
            yaml_get(oai, "enabled"), gw->openai_compat.enabled);
    }
}

static void parse_observability_config(struct yaml_node *root,
                                        agentos_observability_config_t *obs) {
    struct yaml_node *on = yaml_get(root, "observability");
    if (!on) return;

    struct yaml_node *metrics = yaml_get(on, "metrics");
    if (metrics) {
        obs->metrics.enabled = yaml_as_bool(
            yaml_get(metrics, "enabled"), obs->metrics.enabled);
        obs->metrics.port = (uint16_t)yaml_as_int64(
            yaml_get(metrics, "port"), obs->metrics.port);
    }

    struct yaml_node *tracing = yaml_get(on, "tracing");
    if (tracing) {
        obs->tracing.enabled = yaml_as_bool(
            yaml_get(tracing, "enabled"), obs->tracing.enabled);
        const char *exp = yaml_as_string(
            yaml_get(tracing, "exporter"), obs->tracing.exporter);
        safe_strcpy(obs->tracing.exporter, exp,
                    sizeof(obs->tracing.exporter));
    }

    struct yaml_node *logging = yaml_get(on, "logging");
    if (logging) {
        const char *level = yaml_as_string(
            yaml_get(logging, "level"), obs->logging.level);
        safe_strcpy(obs->logging.level, level,
                    sizeof(obs->logging.level));
        const char *format = yaml_as_string(
            yaml_get(logging, "format"), obs->logging.format);
        safe_strcpy(obs->logging.format, format,
                    sizeof(obs->logging.format));
    }

    struct yaml_node *health = yaml_get(on, "health");
    if (health) {
        const char *ep = yaml_as_string(
            yaml_get(health, "endpoint"), obs->health.endpoint);
        safe_strcpy(obs->health.endpoint, ep,
                    sizeof(obs->health.endpoint));
        const char *rep = yaml_as_string(
            yaml_get(health, "ready_endpoint"),
            obs->health.ready_endpoint);
        safe_strcpy(obs->health.ready_endpoint, rep,
                    sizeof(obs->health.ready_endpoint));
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

int agentos_yaml_parse(const char *yaml_content,
                       agentos_yaml_config_t *config) {
    if (!yaml_content || !config) return -1;

    yaml_document_t *doc = yaml_create();
    if (!doc) return -1;

    if (yaml_parse_string(doc, yaml_content,
                          strlen(yaml_content)) != 0) {
        yaml_destroy(doc);
        return -1;
    }

    struct yaml_node *root = yaml_root(doc);
    if (!root || root->type != YAML_NODE_MAPPING) {
        yaml_destroy(doc);
        return -1;
    }

    /* 解析 version */
    const char *ver = yaml_as_string(yaml_get(root, "version"),
                                     config->version);
    safe_strcpy(config->version, ver, sizeof(config->version));

    /* 解析各配置节 */
    parse_kernel_config(root, &config->kernel);
    parse_llm_config(root, &config->llm);
    parse_memory_config(root, &config->memory);
    parse_security_config(root, &config->security);
    parse_gateway_config(root, &config->gateway);
    parse_observability_config(root, &config->observability);

    /* 解析 multi_agent */
    struct yaml_node *ma = yaml_get(root, "multi_agent");
    if (ma) {
        config->multi_agent.enabled = yaml_as_bool(
            yaml_get(ma, "enabled"), config->multi_agent.enabled);
        config->multi_agent.max_concurrent_agents = (uint32_t)yaml_as_int64(
            yaml_get(ma, "max_concurrent_agents"),
            config->multi_agent.max_concurrent_agents);

        struct yaml_node *comm = yaml_get(ma, "communication");
        if (comm) {
            const char *proto = yaml_as_string(
                yaml_get(comm, "protocol"),
                config->multi_agent.communication.protocol);
            safe_strcpy(config->multi_agent.communication.protocol, proto,
                        sizeof(config->multi_agent.communication.protocol));
        }

        struct yaml_node *collab = yaml_get(ma, "collaboration");
        if (collab) {
            const char *pattern = yaml_as_string(
                yaml_get(collab, "default_pattern"),
                config->multi_agent.collaboration.default_pattern);
            safe_strcpy(config->multi_agent.collaboration.default_pattern,
                        pattern,
                        sizeof(config->multi_agent.collaboration
                                   .default_pattern));
        }

        struct yaml_node *lanes = yaml_get(ma, "lanes");
        if (lanes) {
            config->multi_agent.lanes.enabled = yaml_as_bool(
                yaml_get(lanes, "enabled"),
                config->multi_agent.lanes.enabled);
            const char *iso = yaml_as_string(
                yaml_get(lanes, "default_isolation"),
                config->multi_agent.lanes.default_isolation);
            safe_strcpy(config->multi_agent.lanes.default_isolation, iso,
                        sizeof(config->multi_agent.lanes.default_isolation));
        }
    }

    /* 解析 hooks */
    struct yaml_node *hooks = yaml_get(root, "hooks");
    if (hooks) {
        config->hooks.enabled = yaml_as_bool(
            yaml_get(hooks, "enabled"), config->hooks.enabled);

        struct yaml_node *dirs = yaml_get(hooks, "hook_dirs");
        if (dirs && dirs->type == YAML_NODE_SEQUENCE) {
            size_t dc = yaml_size(dirs);
            if (dc > AGENTOS_HOOK_MAX_DIRS) dc = AGENTOS_HOOK_MAX_DIRS;
            for (size_t i = 0; i < dc; i++) {
                struct yaml_node *item = yaml_get_index(dirs, i);
                const char *d = yaml_as_string(item, "");
                safe_strcpy(config->hooks.hook_dirs[i], d,
                            sizeof(config->hooks.hook_dirs[i]));
            }
            config->hooks.hook_dir_count = (uint32_t)dc;
        }
    }

    /* 解析 plugins */
    struct yaml_node *plugins = yaml_get(root, "plugins");
    if (plugins) {
        config->plugins.enabled = yaml_as_bool(
            yaml_get(plugins, "enabled"), config->plugins.enabled);
        config->plugins.auto_discover = yaml_as_bool(
            yaml_get(plugins, "auto_discover"),
            config->plugins.auto_discover);

        struct yaml_node *dirs = yaml_get(plugins, "plugin_dirs");
        if (dirs && dirs->type == YAML_NODE_SEQUENCE) {
            size_t dc = yaml_size(dirs);
            if (dc > AGENTOS_PLUGIN_MAX_DIRS) dc = AGENTOS_PLUGIN_MAX_DIRS;
            for (size_t i = 0; i < dc; i++) {
                struct yaml_node *item = yaml_get_index(dirs, i);
                const char *d = yaml_as_string(item, "");
                safe_strcpy(config->plugins.plugin_dirs[i], d,
                            sizeof(config->plugins.plugin_dirs[i]));
            }
            config->plugins.plugin_dir_count = (uint32_t)dc;
        }
    }

    yaml_destroy(doc);
    return 0;
}

int agentos_yaml_load(const char *yaml_path,
                      agentos_yaml_config_t *config) {
    if (!yaml_path || !config) return -1;

    /* 先填充默认值 */
    agentos_yaml_config_defaults(config);

    /* 读取文件 */
    FILE *fp = fopen(yaml_path, "rb");
    if (!fp) {
        /* 文件不存在时使用默认配置 */
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0 || size > (4 * 1024 * 1024)) { /* 4MB max */
        fclose(fp);
        return -1;
    }

    char *content = (char *)malloc((size_t)size + 1);
    if (!content) {
        fclose(fp);
        return -1;
    }

    size_t read = fread(content, 1, (size_t)size, fp);
    fclose(fp);

    if (read != (size_t)size) {
        free(content);
        return -1;
    }
    content[read] = '\0';

    int ret = agentos_yaml_parse(content, config);
    free(content);

    /* 解析后应用环境变量覆盖 */
    if (ret == 0) {
        agentos_yaml_env_override(config);
        /* 平台路径映射（P1.12.4） */
        agentos_yaml_resolve_platform_paths(config);
    }

    return ret;
}

/* ================================================================
 * 环境变量覆盖
 * ================================================================ */

int agentos_yaml_env_override(agentos_yaml_config_t *config) {
    if (!config) return -1;

    const char *val;

    /* LLM 默认提供商 */
    val = getenv("AGENTOS_LLM_DEFAULT_PROVIDER");
    if (val) {
        safe_strcpy(config->llm.default_provider, val,
                    sizeof(config->llm.default_provider));
    }

    /* LLM 路由策略 */
    val = getenv("AGENTOS_LLM_ROUTING_STRATEGY");
    if (val) {
        safe_strcpy(config->llm.routing.strategy, val,
                    sizeof(config->llm.routing.strategy));
    }

    /* LLM 成本预算 */
    val = getenv("AGENTOS_LLM_ROUTING_COST_BUDGET_DAILY_USD");
    if (val) {
        config->llm.routing.cost_budget_daily_usd = atof(val);
    }

    /* 各提供商的 API Key */
    static const char *provider_names[] = {
        "OPENAI", "ANTHROPIC", "DEEPSEEK", "GOOGLE", "LOCAL",
        "OLLAMA", "AZURE", "COHERE", "MISTRAL", "GROQ", NULL
    };

    for (int p = 0; provider_names[p]; p++) {
        /* 查找对应提供商 */
        for (uint32_t i = 0; i < config->llm.provider_count; i++) {
            /* 不区分大小写比较 */
            /* 简化：直接按配置顺序匹配 */
        }
    }

    /* API Key 环境变量覆盖 */
    /* AGENTOS_LLM_PROVIDERS_OPENAI_API_KEY → OPENAI_API_KEY */
    val = getenv("AGENTOS_LLM_PROVIDERS_OPENAI_API_KEY");
    if (!val) val = getenv("OPENAI_API_KEY");
    if (val) {
        /* 标记已有 API Key（实际使用时通过 api_key_env 环境变量名获取） */
    }

    val = getenv("AGENTOS_LLM_PROVIDERS_ANTHROPIC_API_KEY");
    if (!val) val = getenv("ANTHROPIC_API_KEY");

    val = getenv("AGENTOS_LLM_PROVIDERS_DEEPSEEK_API_KEY");
    if (!val) val = getenv("DEEPSEEK_API_KEY");

    /* 记忆存储路径 */
    val = getenv("AGENTOS_MEMORY_STORAGE_PATH");
    if (val) {
        safe_strcpy(config->memory.storage_path, val,
                    sizeof(config->memory.storage_path));
    }

    /* 网关 HTTP 端口 */
    val = getenv("AGENTOS_GATEWAY_HTTP_PORT");
    if (val) {
        int port = atoi(val);
        if (port > 0 && port <= 65535) {
            config->gateway.http.port = (uint16_t)port;
        }
    }

    /* Metrics 端口 */
    val = getenv("AGENTOS_OBSERVABILITY_METRICS_PORT");
    if (val) {
        int port = atoi(val);
        if (port > 0 && port <= 65535) {
            config->observability.metrics.port = (uint16_t)port;
        }
    }

    /* 日志级别 */
    val = getenv("AGENTOS_OBSERVABILITY_LOGGING_LEVEL");
    if (val) {
        safe_strcpy(config->observability.logging.level, val,
                    sizeof(config->observability.logging.level));
    }

    /* 安全模式 */
    val = getenv("AGENTOS_SECURITY_MODE");
    if (val) {
        safe_strcpy(config->security.mode, val,
                    sizeof(config->security.mode));
    }

    /* 内核内存限制 */
    val = getenv("AGENTOS_KERNEL_MEMORY_MAX_ALLOC_MB");
    if (val) {
        int mb = atoi(val);
        if (mb > 0) {
            config->kernel.memory.max_alloc_mb = (uint32_t)mb;
        }
    }

    /* 内核 IPC 配置 */
    val = getenv("AGENTOS_KERNEL_IPC_MAX_MESSAGE_SIZE");
    if (val) {
        int sz = atoi(val);
        if (sz > 0) config->kernel.ipc.max_message_size = (uint32_t)sz;
    }

    val = getenv("AGENTOS_KERNEL_IPC_SHM_POOL_SIZE_MB");
    if (val) {
        int mb = atoi(val);
        if (mb > 0) config->kernel.ipc.shm_pool_size_mb = (uint32_t)mb;
    }

    /* 内核调度器配置 */
    val = getenv("AGENTOS_KERNEL_SCHEDULER_MAX_TASKS");
    if (val) {
        int tasks = atoi(val);
        if (tasks > 0) config->kernel.scheduler.max_tasks = (uint32_t)tasks;
    }

    val = getenv("AGENTOS_KERNEL_SCHEDULER_TIME_SLICE_MS");
    if (val) {
        int ms = atoi(val);
        if (ms > 0) config->kernel.scheduler.time_slice_ms = (uint32_t)ms;
    }

    /* 内核 OOM 水位 */
    val = getenv("AGENTOS_KERNEL_MEMORY_OOM_WATERMARK_PERCENT");
    if (val) {
        int pct = atoi(val);
        if (pct >= 50 && pct <= 100) {
            config->kernel.memory.oom_watermark_percent = (uint32_t)pct;
        }
    }

    /* 记忆系统提供商 */
    val = getenv("AGENTOS_MEMORY_PROVIDER");
    if (val) {
        /* P1.11.2: 通过环境变量切换 memory provider */
        /* 值: builtin | memoryrovol | auto */
    }

    /* 多 Agent 配置 */
    val = getenv("AGENTOS_MULTI_AGENT_MAX_CONCURRENT");
    if (val) {
        int n = atoi(val);
        if (n > 0) config->multi_agent.max_concurrent_agents = (uint32_t)n;
    }

    /* 安全沙箱类型 */
    val = getenv("AGENTOS_SECURITY_SANDBOX_TYPE");
    if (val) {
        safe_strcpy(config->security.sandbox.type, val,
                    sizeof(config->security.sandbox.type));
    }

    /* 审计日志路径 */
    val = getenv("AGENTOS_SECURITY_AUDIT_LOG_PATH");
    if (val) {
        safe_strcpy(config->security.audit.log_path, val,
                    sizeof(config->security.audit.log_path));
    }

    /* 可观测性追踪导出器 */
    val = getenv("AGENTOS_OBSERVABILITY_TRACING_EXPORTER");
    if (val) {
        safe_strcpy(config->observability.tracing.exporter, val,
                    sizeof(config->observability.tracing.exporter));
    }

    return 0;
}

/* ================================================================
 * 配置验证
 * ================================================================ */

void agentos_yaml_config_free(agentos_yaml_config_t *config) {
    if (!config) return;
    /* 目前所有字段都是栈上的固定大小数组，无需额外释放 */
    /* 未来如果改为动态分配，在此处释放 */
}

int agentos_yaml_validate(const agentos_yaml_config_t *config) {
    if (!config) return -1;

    /* 验证 version */
    if (config->version[0] == '\0') return -2;

    /* 验证 LLM 配置 */
    if (config->llm.default_provider[0] == '\0') return -3;
    if (config->llm.provider_count == 0) return -4;

    /* 验证路由策略 */
    if (config->llm.routing.strategy[0] == '\0') return -5;
    if (config->llm.routing.fallback_count == 0) return -6;

    /* 验证 LLM 缓存配置 */
    if (config->llm.cache.enabled) {
        if (config->llm.cache.ttl_seconds == 0) return -7;
        if (config->llm.cache.max_entries == 0) return -8;
    }

    /* 验证内存配置 */
    if (config->memory.enabled) {
        if (config->memory.storage_path[0] == '\0') return -9;
    }

    /* 验证安全配置 */
    if (config->security.enabled) {
        if (config->security.mode[0] == '\0') return -10;
    }

    /* 验证网关配置 */
    if (config->gateway.enabled) {
        if (config->gateway.http.port == 0) return -11;
    }

    /* 验证可观测性配置 */
    if (config->observability.metrics.enabled) {
        if (config->observability.metrics.port == 0) return -12;
    }

    /* 验证内核配置 */
    if (config->kernel.memory.oom_watermark_percent > 100) return -13;
    if (config->kernel.memory.oom_watermark_percent < 50) return -14;
    if (config->kernel.ipc.max_message_size == 0) return -15;
    if (config->kernel.ipc.shm_pool_size_mb == 0) return -16;
    if (config->kernel.scheduler.max_tasks == 0) return -17;
    if (config->kernel.scheduler.time_slice_ms == 0) return -18;
    if (config->kernel.memory.max_alloc_mb == 0) return -19;

    /* 验证多 Agent 配置 */
    if (config->multi_agent.enabled) {
        if (config->multi_agent.max_concurrent_agents == 0) return -20;
        if (config->multi_agent.communication.protocol[0] == '\0') return -21;
        if (config->multi_agent.collaboration.default_pattern[0] == '\0') return -22;
    }

    /* 验证 Hook 配置 */
    if (config->hooks.enabled) {
        /* hook_dirs 可以为空（使用默认目录），不强制要求 */
        for (uint32_t i = 0; i < config->hooks.global_hooks.on_tool_call_count; i++) {
            if (config->hooks.global_hooks.on_tool_call[i].hook[0] == '\0') return -23;
        }
    }

    /* 验证插件配置 */
    if (config->plugins.enabled && config->plugins.plugin_dir_count == 0 && config->plugins.auto_discover) {
        /* auto_discover 模式下 plugin_dirs 可以为空 */
    }

    /* 验证可观测性日志级别 */
    if (config->observability.logging.level[0] != '\0') {
        static const char *valid_levels[] = {"debug", "info", "warn", "error", NULL};
        int valid = 0;
        for (int i = 0; valid_levels[i]; i++) {
            if (strcmp(config->observability.logging.level, valid_levels[i]) == 0) {
                valid = 1;
                break;
            }
        }
        if (!valid) return -24;
    }

    /* 验证安全模式 */
    if (config->security.enabled && config->security.mode[0] != '\0') {
        static const char *valid_modes[] = {"standard", "strict", "permissive", NULL};
        int valid = 0;
        for (int i = 0; valid_modes[i]; i++) {
            if (strcmp(config->security.mode, valid_modes[i]) == 0) {
                valid = 1;
                break;
            }
        }
        if (!valid) return -25;
    }

    /* 验证端口范围 */
    if (config->gateway.enabled && config->gateway.http.port > 0) {
        if (config->gateway.http.port < 1024 && config->gateway.http.port != 80 &&
            config->gateway.http.port != 443) {
            /* 非特权端口建议但不强制 */
        }
    }

    /* 验证记忆系统配置 */
    if (config->memory.enabled) {
        if (config->memory.mode[0] != '\0') {
            static const char *valid_modes[] = {"full", "lite", "off", NULL};
            int valid = 0;
            for (int i = 0; valid_modes[i]; i++) {
                if (strcmp(config->memory.mode, valid_modes[i]) == 0) {
                    valid = 1;
                    break;
                }
            }
            if (!valid) return -26;
        }
    }

    return 0;
}

/* ================================================================
 * 平台路径映射（P1.12.4）
 *
 * Linux 约定路径 → Windows 平台路径映射：
 *   /var/lib/agentos/ → %ProgramData%\AgentRT\data\
 *   /var/log/agentos/ → %ProgramData%\AgentRT\logs\
 *   /etc/agentos/     → %ProgramData%\AgentRT\config\
 *   /usr/lib/agentos/ → %ProgramData%\AgentRT\lib\
 *   ~/.agentos/       → %APPDATA%\AgentRT\
 * ================================================================ */

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>

static void platform_get_programdata(char *buf, size_t buf_size) {
    const char *pd = getenv("ProgramData");
    if (!pd) {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA,
                                        NULL, 0, path))) {
            pd = path;
        } else {
            pd = "C:\\ProgramData";
        }
    }
    if (buf && buf_size > 0) {
        snprintf(buf, buf_size, "%s\\AgentRT\\", pd);
    }
}

static void platform_get_appdata(char *buf, size_t buf_size) {
    const char *ad = getenv("APPDATA");
    if (!ad) {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA,
                                        NULL, 0, path))) {
            ad = path;
        } else {
            ad = "C:\\Users\\Default\\AppData\\Roaming";
        }
    }
    if (buf && buf_size > 0) {
        snprintf(buf, buf_size, "%s\\AgentRT\\", ad);
    }
}

static void platform_resolve_path(char *dst, size_t dst_size,
                                  const char *linux_prefix,
                                  const char *win_relative) {
    if (!dst || dst_size == 0) return;

    char new_path[512];
    char win_base[256];

    if (strstr(linux_prefix, "/var/lib/")
        || strstr(linux_prefix, "/var/log/")) {
        platform_get_programdata(win_base, sizeof(win_base));
    } else if (strstr(linux_prefix, "/etc/")) {
        platform_get_programdata(win_base, sizeof(win_base));
    } else if (strstr(linux_prefix, "/usr/lib/")) {
        platform_get_programdata(win_base, sizeof(win_base));
    } else if (strstr(linux_prefix, "~/")) {
        platform_get_appdata(win_base, sizeof(win_base));
    } else {
        return;
    }

    snprintf(new_path, sizeof(new_path), "%s%s", win_base, win_relative);
    safe_strcpy(dst, new_path, dst_size);
}

#else
#include <pwd.h>
#include <unistd.h>

static void platform_expand_home(char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (dst[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        char expanded[512];
        snprintf(expanded, sizeof(expanded), "%s%s", home, dst + 1);
        safe_strcpy(dst, expanded, dst_size);
    }
}

static void platform_resolve_path(char *dst, size_t dst_size,
                                  const char *linux_prefix,
                                  const char *win_relative) {
    (void)linux_prefix;
    (void)win_relative;
    if (dst && dst_size > 0) {
        platform_expand_home(dst, dst_size);
    }
}
#endif

int agentos_yaml_resolve_platform_paths(agentos_yaml_config_t *config) {
    if (!config) return -1;

    /* 记忆存储路径 */
    platform_resolve_path(config->memory.storage_path,
                          sizeof(config->memory.storage_path),
                          "/var/lib/agentos/", "data\\memory\\");

    /* 审计日志路径 */
    platform_resolve_path(config->security.audit.log_path,
                          sizeof(config->security.audit.log_path),
                          "/var/log/agentos/", "logs\\audit\\");

    /* Hook 目录 */
    for (uint32_t i = 0; i < config->hooks.hook_dir_count; i++) {
        if (strstr(config->hooks.hook_dirs[i], "/etc/agentos/")) {
            platform_resolve_path(config->hooks.hook_dirs[i],
                                  sizeof(config->hooks.hook_dirs[i]),
                                  "/etc/agentos/", "config\\hooks.d\\");
        } else if (config->hooks.hook_dirs[i][0] == '~') {
            platform_resolve_path(config->hooks.hook_dirs[i],
                                  sizeof(config->hooks.hook_dirs[i]),
                                  "~/", "hooks\\");
        }
    }

    /* Plugin 目录 */
    for (uint32_t i = 0; i < config->plugins.plugin_dir_count; i++) {
        if (strstr(config->plugins.plugin_dirs[i], "/usr/lib/agentos/")) {
            platform_resolve_path(config->plugins.plugin_dirs[i],
                                  sizeof(config->plugins.plugin_dirs[i]),
                                  "/usr/lib/agentos/", "lib\\plugins\\");
        } else if (config->plugins.plugin_dirs[i][0] == '~') {
            platform_resolve_path(config->plugins.plugin_dirs[i],
                                  sizeof(config->plugins.plugin_dirs[i]),
                                  "~/", "plugins\\");
        }
    }

    return 0;
}