/**
 * @file strategy.h
 * @brief 协同策略创建函数的统一声明和内部类型定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_COORDINATOR_STRATEGIES_H
#define AGENTOS_COORDINATOR_STRATEGIES_H

#include "cognition.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 内部协调器基础类型 ==================== */

/**
 * @brief 协调上下文（传递给coordinate函数）
 *
 * 包含协调过程中需要的元数据和状态信息。
 * 此结构为内部使用，不应直接暴露给API用户。
 */
typedef struct agentos_coordination_context {
    uint32_t context_flags;           /**< 上下文标志位 */
    void* context_user_data;          /**< 用户自定义数据 */
    size_t context_timeout_ms;        /**< 超时时间（毫秒） */
} agentos_coordination_context_t;

/**
 * @brief 协调器基础结构（内部继承用）
 *
 * 所有具体协调器实现都应以此为基础，
 * 通过包含此结构来实现"继承"效果。
 * 此结构为内部使用，对外隐藏实现细节。
 */
typedef struct agentos_coordinator_base {
    void* data;                       /**< 私有数据指针（指向具体实现的私有数据） */
    
    /**
     * @brief 协调执行函数
     * @param base 基础结构指针
     * @param context 协调上下文
     * @param inputs 输入字符串数组（多个模型的输出）
     * @param input_count 输入数量
     * @param out_result 输出协调结果（调用者负责释放）
     * @return 错误码
     */
    agentos_error_t (*coordinate)(
        struct agentos_coordinator_base* base,
        const agentos_coordination_context_t* context,
        const char** inputs,
        size_t input_count,
        char** out_result
    );
    
    /**
     * @brief 销毁函数
     * @param base 基础结构指针
     */
    void (*destroy)(struct agentos_coordinator_base* base);
} agentos_coordinator_base_t;

/* ==================== 公共API：协同策略工厂函数 ==================== */

/**
 * @brief 创建双模型协同策略（1主+2辅）
 * @param primary_model 主模型名称
 * @param secondary1 辅模型1名称
 * @param secondary2 辅模型2名称
 * @param llm LLM服务客户端句柄
 * @return 策略对象，失败返回NULL
 */
agentos_coordinator_strategy_t* agentos_dual_model_coordinator_create(
    const char* primary_model,
    const char* secondary1,
    const char* secondary2,
    agentos_llm_service_t* llm);

/**
 * @brief 创建多数投票协同策略
 * @param model_names 模型名称数组
 * @param model_count 模型数量
 * @param llm LLM服务客户端
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_majority_coordinator_create(
    const char** model_names,
    size_t model_count,
    agentos_llm_service_t* llm);

/**
 * @brief 创建加权融合策略
 * @param model_names 模型名称数组
 * @param weights 权重数组（总和不必为1）
 * @param model_count 数量
 * @param llm LLM服务客户端
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_weighted_coordinator_create(
    const char** model_names,
    const float* weights,
    size_t model_count,
    agentos_llm_service_t* llm);

/**
 * @brief 创建外部仲裁策略（模型仲裁）
 * @param arbiter_model 仲裁模型名称
 * @param llm LLM服务客户端
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_arbiter_model_create(
    const char* arbiter_model,
    agentos_llm_service_t* llm);

/**
 * @brief 创建外部仲裁策略（人工仲裁）
 * @param callback 人工回调函数，接收问题并填充答案
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_arbiter_human_create(
    void (*callback)(const char* question, char* answer, size_t max_len));

/* ==================== 内部API：协调器创建函数 ==================== */

/**
 * @brief 创建外部仲裁协调器（内部实现接口）
 * @param arbiter_model 仲裁模型名称（可为NULL表示人工）
 * @param human_callback 人工回调函数（可为NULL）
 * @param out_base 输出基础协调器
 * @return 错误码
 */
agentos_error_t agentos_coordinator_arbiter_create(
    const char* arbiter_model,
    void (*human_callback)(const char* question, char* answer, size_t max_len),
    agentos_coordinator_base_t** out_base);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COORDINATOR_STRATEGIES_H */
