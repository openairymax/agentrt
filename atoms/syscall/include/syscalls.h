/**
 * @file syscalls.h
 * @brief 内核系统调用接口
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
// From data intelligence emerges. by spharx

/* ==================== 系统初始化 ==================== */

/**
 * @brief 初始化系统调用模块，设置底层引擎句柄
 * @param cognition 认知引擎句柄
 * @param execution 执行引擎句柄
 * @param memory 记忆引擎句柄
 */
AGENTOS_API void agentos_sys_init(void* cognition, void* execution, void* memory);

/* ==================== 任务管理 ==================== */

/**
 * @brief 提交一个自然语言任务，同步等待执行完成
 * @param input 输入文本
 * @param input_len 文本长度
 * @param timeout_ms 超时（毫秒，0表示无限）
 * @param out_result 输出结果（JSON字符串，需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_submit(const char* input, size_t input_len,
                                        uint32_t timeout_ms, char** out_result);

/**
 * @brief 查询任务状态（通过任务ID）
 * @param task_id 任务ID
 * @param out_status 输出状态（0 pending, 1 running, 2 succeeded, 3 failed, 4 cancelled）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_query(const char* task_id, int* out_status);

/**
 * @brief 等待指定任务完成并获取结果
 * @param task_id 任务ID
 * @param timeout_ms 超时（毫秒，0无限）
 * @param out_result 输出结果（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_wait(const char* task_id, uint32_t timeout_ms, char** out_result);

/**
 * @brief 取消任务
 * @param task_id 任务ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_cancel(const char* task_id);

/* ==================== 记忆管理 ==================== */

/**
 * @brief 写入原始记忆
 * @param data 数据
 * @param len 数据长度
 * @param metadata JSON元数据（可为NULL）
 * @param out_record_id 输出记录ID（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(const void* data, size_t len,
                                         const char* metadata, char** out_record_id);

/**
 * @brief 查询记忆（语义搜索）
 * @param query 查询文本
 * @param limit 最大结果数
 * @param out_record_ids 输出记录ID数组（需调用者释放）
 * @param out_scores 输出得分数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_search(const char* query, uint32_t limit,
                                          char*** out_record_ids, float** out_scores,
                                          size_t* out_count);

/**
 * @brief 获取记忆原始数据
 * @param record_id 记录ID
 * @param out_data 输出数据（需调用者释放）
 * @param out_len 输出长度
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_get(const char* record_id, void** out_data, size_t* out_len);

/**
 * @brief 删除记忆（永久移除）
 * @param record_id 记录ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_delete(const char* record_id);

/* ==================== 会话管理 ==================== */

/**
 * @brief 创建新会话
 * @param metadata JSON元数据（可为NULL）
 * @param out_session_id 输出会话ID（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_create(const char* metadata, char** out_session_id);

/**
 * @brief 获取会话信息
 * @param session_id 会话ID
 * @param out_info 输出JSON信息（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_get(const char* session_id, char** out_info);

/**
 * @brief 关闭会话
 * @param session_id 会话ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_close(const char* session_id);

/**
 * @brief 列出所有活跃会话
 * @param out_sessions 输出会话ID数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_list(char*** out_sessions, size_t* out_count);

/* ==================== 可观测性 ==================== */

/**
 * @brief 获取系统指标
 * @param out_metrics 输出JSON指标（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_telemetry_metrics(char** out_metrics);

/**
 * @brief 获取追踪数据
 * @param out_traces 输出JSON追踪（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_telemetry_traces(char** out_traces);

/* ==================== 内部系统调用入口 ==================== */

/**
 * @brief 系统调用入口（供底层调用）
 * @param syscall_num 系统调用号
 * @param args 参数数组
 * @param argc 参数数量
 * @return 结果指针（通常为整数错误码或数据指针）
 */
void* agentos_syscall_invoke(int syscall_num, void** args, int argc);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYSCALL_H */