/**
 * @file session.h
 * @brief 会话管理器接口
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DYNAMIC_SESSION_H
#define DYNAMIC_SESSION_H

#include <stddef.h>
#include <stdint.h>
#include "agentos.h"

/**
 * @brief 会话管理器不透明句柄
 */
typedef struct session_manager session_manager_t;

/**
 * @brief 创建会话管理器
 * 
 * @param max_sessions 最大会话数
 * @param timeout_sec 超时秒数
 * @return 句柄，失败返回 NULL
 * 
 * @note 创建后会启动后台清理线程
 * @ownership 调用者需通过 session_manager_destroy() 释放
 */
session_manager_t* session_manager_create(
    size_t max_sessions,
    uint32_t timeout_sec);

/**
 * @brief 销毁会话管理器
 * @param mgr 管理器句柄
 */
void session_manager_destroy(session_manager_t* mgr);

/**
 * @brief 创建新会话
 * 
 * @param mgr 管理器
 * @param metadata 元数据（JSON 字符串，可为 NULL）
 * @param out_session_id 输出会话 ID（需调用者 free）
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_EBUSY 已达最大会话数
 * @return AGENTOS_ENOMEM 内存不足
 */
agentos_error_t session_manager_create_session(
    session_manager_t* mgr,
    const char* metadata,
    char** out_session_id);

/**
 * @brief 获取会话信息（JSON 字符串）
 * 
 * @param mgr 管理器
 * @param session_id 会话 ID
 * @param out_info 输出 JSON 字符串（需调用者 free）
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOENT 会话不存在
 */
agentos_error_t session_manager_get_session(
    session_manager_t* mgr,
    const char* session_id,
    char** out_info);

/**
 * @brief 更新会话活动时间
 * 
 * @param mgr 管理器
 * @param session_id 会话 ID
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_ENOENT 会话不存在
 */
agentos_error_t session_manager_touch_session(
    session_manager_t* mgr,
    const char* session_id);

/**
 * @brief 关闭会话
 * 
 * @param mgr 管理器
 * @param session_id 会话 ID
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_ENOENT 会话不存在
 */
agentos_error_t session_manager_close_session(
    session_manager_t* mgr,
    const char* session_id);

/**
 * @brief 列出所有活跃会话 ID
 * 
 * @param mgr 管理器
 * @param out_sessions 输出字符串数组（需调用者 free 数组及每个元素）
 * @param out_count 输出数量
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t session_manager_list_sessions(
    session_manager_t* mgr,
    char*** out_sessions,
    size_t* out_count);

/**
 * @brief 获取当前活跃会话数
 * 
 * @param mgr 管理器
 * @return 活跃会话数
 */
size_t session_manager_count(session_manager_t* mgr);

/**
 * @brief 获取会话管理器统计信息
 * 
 * @param mgr 管理器
 * @param out_json 输出JSON字符串（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t session_manager_get_stats(session_manager_t* mgr, char** out_json);

#endif /* DYNAMIC_SESSION_H */
