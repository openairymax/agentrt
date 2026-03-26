/**
 * @file execution_lite.h
 * @brief AgentOS Lite CoreLoopThree - 轻量化执行层接口
 * @version 1.0.0
 * @date 2026-03-26
 * 
 * 轻量化执行层提供高效的任务执行和状态管理功能：
 * 1. 计划执行：执行认知层生成的执行计划
 * 2. 技能调度：调用相应的技能执行具体操作
 * 3. 状态跟踪：跟踪任务执行状态和进度
 * 4. 结果收集：收集执行结果并格式化输出
 * 
 * 设计目标：
 * - 执行延迟：< 1ms（简单任务）
 * - 内存占用：< 200KB
 * - 并发执行：支持多个任务并行执行
 * - 错误恢复：基本的错误处理和重试机制
 */

#ifndef AGENTOS_EXECUTION_LITE_H
#define AGENTOS_EXECUTION_LITE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码定义 ==================== */

typedef enum {
    CLT_EXECUTION_SUCCESS = 0,           /**< 操作成功 */
    CLT_EXECUTION_ERROR = -1,            /**< 通用错误 */
    CLT_EXECUTION_INVALID_PLAN = -2,     /**< 无效的执行计划 */
    CLT_EXECUTION_SKILL_NOT_FOUND = -3,  /**< 技能未找到 */
    CLT_EXECUTION_SKILL_FAILED = -4,     /**< 技能执行失败 */
    CLT_EXECUTION_TIMEOUT = -5,          /**< 执行超时 */
    CLT_EXECUTION_OUT_OF_MEMORY = -6,    /**< 内存不足 */
    CLT_EXECUTION_INVALID_STATE = -7,    /**< 无效的执行状态 */
} clt_execution_error_t;

/* ==================== 执行状态定义 ==================== */

typedef enum {
    CLT_EXECUTION_STATE_PENDING = 0,     /**< 待执行 */
    CLT_EXECUTION_STATE_RUNNING = 1,     /**< 执行中 */
    CLT_EXECUTION_STATE_COMPLETED = 2,   /**< 已完成 */
    CLT_EXECUTION_STATE_FAILED = 3,      /**< 已失败 */
    CLT_EXECUTION_STATE_CANCELLED = 4,   /**< 已取消 */
} clt_execution_state_t;

/* ==================== 技能接口定义 ==================== */

/**
 * @brief 技能执行函数类型
 * @param params 技能参数（JSON格式字符串）
 * @param params_len 参数长度
 * @param result 输出参数：执行结果（JSON格式字符串）
 * @param result_len 输出参数：结果长度
 * @return 错误码
 */
typedef clt_execution_error_t (*clt_skill_execute_func_t)(
    const char* params,
    size_t params_len,
    char** result,
    size_t* result_len
);

/**
 * @brief 技能描述信息
 */
typedef struct {
    const char* name;                    /**< 技能名称 */
    const char* description;             /**< 技能描述 */
    clt_skill_execute_func_t execute_func; /**< 执行函数 */
    void* user_data;                     /**< 用户数据 */
} clt_skill_info_t;

/* ==================== 执行上下文结构 ==================== */

/**
 * @brief 执行上下文（不透明类型）
 */
typedef struct clt_execution_context_s clt_execution_context_t;

/* ==================== 公共接口 ==================== */

/**
 * @brief 初始化执行层
 * @return 成功返回true，失败返回false
 */
bool clt_execution_init(void);

/**
 * @brief 清理执行层资源
 */
void clt_execution_cleanup(void);

/**
 * @brief 注册技能
 * @param skill 技能信息
 * @return 成功返回true，失败返回false
 */
bool clt_execution_register_skill(const clt_skill_info_t* skill);

/**
 * @brief 注销技能
 * @param skill_name 技能名称
 * @return 成功返回true，失败返回false
 */
bool clt_execution_unregister_skill(const char* skill_name);

/**
 * @brief 执行认知结果
 * @param cognition_result 认知结果（JSON格式字符串）
 * @param cognition_result_len 认知结果长度
 * @return 执行结果字符串（JSON格式），需要调用clt_execution_free_result释放
 */
char* clt_execution_execute(const char* cognition_result, size_t cognition_result_len);

/**
 * @brief 创建执行上下文
 * @param context_name 上下文名称
 * @return 执行上下文句柄，失败返回NULL
 */
clt_execution_context_t* clt_execution_create_context(const char* context_name);

/**
 * @brief 销毁执行上下文
 * @param context 执行上下文句柄
 */
void clt_execution_destroy_context(clt_execution_context_t* context);

/**
 * @brief 在指定上下文中执行任务
 * @param context 执行上下文句柄
 * @param cognition_result 认知结果（JSON格式字符串）
 * @param cognition_result_len 认知结果长度
 * @return 执行结果字符串（JSON格式），需要调用clt_execution_free_result释放
 */
char* clt_execution_execute_in_context(
    clt_execution_context_t* context,
    const char* cognition_result,
    size_t cognition_result_len
);

/**
 * @brief 获取执行状态
 * @param context 执行上下文句柄
 * @return 执行状态
 */
clt_execution_state_t clt_execution_get_state(const clt_execution_context_t* context);

/**
 * @brief 取消执行
 * @param context 执行上下文句柄
 * @return 成功返回true，失败返回false
 */
bool clt_execution_cancel(clt_execution_context_t* context);

/**
 * @brief 获取执行进度
 * @param context 执行上下文句柄
 * @return 进度百分比（0.0-1.0）
 */
float clt_execution_get_progress(const clt_execution_context_t* context);

/**
 * @brief 获取执行统计信息
 * @param stats 输出参数：统计信息（JSON格式字符串）
 * @param stats_len 输出参数：统计信息长度
 * @return 错误码
 */
clt_execution_error_t clt_execution_get_stats(char** stats, size_t* stats_len);

/**
 * @brief 释放执行结果字符串
 * @param result 执行结果字符串
 */
void clt_execution_free_result(char* result);

/**
 * @brief 获取最后错误信息
 * @return 错误信息字符串
 */
const char* clt_execution_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EXECUTION_LITE_H */