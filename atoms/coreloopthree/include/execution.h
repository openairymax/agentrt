/**
 * @file execution.h
 * @brief 行动层公共接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_EXECUTION_H
#define AGENTOS_EXECUTION_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define EXECUTION_API_VERSION_MAJOR 1
#define EXECUTION_API_VERSION_MINOR 0
#define EXECUTION_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

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
    size_t task_id_len;               /**< ID长度 */
    char* task_agent_id;              /**< 分配的Agent ID */
    size_t task_agent_id_len;         /**< Agent ID长度 */
    agentos_task_status_t task_status; /**< 任务状态 */
    void* task_input;                 /**< 输入数据 */
    void* task_output;                /**< 输出数据 */
    uint64_t task_created_ns;         /**< 创建时间（纳秒） */
    uint64_t task_started_ns;         /**< 开始时间 */
    uint64_t task_completed_ns;       /**< 完成时间 */
    uint32_t task_timeout_ms;         /**< 超时时间 */
    uint32_t task_retry_count;        /**< 已重试次数 */
    uint32_t task_max_retries;        /**< 最大重试次数 */
    char* task_error_msg;             /**< 错误信息 */
} agentos_task_t;

/**
 * @brief 执行单元基类（类似抽象接口）
 */
struct agentos_execution_unit {
    void* execution_unit_data;                        /**< 私有数据 */
    /**
     * @brief 执行方法
     * @param unit 单元对象
     * @param input 输入数据
     * @param out_output 输出数据（需分配）
     * @return agentos_error_t
     */
    agentos_error_t (*execution_unit_execute)(agentos_execution_unit_t* unit,
                               const void* input,
                               void** out_output);
    /**
     * @brief 释放单元资源
     */
    void (*execution_unit_destroy)(agentos_execution_unit_t* unit);
    /**
     * @brief 获取单元元数据
     */
    const char* (*execution_unit_get_metadata)(agentos_execution_unit_t* unit);
};

/* ==================== 执行引擎接口 ==================== */

/**
 * @brief 创建执行引擎
 * 
 * @param max_concurrency [in] 最大并发数
 * @param out_engine [out] 输出引擎句柄（调用者负责销毁）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_engine 由调用者负责通过 agentos_execution_destroy() 释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_execution_destroy()
 */
AGENTOS_API agentos_error_t agentos_execution_create(
    uint32_t max_concurrency,
    agentos_execution_engine_t** out_engine);

/**
 * @brief 销毁执行引擎
 * 
 * @param engine [in] 引擎句柄（非NULL）
 * 
 * @ownership 释放 engine 及其内部所有资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_execution_create()
 */
AGENTOS_API void agentos_execution_destroy(agentos_execution_engine_t* engine);

/**
 * @brief 注册执行单元
 * 
 * @param engine [in] 执行引擎（非NULL）
 * @param unit_id [in] 单元标识符（非NULL）
 * @param unit [in] 执行单元对象（非NULL）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership 引擎不接管 unit 的所有权，调用者仍需负责其生命周期
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_execution_unregister_unit()
 */
AGENTOS_API agentos_error_t agentos_execution_register_unit(
    agentos_execution_engine_t* engine,
    const char* name,
    agentos_execution_unit_t unit);

/**
 * @brief 注销执行单元
 * 
 * @param engine [in] 执行引擎（非NULL）
 * @param unit_id [in] 单元标识符（非NULL）
 * 
 * @ownership 不会释放单元对象，调用者仍需负责其生命周期
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_execution_register_unit()
 */
AGENTOS_API void agentos_execution_unregister_unit(
    agentos_execution_engine_t* engine,
    const char* name);

/**
 * @brief 提交任务执行
 * 
 * @param engine [in] 执行引擎（非NULL）
 * @param task [in] 任务描述（包含输入、超时等，非NULL）
 * @param out_task_id [out] 输出任务ID（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_task_id 由调用者负责释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 * 
 * @concurrency 并发合约：
 * - 该函数是线程安全的，可以在多个线程同时调用
 * - 内部使用互斥锁保护任务队列，避免并发冲突
 * - 调用者无需额外的同步措施
 * 
 * @see agentos_execution_query(), agentos_execution_wait()
 */
AGENTOS_API agentos_error_t agentos_execution_submit(
    agentos_execution_engine_t* engine,
    const agentos_task_t* task,
    char** out_task_id);

/**
 * @brief 查询任务状态
 * 
 * @param engine [in] 执行引擎（非NULL）
 * @param task_id [in] 任务ID（非NULL）
 * @param out_status [out] 输出状态
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 * @see agentos_execution_submit()
 */
AGENTOS_API agentos_error_t agentos_execution_query(
    agentos_execution_engine_t* engine,
    const char* task_id,
    agentos_task_status_t* out_status);

/**
 * @brief 等待任务完成
 * 
 * @param engine [in] 执行引擎（非NULL）
 * @param task_id [in] 任务ID（非NULL）
 * @param timeout_ms [in] 超时时间（0表示无限等待）
 * @param out_result [out] 输出结果（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_result 由调用者负责通过 agentos_task_free() 释放
 * @threadsafe 是（内部使用条件变量和互斥锁）
 * @reentrant 否
 * @see agentos_task_free()
 */
AGENTOS_API agentos_error_t agentos_execution_wait(
    agentos_execution_engine_t* engine,
    const char* task_id,
    uint32_t timeout_ms,
    agentos_task_t** out_result);

/**
 * @brief 取消任务
 * 
 * @param engine [in] 执行引擎（非NULL）
 * @param task_id [in] 任务ID（非NULL）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 * @see agentos_execution_submit()
 */
AGENTOS_API agentos_error_t agentos_execution_cancel(
    agentos_execution_engine_t* engine,
    const char* task_id);

/**
 * @brief 获取任务结果
 * 
 * @param engine [in] 执行引擎（非NULL）
 * @param task_id [in] 任务ID（非NULL）
 * @param out_result [out] 输出结果（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_result 由调用者负责通过 agentos_task_free() 释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 * @see agentos_task_free()
 */
AGENTOS_API agentos_error_t agentos_execution_get_result(
    agentos_execution_engine_t* engine,
    const char* task_id,
    agentos_task_t** out_result);

/**
 * @brief 释放任务结果（递减引用计数，当计数为0时释放内部结构）
 * 
 * @param task [in] 任务结构（可为NULL）
 * 
 * @ownership 释放 task 及其内部所有资源
 * @threadsafe 是（内部使用原子操作管理引用计数）
 * @reentrant 否
 * @see agentos_execution_wait(), agentos_execution_get_result()
 */
AGENTOS_API void agentos_task_free(agentos_task_t* task);

/* ==================== 补偿事务接口 ==================== */

/**
 * @brief 创建补偿事务管理器
 * 
 * @param out_manager [out] 输出管理器句柄（调用者负责销毁）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_manager 由调用者负责通过 agentos_compensation_destroy() 释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_compensation_destroy()
 */
AGENTOS_API agentos_error_t agentos_compensation_create(
    agentos_compensation_t** out_manager);

/**
 * @brief 销毁补偿管理器
 * 
 * @param manager [in] 管理器句柄（非NULL）
 * 
 * @ownership 释放 manager 及其内部所有资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_compensation_create()
 */
AGENTOS_API void agentos_compensation_destroy(agentos_compensation_t* manager);

/**
 * @brief 注册可补偿操作
 * 
 * @param manager [in] 补偿管理器（非NULL）
 * @param action_id [in] 操作ID（非NULL）
 * @param compensator_id [in] 补偿执行单元ID（非NULL）
 * @param input [in] 原始输入（用于补偿，可为NULL）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership 管理器会复制 input 的内容，调用者仍需负责其原始资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_compensation_compensate()
 */
AGENTOS_API agentos_error_t agentos_compensation_register(
    agentos_compensation_t* manager,
    const char* action_id,
    const char* compensator_id,
    const void* input);

/**
 * @brief 执行补偿（回滚）
 * 
 * @param manager [in] 补偿管理器（非NULL）
 * @param action_id [in] 操作ID（非NULL）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_compensation_register()
 */
AGENTOS_API agentos_error_t agentos_compensation_compensate(
    agentos_compensation_t* manager,
    const char* action_id);

/**
 * @brief 获取待人工介入的队列
 * 
 * @param manager [in] 补偿管理器（非NULL）
 * @param out_actions [out] 输出操作ID数组（调用者负责释放）
 * @param out_count [out] 输出数量
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_actions 由调用者负责释放，包括数组本身和数组中的每个元素
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_compensation_get_human_queue(
    agentos_compensation_t* manager,
    char*** out_actions,
    size_t* out_count);

/**
 * @brief 获取执行引擎健康状态
 * 
 * @param engine [in] 执行引擎句柄（非NULL）
 * @param out_json [out] 输出 JSON 状态字符串（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_json 由调用者负责释放
 * @threadsafe 是（内部使用互斥锁保护）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_execution_health_check(
    agentos_execution_engine_t* engine,
    char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EXECUTION_H */