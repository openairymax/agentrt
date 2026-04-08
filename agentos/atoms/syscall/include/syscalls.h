/**
 * @file syscalls.h
 * @brief 系统调用接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SYSCALL_H
#define AGENTOS_SYSCALL_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define SYSCALL_API_VERSION_MAJOR 1
#define SYSCALL_API_VERSION_MINOR 0
#define SYSCALL_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 系统初始化 ==================== */

/**
 * @brief 初始化系统调用层
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_syscalls_init(void);

/**
 * @brief 清理系统调用层资源
 */
AGENTOS_API void agentos_syscalls_cleanup(void);

/* ==================== 任务管理 ==================== */

/**
 * @brief 提交任务到内核
 * @param input 输入数据（JSON 格式）
 * @param input_len 输入长度
 * @param timeout_ms 超时时间（毫秒）
 * @param out_output 输出结果
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_submit(const char* input, size_t input_len,
                                                    uint32_t timeout_ms, char** out_output);

/**
 * @brief 查询任务状态
 * @param task_id 任务 ID
 * @param out_status 输出状态
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_query(const char* task_id, int* out_status);

/**
 * @brief 等待任务完成
 * @param task_id 任务 ID
 * @param timeout_ms 超时时间
 * @param out_result 输出结果
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_wait(const char* task_id, uint32_t timeout_ms, char** out_result);

/**
 * @brief 取消任务
 * @param task_id 任务 ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_cancel(const char* task_id);

/* ==================== 内存管理 ==================== */

/**
 * @brief 写入记忆数据
 * @param data 数据指针
 * @param len 数据长度
 * @param metadata 元数据（JSON 格式）
 * @param out_record_id 输出记录 ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(const void* data, size_t len,
                                                     const char* metadata, char** out_record_id);

/**
 * @brief 搜索记忆
 * @param query 查询字符串
 * @param limit 返回数量限制
 * @param out_record_ids 输出记录 ID 数组
 * @param out_scores 输出相似度分数数组
 * @param out_count 输出数量
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_search(const char* query, uint32_t limit,
                                                      char*** out_record_ids, float** out_scores,
                                                      size_t* out_count);

/**
 * @brief 获取记忆记录
 * @param record_id 记录 ID
 * @param out_data 输出数据
 * @param out_len 输出数据长度
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_get(const char* record_id,
                                                   void** out_data, size_t* out_len);

/**
 * @brief 删除记忆记录
 * @param record_id 记录 ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_delete(const char* record_id);

/* ==================== 会话管理 ==================== */

/**
 * @brief 会话持久化状态
 */
typedef enum {
    SESSION_PERSIST_UNKNOWN = 0,    /**< 未知状态 */
    SESSION_PERSIST_PENDING,        /**< 等待持久化 */
    SESSION_PERSIST_SUCCESS,        /**< 持久化成功 */
    SESSION_PERSIST_FAILED,         /**< 持久化失败 */
    SESSION_PERSIST_DISABLED        /**< 持久化禁用 */
} session_persist_status_t;

/**
 * @brief 创建会话
 * @param metadata 会话元数据（JSON 格式，可为 NULL）
 * @param out_session_id 输出会话 ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_create(const char* metadata, char** out_session_id);

/**
 * @brief 获取会话信息
 * @param session_id 会话 ID
 * @param out_info 输出会话信息（JSON 格式）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_get(const char* session_id, char** out_info);

/**
 * @brief 关闭会话
 * @param session_id 会话 ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_close(const char* session_id);

/**
 * @brief 列出所有会话
 * @param out_sessions 输出会话 ID 数组（需调用者释放）
 * @param out_count 输出会话数量
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_list(char*** out_sessions, size_t* out_count);

/**
 * @brief 获取会话持久化状态
 * @param session_id 会话 ID
 * @param out_status 输出持久化状态
 * @param out_error 输出持久化错误码（可为 NULL）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_get_persist_status(
    const char* session_id,
    session_persist_status_t* out_status,
    agentos_error_t* out_error);

/* ==================== 可观测性 ==================== */

/**
 * @brief 获取系统指标
 * @param out_metrics 输出指标（JSON 格式）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_telemetry_metrics(char** out_metrics);

/**
 * @brief 获取链路追踪
 * @param trace_id 追踪 ID
 * @param out_spans 输出跨度列表（JSON 数组）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_telemetry_traces(const char* trace_id, char** out_spans);

/* ==================== Agent 管理 ==================== */

/**
 * @brief 创建 Agent 实例
 * @param agent_spec Agent 规格（JSON 格式）
 * @param out_agent_id 输出 Agent ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_agent_spawn(const char* agent_spec, char** out_agent_id);

/**
 * @brief 销毁 Agent 实例
 * @param agent_id Agent ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_agent_terminate(const char* agent_id);

/**
 * @brief 调用 Agent 执行任务
 * @param agent_id Agent ID
 * @param input 输入数据
 * @param input_len 输入长度
 * @param out_output 输出结果
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_agent_invoke(const char* agent_id, const char* input,
                                                     size_t input_len, char** out_output);

/**
 * @brief 列出所有 Agent
 * @param out_agent_ids 输出 Agent ID 数组
 * @param out_count 输出数量
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_agent_list(char*** out_agent_ids, size_t* out_count);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 释放系统调用分配的内存
 * @param ptr 内存指针
 */
AGENTOS_API void agentos_sys_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYSCALL_H */
