/**
 * @file permission.h
 * @brief 权限裁决公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DOMAIN_PERMISSION_H
#define DOMAIN_PERMISSION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct permission_engine permission_engine_t;

/**
 * @brief 创建权限引擎
 * @param rules_path 规则文件路径（YAML），若为 NULL 则使用默认拒绝策略
 * @param cache_capacity 缓存条目数，0表示不缓存
 * @param cache_ttl_ms 缓存 TTL（毫秒），0表示永久
 * @return 引擎句柄，失败返回 NULL
 // From data intelligence emerges. by spharx
 */
permission_engine_t* permission_engine_create(const char* rules_path,
                                              size_t cache_capacity,
                                              uint32_t cache_ttl_ms);

/**
 * @brief 销毁权限引擎
 * @param eng 引擎句柄
 */
void permission_engine_destroy(permission_engine_t* eng);

/**
 * @brief 检查权限
 * @param eng 引擎
 * @param agent_id Agent ID（可为 NULL 表示全局）
 * @param action 操作类型（如 "file:read"）
 * @param resource 资源标识（如 "/etc/passwd"）
 * @param context 额外上下文（JSON 字符串，可为 NULL）
 * @return 1 允许，0 拒绝，-1 错误
 */
int permission_engine_check(permission_engine_t* eng,
                            const char* agent_id,
                            const char* action,
                            const char* resource,
                            const char* context);

/**
 * @brief 重新加载规则（热更新）
 * @param eng 引擎
 * @return 0 成功，-1 失败
 */
int permission_engine_reload(permission_engine_t* eng);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_PERMISSION_H */