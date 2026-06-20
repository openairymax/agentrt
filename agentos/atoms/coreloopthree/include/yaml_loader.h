/**
 * @file yaml_loader.h
 * @brief agentos.yaml 解析器 — 将 YAML 配置解析为结构化 C 类型
 *
 * 基于 libyaml 解析 agentos.yaml，填充 agentos_yaml_config_t 结构体。
 * 支持环境变量覆盖（AGENTOS_* 前缀）和配置热重载。
 *
 * 配置节：
 *   - kernel:    IPC、调度器、内存
 *   - llm:       提供商、路由、缓存
 *   - memory:    记忆系统（L1-L4层）
 *   - security:  Cupolas 安全穹顶
 *   - multi_agent: 多 Agent 协作
 *   - gateway:   HTTP/WS/MCP/A2A
 *   - hooks:     Hook 目录和全局 Hook
 *   - plugins:   插件目录和自动发现
 *   - observability: 指标、追踪、日志、健康检查
 *
 * @owner team-A
 * @see contracts/contract_A_B.h 第5节
 * @see 技术全面改进方案v2.6 第六章
 */

#ifndef AGENTOS_CORELOOPTHREE_YAML_LOADER_H
#define AGENTOS_CORELOOPTHREE_YAML_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 配置结构体定义
 * ================================================================ */

/* ── 内核 IPC 配置 ── */
typedef struct {
    uint32_t max_message_size;    /**< 最大消息大小（字节），默认 65536 */
    uint32_t shm_pool_size_mb;    /**< 共享内存池大小（MB），默认 128 */
} agentos_kernel_ipc_config_t;

/* ── 内核调度器配置 ── */
typedef struct {
    uint32_t max_tasks;           /**< 最大任务数，默认 1024 */
    uint32_t default_priority;    /**< 默认优先级，默认 50 */
    uint32_t time_slice_ms;       /**< 时间片（毫秒），默认 10 */
} agentos_kernel_scheduler_config_t;

/* ── 内核内存配置 ── */
typedef struct {
    uint32_t max_alloc_mb;        /**< 最大分配（MB），默认 4096 */
    uint32_t oom_watermark_percent; /**< OOM 水位百分比，默认 85 */
    uint32_t arena_default_size_kb; /**< Arena 默认大小（KB），默认 64 */
    uint32_t pool_thread_cache_max; /**< per-thread 缓存上限，默认 256 */
    uint32_t slab_min_objs;       /**< Slab 最小对象数，默认 8 */
    uint32_t mempool_reserved_mb; /**< IPC 紧急预留（MB），默认 32 */
    uint32_t leak_scan_interval_sec; /**< 泄漏扫描间隔（秒），默认 60 */
    uint32_t soak_test_rss_growth_percent; /**< 24h soak test RSS 增长上限%，默认 5 */
    bool sensitive_zero_on_free;  /**< 敏感数据释放前清零，默认 true */
    uint32_t deferred_free_delay_ms; /**< 延迟释放时间（毫秒），默认 1000 */
} agentos_kernel_memory_config_t;

/* ── 内核配置 ── */
typedef struct {
    agentos_kernel_ipc_config_t ipc;
    agentos_kernel_scheduler_config_t scheduler;
    agentos_kernel_memory_config_t memory;
} agentos_kernel_config_t;

/* ── LLM 提供商配置 ── */
#define AGENTOS_LLM_MAX_PROVIDERS 16
#define AGENTOS_LLM_MAX_MODELS_PER_PROVIDER 32

typedef struct {
    char name[64];                /**< 提供商名称 */
    char type[32];                /**< 类型: openai, anthropic, google, ollama, openai-compatible */
    char api_key_env[64];         /**< API Key 环境变量名 */
    char base_url[256];           /**< 基础 URL */
    char models[AGENTOS_LLM_MAX_MODELS_PER_PROVIDER][64]; /**< 模型列表 */
    uint32_t model_count;         /**< 模型数量 */
} agentos_llm_provider_config_t;

/* ── LLM 路由配置 ── */
#define AGENTOS_LLM_MAX_FALLBACK 8

typedef struct {
    char strategy[32];            /**< 路由策略: cost_aware, round_robin, least_latency */
    char fallback_chain[AGENTOS_LLM_MAX_FALLBACK][64]; /**< 降级链 */
    uint32_t fallback_count;      /**< 降级链数量 */
    double cost_budget_daily_usd; /**< 每日成本预算（美元） */
} agentos_llm_routing_config_t;

/* ── LLM 缓存配置 ── */
typedef struct {
    bool enabled;                 /**< 是否启用缓存 */
    uint32_t ttl_seconds;         /**< 缓存 TTL（秒） */
    uint32_t max_entries;         /**< 最大缓存条目数 */
} agentos_llm_cache_config_t;

/* ── LLM 配置 ── */
typedef struct {
    char default_provider[64];    /**< 默认提供商 */
    agentos_llm_provider_config_t providers[AGENTOS_LLM_MAX_PROVIDERS];
    uint32_t provider_count;
    agentos_llm_routing_config_t routing;
    agentos_llm_cache_config_t cache;
} agentos_llm_config_t;

/* ── 记忆系统层配置 ── */
typedef struct {
    char compression[16];         /**< 压缩算法: none, zstd, lz4 */
} agentos_memory_l1_config_t;

typedef struct {
    char embedder[64];            /**< 嵌入服务提供商 */
    uint32_t embedding_dim;       /**< 嵌入维度 */
} agentos_memory_l2_config_t;

typedef struct {
    bool enabled;
    uint32_t max_entities;
} agentos_memory_l3_config_t;

typedef struct {
    bool enabled;
    char clustering_algorithm[32]; /**< 聚类算法: hdbscan, kmeans, dbscan */
} agentos_memory_l4_config_t;

/* ── 记忆系统配置 ── */
typedef struct {
    bool enabled;
    char mode[16];                /**< full | lite | off */
    char storage_path[256];       /**< 存储路径 */
    agentos_memory_l1_config_t l1;
    agentos_memory_l2_config_t l2;
    agentos_memory_l3_config_t l3;
    agentos_memory_l4_config_t l4;
} agentos_memory_config_t;

/* ── 安全配置 ── */
typedef struct {
    bool enabled;
    char mode[16];                /**< standard | strict | permissive */
    struct {
        bool enabled;
        char type[32];            /**< seccomp | gvisor | none */
    } sandbox;
    struct {
        char model[32];           /**< rbac | abac | custom */
        uint32_t cache_ttl_seconds;
    } permission;
    struct {
        bool enabled;
        char log_path[256];
    } audit;
} agentos_security_config_t;

/* ── 多 Agent 配置 ── */
typedef struct {
    bool enabled;
    uint32_t max_concurrent_agents;
    struct {
        char protocol[16];        /**< a2a | jsonrpc | custom */
    } communication;
    struct {
        char default_pattern[32]; /**< orchestrator | consensus | competition */
    } collaboration;
    struct {
        bool enabled;
        char default_isolation[16]; /**< process | thread | container */
    } lanes;
    /* P1.6: Checkpoint 配置 */
    bool checkpoint_enabled;
    uint32_t checkpoint_interval_ms;
    uint32_t checkpoint_interval_turns;
    char checkpoint_path[256];
} agentos_multi_agent_config_t;

/* ── 网关配置 ── */
typedef struct {
    bool enabled;
    struct {
        uint16_t port;
    } http;
    struct {
        bool enabled;
    } websocket;
    struct {
        bool enabled;
        bool enable_progress;
        bool enable_cancellation;
    } mcp;
    struct {
        bool enabled;
        uint32_t default_timeout_ms;
    } a2a;
    struct {
        bool enabled;
    } openai_compat;
} agentos_gateway_config_t;

/* ── Hook 配置 ── */
#define AGENTOS_HOOK_MAX_DIRS 16
#define AGENTOS_HOOK_MAX_GLOBAL 32

typedef struct {
    char hook[64];                /**< Hook 名称 */
    int32_t priority;             /**< 优先级 */
} agentos_hook_entry_t;

typedef struct {
    bool enabled;
    char hook_dirs[AGENTOS_HOOK_MAX_DIRS][256];
    uint32_t hook_dir_count;
    struct {
        agentos_hook_entry_t on_tool_call[AGENTOS_HOOK_MAX_GLOBAL];
        uint32_t on_tool_call_count;
        agentos_hook_entry_t on_llm_request[AGENTOS_HOOK_MAX_GLOBAL];
        uint32_t on_llm_request_count;
        agentos_hook_entry_t on_agent_init[AGENTOS_HOOK_MAX_GLOBAL];
        uint32_t on_agent_init_count;
    } global_hooks;
} agentos_hooks_config_t;

/* ── 插件配置 ── */
#define AGENTOS_PLUGIN_MAX_DIRS 16

typedef struct {
    bool enabled;
    char plugin_dirs[AGENTOS_PLUGIN_MAX_DIRS][256];
    uint32_t plugin_dir_count;
    bool auto_discover;
} agentos_plugins_config_t;

/* ── 可观测性配置 ── */
typedef struct {
    bool enabled;
    uint16_t port;
} agentos_metrics_config_t;

typedef struct {
    bool enabled;
    char exporter[32];            /**< otlp | zipkin | none */
} agentos_tracing_config_t;

typedef struct {
    char level[16];               /**< debug | info | warn | error */
    char format[16];              /**< json | text */
} agentos_logging_config_t;

typedef struct {
    char endpoint[64];            /**< 健康检查端点 */
    char ready_endpoint[64];      /**< 就绪检查端点 */
} agentos_health_config_t;

typedef struct {
    agentos_metrics_config_t metrics;
    agentos_tracing_config_t tracing;
    agentos_logging_config_t logging;
    agentos_health_config_t health;
} agentos_observability_config_t;

/* ── 顶层配置结构体 ── */

typedef struct {
    char version[16];             /**< 配置版本 */
    agentos_kernel_config_t kernel;
    agentos_llm_config_t llm;
    agentos_memory_config_t memory;
    agentos_security_config_t security;
    agentos_multi_agent_config_t multi_agent;
    agentos_gateway_config_t gateway;
    agentos_hooks_config_t hooks;
    agentos_plugins_config_t plugins;
    agentos_observability_config_t observability;
} agentos_yaml_config_t;

/* ================================================================
 * 解析器 API
 * ================================================================ */

/**
 * @brief 获取默认配置
 * @param config 输出配置（填充默认值）
 */
void agentos_yaml_config_defaults(agentos_yaml_config_t *config);

/**
 * @brief 从 YAML 字符串解析配置
 * @param yaml_content YAML 文件内容（以 null 结尾）
 * @param config 输出配置（已有默认值，覆盖 YAML 中存在的字段）
 * @return 0 成功，非0失败
 */
int agentos_yaml_parse(const char *yaml_content, agentos_yaml_config_t *config);

/**
 * @brief 从文件加载并解析配置
 * @param yaml_path agentos.yaml 文件路径
 * @param config 输出配置
 * @return 0 成功，非0失败
 */
int agentos_yaml_load(const char *yaml_path, agentos_yaml_config_t *config);

/**
 * @brief 应用环境变量覆盖
 *
 * 环境变量命名规则：AGENTOS_<SECTION>_<KEY>_<SUBKEY>
 * 例如：AGENTOS_LLM_PROVIDERS_OPENAI_API_KEY
 *
 * 支持的覆盖：
 *   - AGENTOS_LLM_DEFAULT_PROVIDER
 *   - AGENTOS_LLM_PROVIDERS_<NAME>_API_KEY
 *   - AGENTOS_LLM_PROVIDERS_<NAME>_BASE_URL
 *   - AGENTOS_LLM_ROUTING_STRATEGY
 *   - AGENTOS_LLM_ROUTING_COST_BUDGET_DAILY_USD
 *   - AGENTOS_MEMORY_STORAGE_PATH
 *   - AGENTOS_GATEWAY_HTTP_PORT
 *   - AGENTOS_OBSERVABILITY_METRICS_PORT
 *   - AGENTOS_OBSERVABILITY_LOGGING_LEVEL
 *   - AGENTOS_SECURITY_MODE
 *   - AGENTOS_KERNEL_MEMORY_MAX_ALLOC_MB
 *   - AGENTOS_KERNEL_IPC_MAX_MESSAGE_SIZE
 *   - AGENTOS_KERNEL_IPC_SHM_POOL_SIZE_MB
 *   - AGENTOS_KERNEL_SCHEDULER_MAX_TASKS
 *   - AGENTOS_KERNEL_SCHEDULER_TIME_SLICE_MS
 *   - AGENTOS_KERNEL_MEMORY_OOM_WATERMARK_PERCENT
 *   - AGENTOS_MEMORY_PROVIDER
 *   - AGENTOS_MULTI_AGENT_MAX_CONCURRENT
 *   - AGENTOS_SECURITY_SANDBOX_TYPE
 *   - AGENTOS_SECURITY_AUDIT_LOG_PATH
 *   - AGENTOS_OBSERVABILITY_TRACING_EXPORTER
 *
 * @param config 配置（被修改）
 * @return 0 成功，非0失败
 */
int agentos_yaml_env_override(agentos_yaml_config_t *config);

/**
 * @brief 释放配置中动态分配的内存
 * @param config 配置
 */
void agentos_yaml_config_free(agentos_yaml_config_t *config);

/**
 * @brief 验证配置完整性
 * @param config 配置
 * @return 0 有效，非0 返回第一个发现的错误码
 *
 * 错误码：
 *   -1: config 为 NULL
 *   -2: version 为空
 *   -3: llm.default_provider 为空
 *   -4: llm.provider_count 为 0
 *   -5: llm.routing.strategy 为空
 *   -6: llm.routing.fallback_count 为 0
 *   -7: llm.cache.ttl_seconds 为 0
 *   -8: llm.cache.max_entries 为 0
 *   -9: memory.storage_path 为空
 *  -10: security.mode 为空
 *  -11: gateway.http.port 为 0
 *  -12: observability.metrics.port 为 0
 *  -13: kernel.memory.oom_watermark_percent > 100
 *  -14: kernel.memory.oom_watermark_percent < 50
 *  -15: kernel.ipc.max_message_size 为 0
 *  -16: kernel.ipc.shm_pool_size_mb 为 0
 *  -17: kernel.scheduler.max_tasks 为 0
 *  -18: kernel.scheduler.time_slice_ms 为 0
 *  -19: kernel.memory.max_alloc_mb 为 0
 *  -20: multi_agent.max_concurrent_agents 为 0
 *  -21: multi_agent.communication.protocol 为空
 *  -22: multi_agent.collaboration.default_pattern 为空
 *  -23: hooks.global_hooks 条目名称为空
 *  -24: observability.logging.level 值无效
 *  -25: security.mode 值无效
 *  -26: memory.mode 值无效
 */
int agentos_yaml_validate(const agentos_yaml_config_t *config);

/**
 * @brief 平台路径映射（P1.12.4）
 *
 * 将 agentos.yaml 中的 Linux 约定路径自动映射到当前平台：
 *   - /var/lib/agentos/ → %ProgramData%\AgentRT\ (Windows)
 *   - /var/log/agentos/ → %ProgramData%\AgentRT\logs\ (Windows)
 *   - /etc/agentos/     → %ProgramData%\AgentRT\config\ (Windows)
 *   - /usr/lib/agentos/ → %ProgramData%\AgentRT\lib\ (Windows)
 *   - ~/.agentos/       → %APPDATA%\AgentRT\ (Windows)
 *
 * 在 agentos_yaml_load() 之后调用此函数以调整路径。
 *
 * @param config 配置（被原地修改）
 * @return 0 成功，非0失败
 */
int agentos_yaml_resolve_platform_paths(agentos_yaml_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREE_YAML_LOADER_H */