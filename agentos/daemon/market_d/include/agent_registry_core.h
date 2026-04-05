/**
 * @file agent_registry_core.h
 * @brief Agent注册表核心功能接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 2.0.0
 * @date 2026-04-04
 *
 * 重构说明：
 * - 从agent_registry.c拆分为独立模块
 * - 降低圈复杂度（CC: 130 → <30）
 * - 遵循ARCHITECTURAL_PRINCIPLES.md E-3资源确定性原则
 */

#ifndef AGENTOS_AGENT_REGISTRY_CORE_H
#define AGENTOS_AGENT_REGISTRY_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 类型定义 ==================== */

#define MAX_AGENT_ID_LEN 128
#define MAX_AGENT_NAME_LEN 256
#define MAX_DESCRIPTION_LEN 4096
#define MAX_AUTHOR_LEN 256
#define MAX_TAG_LEN 64
#define MAX_TAGS_PER_AGENT 16
#define MAX_VERSIONS_PER_AGENT 32
#define MAX_DEPENDENCIES_PER_AGENT 16
#define MAX_AGENTS 1024

typedef struct agent_entry agent_entry_t;
typedef struct agent_registry agent_registry_t;
typedef struct agent_version agent_version_t;
typedef struct agent_dependency agent_dependency_t;

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 创建注册表实例
 * @return 注册表实例，失败返回NULL
 */
agent_registry_t* agent_registry_create(void);

/**
 * @brief 销毁注册表实例
 * @param registry 注册表实例
 */
void agent_registry_destroy(agent_registry_t* registry);

/**
 * @brief 初始化注册表
 * @param registry 注册表实例
 * @param db_path 数据库路径（可选）
 * @return 0成功，非0失败
 */
int agent_registry_init(agent_registry_t* registry, const char* db_path);

/**
 * @brief 关闭注册表
 * @param registry 注册表实例
 */
void agent_registry_shutdown(agent_registry_t* registry);

/* ==================== 基本操作 ==================== */

/**
 * @brief 注册Agent
 * @param registry 注册表实例
 * @param reg 注册信息
 * @return 0成功，非0失败
 */
int agent_registry_add(agent_registry_t* registry, const agent_entry_t* reg);

/**
 * @brief 注销Agent
 * @param registry 注册表实例
 * @param agent_id Agent ID
 * @return 0成功，非0失败
 */
int agent_registry_remove(agent_registry_t* registry, const char* agent_id);

/**
 * @brief 获取Agent信息
 * @param registry 注册表实例
 * @param agent_id Agent ID
 * @return Agent信息，未找到返回NULL
 */
const agent_entry_t* agent_registry_get(agent_registry_t* registry, const char* agent_id);

/**
 * @brief 列出所有Agent
 * @param registry 注册表实例
 * @param out_entries [out] 输出数组
 * @param max_entries 最大条目数
 * @return 实际返回的条目数
 */
size_t agent_registry_list(agent_registry_t* registry,
                          const agent_entry_t** out_entries,
                          size_t max_entries);

/**
 * @brief 获取Agent总数
 * @param registry 注册表实例
 * @return Agent总数
 */
size_t agent_registry_count(agent_registry_t* registry);

/* ==================== 搜索功能 ==================== */

/**
 * @brief 按标签搜索Agent
 * @param registry 注册表实例
 * @param tag 标签
 * @param out_entries [out] 输出数组
 * @param max_entries 最大条目数
 * @return 匹配的条目数
 */
size_t agent_registry_search_by_tag(agent_registry_t* registry,
                                     const char* tag,
                                     const agent_entry_t** out_entries,
                                     size_t max_entries);

/**
 * @brief 模糊搜索Agent
 * @param registry 注册表实例
 * @param query 查询字符串
 * @param out_entries [out] 输出数组
 * @param max_entries 最大条目数
 * @return 匹配的条目数
 */
size_t agent_registry_search(agent_registry_t* registry,
                            const char* query,
                            const agent_entry_t** out_entries,
                            size_t max_entries);

/* ==================== 版本管理 ==================== */

/**
 * @brief 添加Agent版本
 * @param registry 注册表实例
 * @param agent_id Agent ID
 * @param version 版本信息
 * @return 0成功，非0失败
 */
int agent_registry_add_version(agent_registry_t* registry,
                               const char* agent_id,
                               const agent_version_t* version);

/**
 * @brief 获取Agent最新版本
 * @param registry 注册表实例
 * @param agent_id Agent ID
 * @return 版本字符串，未找到返回NULL
 */
const char* agent_registry_get_latest_version(agent_registry_t* registry,
                                              const char* agent_id);

/**
 * @brief 检查版本是否兼容
 * @param registry 注册表实例
 * @param agent_id Agent ID
 * @param version_constraint 版本约束
 * @return 1兼容，0不兼容
 */
int agent_registry_check_version(agent_registry_t* registry,
                                const char* agent_id,
                                const char* version_constraint);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AGENT_REGISTRY_CORE_H */
