/**
 * @file execution.h
 * @brief 行动层公共接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_EXECUTION_H
#define AGENTOS_EXECUTION_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_execution_engine agentos_execution_engine_t;
typedef struct agentos_execution_unit agentos_execution_unit_t;
typedef struct agentos_task agentos_task_t;
typedef struct agentos_compensation agentos_compensation_t;

/**
 * @brief 任务状态枚举
 */
typedef enum {
    TASK_STATUS_PENDING = 0,
    TASK_STATUS_RUNNING,
    TASK_STATUS_SUCCEEDED,
    TASK_STATUS_FAILED,
    TASK_STATUS_CANCELLED,
    TASK_STATUS_RETRYING
} agentos_task_status_t;

/**
 * @brief 执行任务结构
 */
typedef struct agentos_task {
    char* task_id;                    /**< 任务ID */
    size_t id_len;                     /**< ID长度 */
    char* agent_id;                    /**< 分配的Agent ID */
    size_t agent_id_len;               /**< Agent ID长度 */
    agentos_task_status_t status;      /**< 任务状态 */
    void* input;                       /**< 输入数据 */
    void* output;                      /**< 输出数据 */
    uint64_t created_ns;               /**< 创建时间（纳秒） */
    uint64_t started_ns;               /**< 开始时间 */
    uint64_t completed_ns;             /**< 完成时间 */
    uint32_t timeout_ms;                /**< 超时时间 */
    uint32_t retry_count;               /**< 已重试次数 */
    uint32_t max_retries;               /**< 最大重试次数 */
    char* error_msg;                    /**< 错误信息 */
} agentos_task_t;

/**
 * @brief 执行单元基类（类似抽象接口）
 */
struct agentos_execution_unit {
    void* data;                        /**< 私有数据 */
    /**
     * @brief 执行方法
     * @param unit 单元对象
     * @param input 输入数据
     * @param out_output 输出数据（需分配）
     * @return agentos_error_t
     */
    agentos_error_t (*execute)(agentos_execution_unit_t* unit,
                               const void* input,
                               void** out_output);
    /**
     * @brief 释放单元资源
     */
    void (*destroy)(agentos_execution_unit_t* unit);
    /**
     * @brief 获取单元元数据
     */
    const char* (*get_metadata)(agentos_execution_unit_t* unit);
};

/* ==================== 执行引擎接口 ==================== */

/**
 * @brief 创建执行引擎
 * @param max_concurrency 最大并发数
 * @param out_engine 输出引擎句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_create(
    uint32_t max_concurrency,
    agentos_execution_engine_t** out_engine);

/**
 * @brief 销毁执行引擎
 * @param engine 引擎句柄
 */
void agentos_execution_destroy(agentos_execution_engine_t* engine);

/**
 * @brief 注册执行单元
 * @param engine 执行引擎
 * @param unit_id 单元标识符
 * @param unit 执行单元对象
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_register_unit(
    agentos_execution_engine_t* engine,
    const char* unit_id,
    agentos_execution_unit_t* unit);

/**
 * @brief 注销执行单元
 * @param engine 执行引擎
 * @param unit_id 单元标识符
 */
void agentos_execution_unregister_unit(
    agentos_execution_engine_t* engine,
    const char* unit_id);

/**
 * @brief 提交任务执行
 * @param engine 执行引擎
 * @param task 任务描述（包含输入、超时等）
 * @param out_task_id 输出任务ID
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_submit(
    agentos_execution_engine_t* engine,
    const agentos_task_t* task,
    char** out_task_id);

/**
 * @brief 查询任务状态
 * @param engine 执行引擎
 * @param task_id 任务ID
 * @param out_status 输出状态
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_query(
    agentos_execution_engine_t* engine,
    const char* task_id,
    agentos_task_status_t* out_status);

/**
 * @brief 等待任务完成
 * @param engine 执行引擎
 * @param task_id 任务ID
 * @param timeout_ms 超时时间（0表示无限等待）
 * @param out_result 输出结果（若成功，需调用 agentos_task_free 释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_wait(
    agentos_execution_engine_t* engine,
    const char* task_id,
    uint32_t timeout_ms,
    agentos_task_t** out_result);

/**
 * @brief 取消任务
 * @param engine 执行引擎
 * @param task_id 任务ID
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_cancel(
    agentos_execution_engine_t* engine,
    const char* task_id);

/**
 * @brief 获取任务结果
 * @param engine 执行引擎
 * @param task_id 任务ID
 * @param out_result 输出结果（需调用 agentos_task_free 释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_get_result(
    agentos_execution_engine_t* engine,
    const char* task_id,
    agentos_task_t** out_result);

/**
 * @brief 释放任务结果（递减引用计数，当计数为0时释放内部结构）
 * @param task 任务结构
 */
void agentos_task_free(agentos_task_t* task);

/* ==================== 补偿事务接口 ==================== */

/**
 * @brief 创建补偿事务管理器
 * @param out_manager 输出管理器句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_compensation_create(
    agentos_compensation_t** out_manager);

/**
 * @brief 销毁补偿管理器
 * @param manager 管理器句柄
 */
void agentos_compensation_destroy(agentos_compensation_t* manager);

/**
 * @brief 注册可补偿操作
 * @param manager 补偿管理器
 * @param action_id 操作ID
 * @param compensator_id 补偿执行单元ID
 * @param input 原始输入（用于补偿）
 * @return agentos_error_t
 */
agentos_error_t agentos_compensation_register(
    agentos_compensation_t* manager,
    const char* action_id,
    const char* compensator_id,
    const void* input);

/**
 * @brief 执行补偿（回滚）
 * @param manager 补偿管理器
 * @param action_id 操作ID
 * @return agentos_error_t
 */
agentos_error_t agentos_compensation_compensate(
    agentos_compensation_t* manager,
    const char* action_id);

/**
 * @brief 获取待人工介入的队列
 * @param manager 补偿管理器
 * @param out_actions 输出操作ID数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
agentos_error_t agentos_compensation_get_human_queue(
    agentos_compensation_t* manager,
    char*** out_actions,
    size_t* out_count);

/**
 * @brief 获取执行引擎健康状态
 * @param engine 执行引擎句柄
 * @param out_json 输出 JSON 状态字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_execution_health_check(
    agentos_execution_engine_t* engine,
    char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EXECUTION_H */