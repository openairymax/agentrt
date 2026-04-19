/**
 * @file llm_client.h
 * @brief LLM客户端接口（简化版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LLM_CLIENT_H
#define AGENTOS_LLM_CLIENT_H

#include "../../corekern/include/agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LLM服务句柄
 */
typedef struct agentos_llm_service agentos_llm_service_t;

/**
 * @brief LLM配置
 */
typedef struct agentos_llm_config {
    const char* model_name;
    const char* api_key;
    const char* base_url;
    uint32_t timeout_ms;
    float temperature;
    uint32_t max_tokens;
} agentos_llm_config_t;

/**
 * @brief LLM请求结构
 */
typedef struct agentos_llm_request {
    const char* model;       /**< 模型名称 */
    const char* prompt;      /**< 提示文本 */
    float temperature;       /**< 温度参数 (0.0-2.0) */
    uint32_t max_tokens;     /**< 最大生成token数 */
    const char* system_prompt; /**< 可选系统提示 */
} agentos_llm_request_t;

/**
 * @brief LLM响应结构
 */
typedef struct agentos_llm_response {
    char* text;              /**< 生成的文本 */
    uint32_t usage_tokens;   /**< 使用token数 */
    uint32_t total_tokens;   /**< 总token数 */
    uint32_t finish_reason;  /**< 完成原因 */
} agentos_llm_response_t;

/* ==================== 双思考系统接口 (DS-004) ==================== */

/**
 * @brief 双思考会话配置
 *
 * 配置流式批判循环的参数：
 * - S2生成内容 → S1验证 → 纠正 → 重试（直到通过或超限）
 */
typedef struct agentos_dual_think_config {
    int max_corrections;          /**< 每步最大修正次数 (默认3) */
    float acceptance_threshold;   /**< S1验证通过阈值 (默认0.7) */
    uint32_t timeout_ms;         /**< 单步超时时间 (默认30000) */
    int enable_context_window;    /**< 是否启用Context Window (默认1) */
    size_t context_window_tokens; /**< Context Window容量 (默认8192) */
    int enable_working_memory;    /**< 是否启用Working Memory (默认1) */
    size_t wm_capacity;           /**< Working Memory容量 (默认64) */
    const char* system_prompt;    /**< 可选的系统级提示词 */
    float temperature_s2;         /**< S2生成温度 (默认0.7) */
    float temperature_s1;         /**< S1验证温度 (默认0.3) */
} agentos_dual_think_config_t;

/**
 * @brief 双思考会话结果
 */
typedef struct agentos_dual_think_result {
    char* final_output;           /**< 最终输出文本 */
    size_t final_output_len;      /**< 最终输出长度 */
    uint32_t total_steps;         /**< 总执行步骤数 */
    uint32_t total_corrections;   /**< 总修正次数 */
    uint64_t elapsed_ns;          /**< 总耗时(纳秒) */
    float final_confidence;       /**< 最终置信度 */
    int accepted;                 /**< 是否被S1接受 (1=是) */
    char* chain_stats_json;       /**< 思考链路统计(JSON格式) */
    size_t chain_stats_len;
} agentos_dual_think_result_t;

/**
 * @brief 执行双思考推理（流式批判循环）
 *
 * 这是DS-004的核心函数。完整实现Phase 2流式批判：
 * 1. 创建思考链路(Context Window + Working Memory)
 * 2. S2调用LLM生成内容
 * 3. S1(元认知)多维度评估
 * 4. 若未通过→S2根据critique重新生成
 * 5. 重复直到通过或达到max_corrections
 *
 * @param service LLM服务句柄
 * @param config 双思考配置 (NULL使用默认值)
 * @param user_prompt 用户输入提示
 * @param out_result 输出结果 (调用者需用agentos_llm_dual_result_free释放)
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_llm_dual_think(
    agentos_llm_service_t* service,
    const agentos_dual_think_config_t* config,
    const char* user_prompt,
    agentos_dual_think_result_t** out_result);

/**
 * @brief 释放双思考结果
 */
void agentos_llm_dual_result_free(agentos_dual_think_result_t* result);

/**
 * @brief 使用默认配置执行双思考推理（简化接口）
 */
agentos_error_t agentos_llm_dual_think_simple(
    agentos_llm_service_t* service,
    const char* user_prompt,
    char** out_response);

/**
 * @brief 创建LLM服务
 */
agentos_error_t agentos_llm_service_create(
    const agentos_llm_config_t* manager,
    agentos_llm_service_t** out_service);

/**
 * @brief 销毁LLM服务
 */
void agentos_llm_service_destroy(agentos_llm_service_t* service);

/**
 * @brief 调用LLM生成响应 (简化接口)
 */
agentos_error_t agentos_llm_service_call(
    agentos_llm_service_t* service,
    const char* prompt,
    char** out_response);

/**
 * @brief 检查LLM服务是否已初始化并可用
 */
int agentos_llm_service_is_available(const agentos_llm_service_t* service);

/**
 * @brief 完整LLM调用接口
 */
agentos_error_t agentos_llm_complete(
    agentos_llm_service_t* service,
    const agentos_llm_request_t* request,
    agentos_llm_response_t** out_response);

/**
 * @brief 释放LLM响应
 */
void agentos_llm_response_free(agentos_llm_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LLM_CLIENT_H */
