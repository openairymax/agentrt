/**
 * @file compensation.h
 * @brief 补偿事务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_COMPENSATION_H
#define AGENTOS_COMPENSATION_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 补偿事务条目
 */
typedef struct agentos_compensation_entry {
    char* action_id;
    char* compensator_id;
    void* input;
    size_t input_size;
    void (*input_free_fn)(void*);
    struct agentos_compensation_entry* next;
} agentos_compensation_entry_t;

/**
 * @brief 补偿事务管理器
 */
typedef struct agentos_compensation {
    agentos_compensation_entry_t* entries;
    size_t entry_count;
    pthread_mutex_t mutex;
    char** human_queue;
    size_t human_queue_size;
} agentos_compensation_t;

/**
 * @brief 补偿执行结果
 */
typedef struct agentos_compensation_result {
    agentos_error_t status;
    char* error_message;
    int requires_human;
} agentos_compensation_result_t;

/**
 * @brief 创建补偿事务管理器
 * @param out_manager [out] 输出管理器句柄
 * @return agentos_error_t AGENTOS_SUCCESS 成功
 */
AGENTOS_API agentos_error_t agentos_compensation_create(
    agentos_compensation_t** out_manager);

/**
 * @brief 销毁补偿事务管理器
 * @param manager [in] 管理器句柄
 */
AGENTOS_API void agentos_compensation_destroy(
    agentos_compensation_t* manager);

/**
 * @brief 注册可补偿操作
 * @param manager [in] 补偿管理器
 * @param action_id [in] 操作ID
 * @param compensator_id [in] 补偿执行单元ID
 * @param input [in] 原始输入
 * @return agentos_error_t AGENTOS_SUCCESS 成功
 */
AGENTOS_API agentos_error_t agentos_compensation_register(
    agentos_compensation_t* manager,
    const char* action_id,
    const char* compensator_id,
    const void* input);

/**
 * @brief 执行补偿（回滚）
 * @param manager [in] 补偿管理器
 * @param action_id [in] 操作ID
 * @return agentos_error_t AGENTOS_SUCCESS 成功
 */
AGENTOS_API agentos_error_t agentos_compensation_compensate(
    agentos_compensation_t* manager,
    const char* action_id);

/**
 * @brief 获取待人工介入的队列
 * @param manager [in] 补偿管理器
 * @param out_actions [out] 输出操作ID数组
 * @param out_count [out] 输出数量
 * @return agentos_error_t AGENTOS_SUCCESS 成功
 */
AGENTOS_API agentos_error_t agentos_compensation_get_human_queue(
    agentos_compensation_t* manager,
    char*** out_actions,
    size_t* out_count);

/**
 * @brief 释放补偿结果
 * @param result [in] 补偿结果
 */
AGENTOS_API void agentos_compensation_result_free(
    agentos_compensation_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMPENSATION_H */
