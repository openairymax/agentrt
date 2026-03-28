/**
 * @file agentos_coreloopthreelite.h
 * @brief AgentOS Lite CoreLoopThree - 轻量化三层运行时核心循环
 * @version 1.0.0
 * @date 2026-03-26
 * 
 * 轻量化版本的CoreLoopThree模块，提供简化的三层运行时架构：
 * 1. 认知层（Cognition Lite）：简化的意图理解和任务规划
 * 2. 行动层（Execution Lite）：高效的任务执行和状态管理
 * 3. 记忆层（Memory Lite）：基本的内存接口和上下文管理
 * 
 * 设计目标：
 * - 运行效率提升30%以上
 * - 内存占用降低50%以上
 * - 保持核心功能的完整性和兼容性
 */

#ifndef AGENTOS_CORELOOPTHREELITE_H
#define AGENTOS_CORELOOPTHREELITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup coreloopthreelite AgentOS Lite CoreLoopThree
 * @brief 轻量化三层运行时核心循环
 * @{
 */

/**
 * @brief 错误码定义
 */
typedef enum {
    AGENTOS_CLT_SUCCESS = 0,               /**< 操作成功 */
    AGENTOS_CLT_ERROR = -1,                /**< 通用错误 */
    AGENTOS_CLT_INVALID_PARAM = -2,        /**< 无效参数 */
    AGENTOS_CLT_OUT_OF_MEMORY = -3,        /**< 内存不足 */
    AGENTOS_CLT_TASK_QUEUE_FULL = -4,      /**< 任务队列已满 */
    AGENTOS_CLT_TASK_NOT_FOUND = -5,       /**< 任务未找到 */
    AGENTOS_CLT_ENGINE_NOT_INITIALIZED = -6, /**< 引擎未初始化 */
    AGENTOS_CLT_OPERATION_TIMEOUT = -7,    /**< 操作超时 */
} agentos_clt_error_t;

/**
 * @brief 任务状态
 */
typedef enum {
    AGENTOS_CLT_TASK_PENDING = 0,          /**< 任务待处理 */
    AGENTOS_CLT_TASK_PROCESSING = 1,       /**< 任务处理中 */
    AGENTOS_CLT_TASK_COMPLETED = 2,        /**< 任务已完成 */
    AGENTOS_CLT_TASK_FAILED = 3,           /**< 任务失败 */
    AGENTOS_CLT_TASK_CANCELLED = 4,        /**< 任务已取消 */
} agentos_clt_task_status_t;

/**
 * @brief 任务优先级
 */
typedef enum {
    AGENTOS_CLT_PRIORITY_LOW = 0,          /**< 低优先级 */
    AGENTOS_CLT_PRIORITY_NORMAL = 1,       /**< 普通优先级 */
    AGENTOS_CLT_PRIORITY_HIGH = 2,         /**< 高优先级 */
    AGENTOS_CLT_PRIORITY_CRITICAL = 3,     /**< 关键优先级 */
} agentos_clt_task_priority_t;

/**
 * @brief 任务句柄（不透明类型）
 */
typedef struct agentos_clt_task_handle_s agentos_clt_task_handle_t;

/**
 * @brief 引擎句柄（不透明类型）
 */
typedef struct agentos_clt_engine_handle_s agentos_clt_engine_handle_t;

/**
 * @brief 任务回调函数类型
 * @param task_handle 任务句柄
 * @param user_data 用户数据
 * @param result 任务结果（JSON格式字符串）
 * @param result_len 结果长度
 */
typedef void (*agentos_clt_task_callback_t)(
    agentos_clt_task_handle_t* task_handle,
    void* user_data,
    const char* result,
    size_t result_len
);

/**
 * @brief 初始化CoreLoopThree Lite引擎
 * @param max_concurrent_tasks 最大并发任务数（建议值：4-16）
 * @param worker_threads 工作线程数（0表示自动选择，建议值：2-8）
 * @return 引擎句柄，失败返回NULL
 */
agentos_clt_engine_handle_t* agentos_clt_engine_init(
    size_t max_concurrent_tasks,
    size_t worker_threads
);

/**
 * @brief 销毁CoreLoopThree Lite引擎
 * @param engine 引擎句柄
 * @return 错误码
 */
agentos_clt_error_t agentos_clt_engine_destroy(
    agentos_clt_engine_handle_t* engine
);

/**
 * @brief 提交任务到引擎
 * @param engine 引擎句柄
 * @param task_data 任务数据（JSON格式字符串）
 * @param task_data_len 任务数据长度
 * @param priority 任务优先级
 * @param callback 任务完成回调函数（可为NULL）
 * @param user_data 用户数据（传递给回调函数）
 * @return 任务句柄，失败返回NULL
 */
agentos_clt_task_handle_t* agentos_clt_task_submit(
    agentos_clt_engine_handle_t* engine,
    const char* task_data,
    size_t task_data_len,
    agentos_clt_task_priority_t priority,
    agentos_clt_task_callback_t callback,
    void* user_data
);

/**
 * @brief 查询任务状态
 * @param task_handle 任务句柄
 * @return 任务状态
 */
agentos_clt_task_status_t agentos_clt_task_get_status(
    const agentos_clt_task_handle_t* task_handle
);

/**
 * @brief 获取任务结果
 * @param task_handle 任务句柄
 * @param result 输出参数：任务结果（JSON格式字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 * 
 * @note 结果缓冲区由调用方释放（使用agentos_clt_free_result）
 */
agentos_clt_error_t agentos_clt_task_get_result(
    const agentos_clt_task_handle_t* task_handle,
    char** result,
    size_t* result_len
);

/**
 * @brief 释放任务结果缓冲区
 * @param result 任务结果缓冲区
 */
void agentos_clt_free_result(char* result);

/**
 * @brief 取消任务
 * @param task_handle 任务句柄
 * @return 错误码
 */
agentos_clt_error_t agentos_clt_task_cancel(
    agentos_clt_task_handle_t* task_handle
);

/**
 * @brief 等待任务完成
 * @param task_handle 任务句柄
 * @param timeout_ms 超时时间（毫秒，0表示无限等待）
 * @return 错误码
 */
agentos_clt_error_t agentos_clt_task_wait(
    agentos_clt_task_handle_t* task_handle,
    uint32_t timeout_ms
);

/**
 * @brief 设置引擎参数
 * @param engine 引擎句柄
 * @param param_name 参数名称
 * @param param_value 参数值（JSON格式字符串）
 * @return 错误码
 */
agentos_clt_error_t agentos_clt_engine_set_param(
    agentos_clt_engine_handle_t* engine,
    const char* param_name,
    const char* param_value
);

/**
 * @brief 获取引擎统计信息
 * @param engine 引擎句柄
 * @param stats 输出参数：统计信息（JSON格式字符串）
 * @param stats_len 输出参数：统计信息长度
 * @return 错误码
 * 
 * @note 统计信息缓冲区由调用方释放（使用agentos_clt_free_result）
 */
agentos_clt_error_t agentos_clt_engine_get_stats(
    const agentos_clt_engine_handle_t* engine,
    char** stats,
    size_t* stats_len
);

/**
 * @brief 获取最后错误信息
 * @return 错误信息字符串
 */
const char* agentos_clt_get_last_error(void);

/**
 * @brief 获取版本信息
 * @return 版本字符串
 */
const char* agentos_clt_get_version(void);

/** @} */ // end of coreloopthreelite group

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREELITE_H */