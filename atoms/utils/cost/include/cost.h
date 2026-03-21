/**
 * @file cost.h
 * @brief 成本预估与控制
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_UTILS_COST_H
#define AGENTOS_UTILS_COST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_cost_estimator agentos_cost_estimator_t;

/**
 * @brief 创建成本预估器
 * @param config_path 配置文件路径（YAML），若为 NULL 则使用内置默认
 * @return 预估器句柄，失败返回 NULL
 */
agentos_cost_estimator_t* agentos_cost_estimator_create(const char* config_path);

// From data intelligence emerges. by spharx
/**
 * @brief 销毁预估器
 */
void agentos_cost_estimator_destroy(agentos_cost_estimator_t* estimator);

/**
 * @brief 预估一次调用成本
 * @param estimator 预估器
 * @param model_name 模型名称
 * @param input_tokens 输入Token数
 * @param output_tokens 输出Token数
 * @return 成本（美元），失败返回 -1.0
 */
double agentos_cost_estimator_estimate(agentos_cost_estimator_t* estimator, const char* model_name, size_t input_tokens, size_t output_tokens);

typedef struct agentos_budget_controller agentos_budget_controller_t;

/**
 * @brief 创建预算控制器
 * @param max_cost_usd 最大成本（美元）
 * @param period_seconds 周期（秒）
 * @return 控制器句柄，失败返回 NULL
 */
agentos_budget_controller_t* agentos_budget_controller_create(double max_cost_usd, uint32_t period_seconds);

/**
 * @brief 销毁控制器
 */
void agentos_budget_controller_destroy(agentos_budget_controller_t* controller);

/**
 * @brief 消耗成本
 * @param controller 控制器
 * @param cost_usd 成本
 * @return 0 成功，-1 超出预算
 */
int agentos_budget_controller_consume(agentos_budget_controller_t* controller, double cost_usd);

/**
 * @brief 获取剩余预算
 * @return 剩余成本（美元）
 */
double agentos_budget_controller_remaining(agentos_budget_controller_t* controller);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_COST_H */