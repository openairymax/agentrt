/**
 * @file compensation.h
 * @brief 补偿事务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_COMPENSATION_H
#define AGENTRT_COMPENSATION_H

#include "agentrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 补偿事务条目
 */
typedef struct agentrt_compensation_entry {
    char *action_id;
    char *compensator_id;
    void *input;
    size_t input_size;
    void (*input_free_fn)(void *);
    struct agentrt_compensation_entry *next;
} agentrt_compensation_entry_t;

/**
 * @brief 补偿事务管理器
 */
typedef struct agentrt_compensation {
    agentrt_compensation_entry_t *entries;
    size_t entry_count;
    agentrt_mutex_t *lock;
    char **human_queue;
    size_t human_queue_size;
    size_t human_queue_capacity;
} agentrt_compensation_t;

/**
 * @brief 补偿执行结果
 */
typedef struct agentrt_compensation_result {
    agentrt_error_t status;
    char *error_message;
    int requires_human;
} agentrt_compensation_result_t;

/**
 * @brief 创建补偿事务管理器
 * @param out_manager [out] 输出管理器句柄
 * @return agentrt_error_t AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t agentrt_compensation_create(agentrt_compensation_t **out_manager);

/**
 * @brief 销毁补偿事务管理器
 * @param manager [in] 管理器句柄
 */
AGENTRT_API void agentrt_compensation_destroy(agentrt_compensation_t *manager);

/**
 * @brief 注册可补偿操作
 * @param manager [in] 补偿管理器
 * @param action_id [in] 操作ID
 * @param compensator_id [in] 补偿执行单元ID
 * @param input [in] 原始输入
 * @return agentrt_error_t AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t agentrt_compensation_register(agentrt_compensation_t *manager,
                                                          const char *action_id,
                                                          const char *compensator_id,
                                                          const void *input);

/**
 * @brief 执行补偿（回滚）
 * @param manager [in] 补偿管理器
 * @param action_id [in] 操作ID
 * @return agentrt_error_t AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t agentrt_compensation_compensate(agentrt_compensation_t *manager,
                                                            const char *action_id);

/**
 * @brief 获取待人工介入的队列
 * @param manager [in] 补偿管理器
 * @param out_actions [out] 输出操作ID数组
 * @param out_count [out] 输出数量
 * @return agentrt_error_t AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t agentrt_compensation_get_human_queue(agentrt_compensation_t *manager,
                                                                 char ***out_actions,
                                                                 size_t *out_count);

/**
 * @brief 释放补偿结果
 * @param result [in] 补偿结果
 */
AGENTRT_API void agentrt_compensation_result_free(agentrt_compensation_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_COMPENSATION_H */
