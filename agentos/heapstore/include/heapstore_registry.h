/**
 * @file heapstore_registry.h
 * @brief AgentOS 数据分区注册表接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_heapstore_REGISTRY_H
#define AGENTOS_heapstore_REGISTRY_H

#include "heapstore.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 共享类型已在 heapstore_types.h 中定义，此处仅用于 API 声明 */

/**
 * @brief 注册表类型
 */
typedef enum {
    heapstore_REG_AGENTS,    /* Agent 注册表 */
    heapstore_REG_SKILLS,    /* 技能注册表 */
    heapstore_REG_SESSIONS,  /* 会话注册表 */
    heapstore_REG_MAX
} heapstore_registry_type_t;

/**
 * @brief 注册表迭代器
 */
typedef struct heapstore_registry_iter heapstore_registry_iter_t;

/**
 * @brief 初始化注册表系统
 *
 * @return heapstore_error_t 错误码
 *
 * @ownership 内部管理所有资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @see heapstore_registry_shutdown()
 * @since v1.0.0
 */
heapstore_error_t heapstore_registry_init(void);

/**
 * @brief 关闭注册表系统
 *
 * @ownership 内部释放所有资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see heapstore_registry_init()
 * @since v1.0.0
 */
void heapstore_registry_shutdown(void);

/**
 * @brief 添加 Agent 记录
 *
 * @param record [in] Agent 记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的生命周期
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_registry_add_agent(const heapstore_agent_record_t* record);

/**
 * @brief 获取 Agent 记录
 *
 * @param id [in] Agent ID
 * @param record [out] 输出记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的分配和释放
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_get_agent(const char* id, heapstore_agent_record_t* record);

/**
 * @brief 更新 Agent 记录
 *
 * @param record [in] Agent 记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的生命周期
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_update_agent(const heapstore_agent_record_t* record);

/**
 * @brief 删除 Agent 记录
 *
 * @param id [in] Agent ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_delete_agent(const char* id);

/**
 * @brief 查询 Agent 记录
 *
 * @param filter_type [in] 按类型过滤（NULL 表示不过滤）
 * @param filter_status [in] 按状态过滤（NULL 表示不过滤）
 * @param iter [out] 输出迭代器
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责调用 heapstore_registry_iter_destroy 释放迭代器
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_query_agents(const char* filter_type, const char* filter_status, heapstore_registry_iter_t** iter);

/**
 * @brief 添加技能记录
 *
 * @param record [in] 技能记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的生命周期
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_add_skill(const heapstore_skill_record_t* record);

/**
 * @brief 获取技能记录
 *
 * @param id [in] 技能 ID
 * @param record [out] 输出记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的分配和释放
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_get_skill(const char* id, heapstore_skill_record_t* record);

/**
 * @brief 删除技能记录
 *
 * @param id [in] 技能 ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_delete_skill(const char* id);

/**
 * @brief 查询技能记录
 *
 * @param iter [out] 输出迭代器
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责调用 heapstore_registry_iter_destroy 释放迭代器
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_query_skills(heapstore_registry_iter_t** iter);

/**
 * @brief 添加会话记录
 *
 * @param record [in] 会话记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的生命周期
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_add_session(const heapstore_session_record_t* record);

/**
 * @brief 获取会话记录
 *
 * @param id [in] 会话 ID
 * @param record [out] 输出记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的分配和释放
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_get_session(const char* id, heapstore_session_record_t* record);

/**
 * @brief 更新会话记录
 *
 * @param record [in] 会话记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 record 的生命周期
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_update_session(const heapstore_session_record_t* record);

/**
 * @brief 删除会话记录
 *
 * @param id [in] 会话 ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_delete_session(const char* id);

/**
 * @brief 查询会话记录
 *
 * @param filter_status [in] 按状态过滤（NULL 表示不过滤）
 * @param iter [out] 输出迭代器
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责调用 heapstore_registry_iter_destroy 释放迭代器
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_query_sessions(const char* filter_status, heapstore_registry_iter_t** iter);

/**
 * @brief 遍历下一条记录
 *
 * @param iter [in] 迭代器
 * @param record [out] 输出记录
 * @return heapstore_error_t 错误码，返回 heapstore_ERR_NOT_FOUND 表示遍历结束
 *
 * @ownership 调用者负责 record 的分配和释放
 * @threadsafe 否
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_registry_iter_next(heapstore_registry_iter_t* iter, void* record);

/**
 * @brief 销毁迭代器
 *
 * @param iter [in] 迭代器
 *
 * @ownership 调用者负责传入有效的迭代器
 * @threadsafe 否
 * @reentrant 否

 * @since v1.0.0*/
void heapstore_registry_iter_destroy(heapstore_registry_iter_t* iter);

/**
 * @brief 执行数据库 VACUUM 操作
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 * @since v1.0.0
 */
heapstore_error_t heapstore_registry_vacuum(void);

/**
 * @brief 批量插入 Agent 记录（事务优化版本）
 *
 * 此函数将多个 INSERT 操作包裹在单个事务中，显著提升批量写入性能。
 * 相比逐条调用 heapstore_registry_add_agent()，性能可提升 5-10 倍。
 *
 * @param records [in] Agent 记录数组
 * @param count [in] 记录数量
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 records 数组的生命周期
 * @threadsafe 是
 * @reentrant 否
 *
 * @note
 * - 所有记录要么全部插入成功，要么全部回滚（原子性）
 * - 如果某条记录失败，整个事务将回滚
 * - 适用于批量导入场景
 *
 * @see heapstore_registry_add_agent()
 * @since v1.0.0.7
 *
 * @example
 * @code
 * heapstore_agent_record_t records[100];
 * // ... 初始化 records ...
 * heapstore_error_t err = heapstore_registry_batch_insert_agents(records, 100);
 * if (err == heapstore_SUCCESS) {
 *     // 批量插入成功
 * }
 * @endcode
 */
heapstore_error_t heapstore_registry_batch_insert_agents(const heapstore_agent_record_t* records, size_t count);

/**
 * @brief 批量插入 Session 记录（事务优化版本）
 *
 * @param records [in] Session 记录数组
 * @param count [in] 记录数量
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 records 数组的生命周期
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 所有记录要么全部插入成功，要么全部回滚
 * @see heapstore_registry_add_session()
 * @since v1.0.0.7
 */
heapstore_error_t heapstore_registry_batch_insert_sessions(const heapstore_session_record_t* records, size_t count);

/**
 * @brief 批量插入 Skill 记录（事务优化版本）
 *
 * @param records [in] Skill 记录数组
 * @param count [in] 记录数量
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 records 数组的生命周期
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 所有记录要么全部插入成功，要么全部回滚
 * @see heapstore_registry_add_skill()
 * @since v1.0.0.7
 */
heapstore_error_t heapstore_registry_batch_insert_skills(const heapstore_skill_record_t* records, size_t count);

/**
 * @brief 检查注册表系统是否健康
 *
 * @return bool 健康返回 true
 *
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
bool heapstore_registry_is_healthy(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_REGISTRY_H */
