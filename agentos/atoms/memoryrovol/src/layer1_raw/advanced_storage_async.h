/**
 * @file advanced_storage_async.h
 * @brief L1 增强存储异步操作管理 - 异步队列、操作调度
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ADVANCED_STORAGE_ASYNC_H
#define AGENTOS_ADVANCED_STORAGE_ASYNC_H

#include "../../../atoms/corekern/include/agentos.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 异步操作状态
 */
typedef enum {
    ASYNC_OP_PENDING = 0,           /**< 等待中 */
    ASYNC_OP_RUNNING,               /**< 运行中 */
    ASYNC_OP_COMPLETED,             /**< 已完成 */
    ASYNC_OP_FAILED,                /**< 失败 */
    ASYNC_OP_CANCELLED              /**< 已取消 */
} async_operation_state_t;

/**
 * @brief 异步操作类型
 */
typedef enum {
    ASYNC_OP_WRITE = 0,             /**< 异步写入 */
    ASYNC_OP_READ,                  /**< 异步读取 */
    ASYNC_OP_DELETE,                /**< 异步删除 */
    ASYNC_OP_COMPRESS,              /**< 异步压缩 */
    ASYNC_OP_ENCRYPT,               /**< 异步加密 */
    ASYNC_OP_DECRYPT,               /**< 异步解密 */
    ASYNC_OP_VERIFY                 /**< 异步验证 */
} async_operation_type_t;

/**
 * @brief 异步操作结构
 */
typedef struct async_operation {
    char* id;                       /**< 操作 ID */
    async_operation_type_t type;    /**< 操作类型 */
    async_operation_state_t state;  /**< 操作状态 */
    void* input_data;               /**< 输入数据 */
    size_t input_size;              /**< 输入大小 */
    void* output_data;              /**< 输出数据 */
    size_t output_size;             /**< 输出大小 */
    agentos_error_t result;         /**< 操作结果 */
    uint64_t start_time;            /**< 开始时间 */
    uint64_t end_time;              /**< 结束时间 */
    void* user_context;             /**< 用户上下文 */
    void (*callback)(struct async_operation*); /**< 回调函数 */
    agentos_mutex_t* lock;          /**< 操作锁 */
    agentos_condition_t* cond;      /**< 操作条件变量 */
} async_operation_t;

/**
 * @brief 异步操作队列
 */
typedef struct async_queue {
    async_operation_t** operations; /**< 操作数组 */
    size_t capacity;                /**< 队列容量 */
    size_t size;                    /**< 当前大小 */
    size_t head;                    /**< 头部索引 */
    size_t tail;                    /**< 尾部索引 */
    agentos_mutex_t* lock;          /**< 队列锁 */
    agentos_condition_t* not_empty; /**< 非空条件变量 */
    agentos_condition_t* not_full;  /**< 非满条件变量 */
} async_queue_t;

/**
 * @brief 创建异步操作
 * @param type 操作类型
 * @param id 数据 ID
 * @param data 数据（可选）
 * @param data_size 数据大小
 * @param callback 回调函数（可选）
 * @param user_context 用户上下文（可选）
 * @return 异步操作，失败返回 NULL
 */
async_operation_t* advanced_async_op_create(async_operation_type_t type,
                                           const char* id,
                                           const void* data, size_t data_size,
                                           void (*callback)(async_operation_t*),
                                           void* user_context);

/**
 * @brief 销毁异步操作
 * @param op 异步操作
 */
void advanced_async_op_destroy(async_operation_t* op);

/**
 * @brief 等待异步操作完成
 * @param op 异步操作
 * @param timeout_ms 超时时间（毫秒）
 * @return AGENTOS_SUCCESS 操作完成，AGENTOS_ETIMEDOUT 超时，其他为错误码
 */
agentos_error_t advanced_async_op_wait(async_operation_t* op, uint32_t timeout_ms);

/**
 * @brief 创建异步队列
 * @param capacity 队列容量
 * @return 异步队列，失败返回 NULL
 */
async_queue_t* advanced_async_queue_create(size_t capacity);

/**
 * @brief 销毁异步队列
 * @param queue 异步队列
 */
void advanced_async_queue_destroy(async_queue_t* queue);

/**
 * @brief 向队列添加操作
 * @param queue 异步队列
 * @param op 异步操作
 * @param timeout_ms 超时时间（毫秒）
 * @return AGENTOS_SUCCESS 成功，AGENTOS_ETIMEDOUT 超时，其他为错误码
 */
agentos_error_t advanced_async_queue_push(async_queue_t* queue, async_operation_t* op, uint32_t timeout_ms);

/**
 * @brief 从队列取出操作
 * @param queue 异步队列
 * @param timeout_ms 超时时间（毫秒）
 * @return 异步操作，NULL 表示超时或错误
 */
async_operation_t* advanced_async_queue_pop(async_queue_t* queue, uint32_t timeout_ms);

/**
 * @brief 获取队列大小
 * @param queue 异步队列
 * @return 队列大小
 */
size_t advanced_async_queue_size(async_queue_t* queue);

/**
 * @brief 判断队列是否为空
 * @param queue 异步队列
 * @return 1 表示空，0 表示非空
 */
int advanced_async_queue_is_empty(async_queue_t* queue);

#endif /* AGENTOS_ADVANCED_STORAGE_ASYNC_H */
