/**
 * @file token.h
 * @brief Token计数与预算管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_UTILS_TOKEN_H
#define AGENTOS_UTILS_TOKEN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_token_counter agentos_token_counter_t;

/**
 * @brief 创建Token计数器
 * @param model_name 模型名称（如 "gpt-4"）
 * @return 计数器句柄，失败返回 NULL
 */
agentos_token_counter_t* agentos_token_counter_create(const char* model_name);

// From data intelligence emerges. by spharx
/**
 * @brief 销毁计数器
 */
void agentos_token_counter_destroy(agentos_token_counter_t* counter);

/**
 * @brief 计算文本的Token数量
 * @param counter 计数器
 * @param text 文本
 * @return Token数量，失败返回 (size_t)-1
 */
size_t agentos_token_counter_count(agentos_token_counter_t* counter, const char* text);

/**
 * @brief 批量计算多个文本的Token数量
 * @param counter 计数器
 * @param texts 文本数组
 * @param count 文本数量
 * @param out_counts 输出Token数量数组
 * @return 0 成功，-1 失败
 */
size_t agentos_token_counter_count_batch(agentos_token_counter_t* counter, const char** texts, size_t count, size_t* out_counts);

/**
 * @brief 截断文本到指定Token数
 * @param counter 计数器
 * @param text 输入文本
 * @param max_tokens 最大Token数
 * @param side 截断侧："left","right","middle"
 * @return 新分配的截断文本，失败返回 NULL
 */
char* agentos_token_counter_truncate(agentos_token_counter_t* counter, const char* text, size_t max_tokens, const char* side);

/**
 * @brief Token预算句柄
 */
typedef struct agentos_token_budget agentos_token_budget_t;

/**
 * @brief 创建Token预算
 * @param max_tokens 最大Token数
 * @return 预算句柄，失败返回 NULL
 */
agentos_token_budget_t* agentos_token_budget_create(size_t max_tokens);

/**
 * @brief 销毁预算
 */
void agentos_token_budget_destroy(agentos_token_budget_t* budget);

/**
 * @brief 添加Token消耗
 * @param budget 预算
 * @param input_tokens 输入Token数
 * @param output_tokens 输出Token数
 * @return 0 成功，-1 超出预算
 */
int agentos_token_budget_add(agentos_token_budget_t* budget, size_t input_tokens, size_t output_tokens);

/**
 * @brief 获取剩余Token数
 * @return 剩余Token数
 */
size_t agentos_token_budget_remaining(agentos_token_budget_t* budget);

/**
 * @brief 重置预算
 */
void agentos_token_budget_reset(agentos_token_budget_t* budget);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_TOKEN_H */