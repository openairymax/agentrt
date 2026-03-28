/**
 * @file permission.h
 * @brief 权限裁决引擎公共接口
 * @author Spharx
 * @date 2024
 * 
 * 设计原则：
 * - 最小权限：默认拒绝，显式允许
 * - 高性能：缓存 + 规则优先级排序
 * - 可扩展：支持动态规则加载
 */

#ifndef DOMAIN_PERMISSION_H
#define DOMAIN_PERMISSION_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 权限引擎句柄 */
typedef struct permission_engine permission_engine_t;

/**
 * @brief 创建权限引擎
 * @param rules_path 规则文件路径（YAML格式），NULL表示空规则集
 * @return 引擎句柄，失败返回 NULL
 */
permission_engine_t* permission_engine_create(const char* rules_path);

/**
 * @brief 销毁权限引擎
 * @param engine 引擎句柄
 */
void permission_engine_destroy(permission_engine_t* engine);

/**
 * @brief 增加引用计数
 * @param engine 引擎句柄
 * @return 引擎句柄
 */
permission_engine_t* permission_engine_ref(permission_engine_t* engine);

/**
 * @brief 减少引用计数
 * @param engine 引擎句柄
 */
void permission_engine_unref(permission_engine_t* engine);

/**
 * @brief 检查权限
 * @param engine 引擎句柄
 * @param agent_id Agent ID
 * @param action 操作类型（如 "read", "write", "execute"）
 * @param resource 资源路径
 * @param context 上下文信息（可选）
 * @return 1 允许，0 拒绝
 */
int permission_engine_check(permission_engine_t* engine,
                            const char* agent_id,
                            const char* action,
                            const char* resource,
                            const char* context);

/**
 * @brief 重新加载规则文件
 * @param engine 引擎句柄
 * @return 0 成功，其他失败
 */
int permission_engine_reload(permission_engine_t* engine);

/**
 * @brief 清空缓存
 * @param engine 引擎句柄
 */
void permission_engine_clear_cache(permission_engine_t* engine);

/**
 * @brief 添加规则
 * @param engine 引擎句柄
 * @param agent_id Agent ID（NULL 或 "*" 表示通配）
 * @param action 操作（NULL 或 "*" 表示通配）
 * @param resource 资源模式（支持 glob 模式，如 "/data/*"）
 * @param allow 1 允许，0 拒绝
 * @param priority 优先级（数值越大优先级越高）
 * @return 0 成功，其他失败
 */
int permission_engine_add_rule(permission_engine_t* engine,
                               const char* agent_id,
                               const char* action,
                               const char* resource,
                               int allow,
                               int priority);

/**
 * @brief 获取规则数量
 * @param engine 引擎句柄
 * @return 规则数量
 */
size_t permission_engine_rule_count(permission_engine_t* engine);

/**
 * @brief 获取缓存统计信息
 * @param engine 引擎句柄
 * @param hit_count 命中次数（输出）
 * @param miss_count 未命中次数（输出）
 */
void permission_engine_cache_stats(permission_engine_t* engine,
                                   uint64_t* hit_count,
                                   uint64_t* miss_count);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_PERMISSION_H */
