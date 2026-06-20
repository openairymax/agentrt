/**
 * @file plugin_permission.h
 * @brief P2.2.4: 插件权限校验 — manifest 权限 ↔ Cupolas 守卫类型映射
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 将插件 manifest 中声明的权限映射到 Cupolas 安全穹顶的守卫类型。
 * 加载插件时自动校验权限，不符合安全策略的插件拒绝加载。
 *
 * 权限映射：
 *   file_read        → SAFETY_GUARD_FILE_READ
 *   file_write       → SAFETY_GUARD_FILE_WRITE
 *   network_outbound → SAFETY_GUARD_NETWORK
 *   tool_execute     → SAFETY_GUARD_TOOL_EXEC
 *   memory_access    → SAFETY_GUARD_MEMORY
 *   hook_register    → SAFETY_GUARD_HOOK
 *   system_call      → SAFETY_GUARD_SYSTEM
 *   process_spawn    → SAFETY_GUARD_PROCESS
 */

#ifndef AGENTOS_PLUGIN_PERMISSION_H
#define AGENTOS_PLUGIN_PERMISSION_H

#include "plugin_service.h"
#include "safety_guard.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 权限校验结果 ==================== */

typedef enum {
    PLUGIN_PERM_ALLOWED    = 0,  /**< 权限通过 */
    PLUGIN_PERM_DENIED     = 1,  /**< 权限拒绝 */
    PLUGIN_PERM_UNKNOWN    = 2,  /**< 未识别的权限声明 */
    PLUGIN_PERM_ERROR      = 3,  /**< 校验失败 */
} plugin_permission_result_t;

/* ==================== 权限校验配置 ==================== */

typedef struct {
    bool enable_strict_mode;          /**< 严格模式：未声明的权限视为拒绝 */
    bool enable_audit_log;            /**< 启用审计日志 */
    char safety_policy_path[512];     /**< 安全策略文件路径 */
    const char *agent_id;             /**< 代理 ID */
} plugin_permission_config_t;

/* ==================== 权限校验 API ==================== */

/**
 * @brief 初始化权限校验模块
 *
 * @param config 配置（NULL 使用默认）
 * @return 0 成功，非0 失败
 */
int plugin_permission_init(const plugin_permission_config_t *config);

/**
 * @brief 销毁权限校验模块
 */
void plugin_permission_destroy(void);

/**
 * @brief 校验插件权限声明
 *
 * 将 manifest 中的权限声明映射到 Cupolas 守卫类型，
 * 逐项检查每个权限是否被安全策略允许。
 *
 * @param permissions       权限声明数组
 * @param permission_count  权限数量
 * @param plugin_name       插件名称（用于审计）
 * @param out_denied        输出被拒绝的权限（逗号分隔）
 * @param out_denied_size   缓冲区大小
 * @return PLUGIN_PERM_ALLOWED 全部通过，否则返回第一个拒绝原因
 */
plugin_permission_result_t plugin_permission_check(
    const char (*permissions)[64],
    uint32_t permission_count,
    const char *plugin_name,
    char *out_denied,
    size_t out_denied_size);

/**
 * @brief 将权限字符串映射到 Cupolas 守卫类型
 *
 * @param permission 权限字符串
 * @param out_guard  输出守卫类型
 * @return 0 成功，-1 未知权限
 */
int plugin_permission_map_to_guard(const char *permission,
                                   safety_guard_type_t *out_guard);

/**
 * @brief 获取权限的人类可读描述
 *
 * @param permission 权限字符串
 * @return 描述字符串
 */
const char *plugin_permission_description(const char *permission);

/**
 * @brief 获取支持的权限列表
 *
 * @param out_permissions 输出权限数组
 * @param out_count       输出数量
 * @return 0 成功
 */
int plugin_permission_list_supported(char ***out_permissions,
                                     size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PLUGIN_PERMISSION_H */