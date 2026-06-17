/**
 * @file llm_router.h
 * @brief LLM 路由器接口
 *
 * LLM Router 负责根据请求特征（复杂度、成本、延迟等）
 * 将 LLM 请求路由到最合适的提供商和模型。
 *
 * 路由策略：
 *   - COMPLEXITY_BASED:  基于任务复杂度路由
 *   - COST_OPTIMIZED:    成本优化路由
 *   - LATENCY_OPTIMIZED: 延迟优化路由
 *   - FALLBACK:          降级路由（主提供商失败时切换）
 *   - ROUND_ROBIN:       轮询路由
 *
 * @owner team-A
 * @see contracts/contract_A_B.h 第3节（协议路由表）
 */

#ifndef AGENTOS_DAEMON_LLM_D_ROUTER_LLM_ROUTER_H
#define AGENTOS_DAEMON_LLM_D_ROUTER_LLM_ROUTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 路由策略 ── */

typedef enum {
    LLM_ROUTE_COMPLEXITY  = 0,  /**< 基于复杂度路由 */
    LLM_ROUTE_COST        = 1,  /**< 成本优化路由 */
    LLM_ROUTE_LATENCY     = 2,  /**< 延迟优化路由 */
    LLM_ROUTE_FALLBACK    = 3,  /**< 降级路由 */
    LLM_ROUTE_ROUND_ROBIN = 4,  /**< 轮询路由 */
    LLM_ROUTE_COUNT       = 5   /**< 策略总数 */
} llm_route_strategy_t;

/* ── 模型能力标志 ── */

typedef enum {
    LLM_CAP_CHAT           = 0x0001,  /**< 对话 */
    LLM_CAP_COMPLETION     = 0x0002,  /**< 补全 */
    LLM_CAP_EMBEDDING      = 0x0004,  /**< 嵌入 */
    LLM_CAP_FUNCTION_CALL  = 0x0008,  /**< 函数调用 */
    LLM_CAP_VISION         = 0x0010,  /**< 视觉 */
    LLM_CAP_STREAMING      = 0x0020,  /**< 流式 */
    LLM_CAP_JSON_MODE      = 0x0040,  /**< JSON 模式 */
    LLM_CAP_EXTENDED_THINK = 0x0080,  /**< 扩展思考 */
    LLM_CAP_CODE_EXEC      = 0x0100   /**< 代码执行 */
} llm_capability_t;

/* ── 提供商端点 ── */

typedef struct {
    char provider_name[64];      /**< 提供商名称 */
    char model_name[64];         /**< 模型名称 */
    char endpoint[256];          /**< API 端点 */
    char api_key_env[64];        /**< API Key 环境变量名 */
    uint32_t capabilities;       /**< 能力标志 (llm_capability_t 位掩码) */
    uint32_t context_window;     /**< 上下文窗口大小 */
    double cost_per_1k_input;    /**< 输入每 1K token 成本 (USD) */
    double cost_per_1k_output;   /**< 输出每 1K token 成本 (USD) */
    uint32_t avg_latency_ms;     /**< 平均延迟 (ms) */
    uint32_t rate_limit_rpm;     /**< 速率限制 (请求/分钟) */
    bool enabled;                /**< 是否启用 */
    int priority;                /**< 优先级 */
} llm_endpoint_t;

/* ── 路由请求 ── */

typedef struct {
    const char *prompt;          /**< 提示文本 */
    size_t prompt_len;           /**< 提示长度 */
    uint32_t required_caps;     /**< 必需能力 (llm_capability_t 位掩码) */
    uint32_t max_tokens;        /**< 最大 token 数 */
    double max_cost;             /**< 最大成本 (USD, 0=无限制) */
    uint32_t max_latency_ms;    /**< 最大延迟 (ms, 0=无限制) */
    llm_route_strategy_t strategy; /**< 路由策略 */
    char preferred_provider[64]; /**< 首选提供商（空字符串=自动） */
} llm_route_request_t;

/* ── 路由结果 ── */

typedef struct {
    char provider_name[64];      /**< 选中的提供商 */
    char model_name[64];         /**< 选中的模型 */
    char endpoint[256];          /**< 选中的端点 */
    double estimated_cost;       /**< 预估成本 (USD) */
    uint32_t estimated_latency_ms; /**< 预估延迟 (ms) */
    llm_route_strategy_t strategy_used; /**< 实际使用的策略 */
    int confidence;              /**< 路由置信度 (0-100) */
    char fallback_provider[64];  /**< 降级提供商 */
    char fallback_model[64];     /**< 降级模型 */
} llm_route_result_t;

/* ── 路由器统计 ── */

typedef struct {
    uint64_t total_requests;     /**< 总请求数 */
    uint64_t routed_count[5];    /**< 各策略路由计数 */
    uint64_t fallback_count;     /**< 降级次数 */
    uint64_t error_count;        /**< 错误次数 */
    double total_cost;           /**< 总成本 (USD) */
    uint64_t total_tokens;       /**< 总 token 数 */
} llm_router_stats_t;

/* ── 路由器 API ── */

/**
 * @brief 初始化 LLM 路由器
 * @param config_path 配置文件路径
 * @return 0 成功，非0失败
 */
int llm_router_init(const char *config_path);

/**
 * @brief 销毁 LLM 路由器
 */
void llm_router_destroy(void);

/**
 * @brief 注册提供商端点
 * @param endpoint 端点信息
 * @return 0 成功，非0失败
 */
int llm_router_register_endpoint(const llm_endpoint_t *endpoint);

/**
 * @brief 注销提供商端点
 * @param provider_name 提供商名称
 * @param model_name    模型名称
 * @return 0 成功，非0失败
 */
int llm_router_unregister_endpoint(const char *provider_name, const char *model_name);

/**
 * @brief 路由 LLM 请求
 * @param request 路由请求
 * @param result  路由结果
 * @return 0 成功，非0失败
 */
int llm_router_route(const llm_route_request_t *request, llm_route_result_t *result);

/**
 * @brief 获取路由器统计
 * @param stats 输出统计
 * @return 0 成功，非0失败
 */
int llm_router_get_stats(llm_router_stats_t *stats);

/**
 * @brief 设置默认路由策略
 * @param strategy 路由策略
 * @return 0 成功，非0失败
 */
int llm_router_set_default_strategy(llm_route_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_LLM_D_ROUTER_LLM_ROUTER_H */
