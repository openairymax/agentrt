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
 * @brief Agent 记录结构
 */
typedef struct heapstore_agent_record {
    char id[128];
    char name[256];
    char type[64];
    char version[32];
    char status[32];
    char config_path[512];
    uint64_t created_at;
    uint64_t updated_at;
} heapstore_agent_record_t;

/**
 * @brief 技能记录结构
 */
typedef struct heapstore_skill_record {
    char id[128];
    char name[256];
    char version[32];
    char library_path[512];
    char manifest_path[512];
    uint64_t installed_at;
} heapstore_skill_record_t;

/**
 * @brief 会话记录结构
 */
typedef struct heapstore_session_record {
    char id[128];
    char user_id[128];
    uint64_t created_at;
    uint64_t last_active_at;
    uint32_t ttl_seconds;
    char status[32];
} heapstore_session_record_t;

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
 */
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
 */
heapstore_error_t heapstore_registry_update_agent(const heapstore_agent_record_t* record);

/**
 * @brief 删除 Agent 记录
 *
 * @param id [in] Agent ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
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
 */
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
 */
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
 */
heapstore_error_t heapstore_registry_get_skill(const char* id, heapstore_skill_record_t* record);

/**
 * @brief 删除技能记录
 *
 * @param id [in] 技能 ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
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
 */
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
 */
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
 */
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
 */
heapstore_error_t heapstore_registry_update_session(const heapstore_session_record_t* record);

/**
 * @brief 删除会话记录
 *
 * @param id [in] 会话 ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_registry_delete_session(const char* id);

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
 */
heapstore_error_t heapstore_registry_iter_next(heapstore_registry_iter_t* iter, void* record);

/**
 * @brief 销毁迭代器
 *
 * @param iter [in] 迭代器
 *
 * @ownership 调用者负责传入有效的迭代器
 * @threadsafe 否
 * @reentrant 否
 */
void heapstore_registry_iter_destroy(heapstore_registry_iter_t* iter);

/**
 * @brief 执行数据库 VACUUM 操作
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_registry_vacuum(void);

/**
 * @brief 检查注册表系统是否健康
 *
 * @return bool 健康返回 true
 *
 * @threadsafe 是
 * @reentrant 是
 */
bool heapstore_registry_is_healthy(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_REGISTRY_H */
