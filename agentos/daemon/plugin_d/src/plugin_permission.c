/**
 * @file plugin_permission.c
 * @brief P2.2.4: 插件权限校验 — manifest 权限 ↔ Cupolas 守卫类型映射
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 将插件 manifest 中声明的权限映射到 Cupolas 安全穹顶的守卫类型。
 * 加载插件时自动校验权限，不符合安全策略的插件拒绝加载。
 */

#include "plugin_permission.h"
#include "safety_guard.h"
#include "safe_string_utils.h"

#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 支持的权限列表 ==================== */

static const char *SUPPORTED_PERMISSIONS[] = {
    "file_read",        "file_write",
    "network_outbound", "network_inbound",
    "tool_execute",     "memory_access",
    "hook_register",    "system_call",
    "process_spawn",    "ipc_connect",
    "service_discovery", "config_read",
    "config_write",     "log_write",
    "metrics_export",   "audit_trigger",
    NULL
};

/* ==================== 内部状态 ==================== */

static struct {
    plugin_permission_config_t config;
    safety_guard_context_t *guard_ctx;
    bool initialized;
} g_permission;

/* ==================== 生命周期实现 ==================== */

int plugin_permission_init(const plugin_permission_config_t *config)
{
    if (g_permission.initialized) return 0;

    __builtin_memset(&g_permission, 0, sizeof(g_permission));

    if (config) {
        g_permission.config = *config;
    } else {
        g_permission.config.enable_strict_mode = true;
        g_permission.config.enable_audit_log = true;
        g_permission.config.safety_policy_path[0] = '\0';
        g_permission.config.agent_id = "plugin_d";
    }

    /* 初始化 SafetyGuard 上下文 */
    g_permission.guard_ctx = safety_guard_create();
    if (!g_permission.guard_ctx) {
        AGENTOS_LOG_WARN("PluginPermission: safety_guard_create failed, "
                         "permission checks will be local-only");
    }

    g_permission.initialized = true;

    AGENTOS_LOG_INFO("PluginPermission: initialized (strict=%d, audit=%d)",
                     g_permission.config.enable_strict_mode,
                     g_permission.config.enable_audit_log);
    return 0;
}

void plugin_permission_destroy(void)
{
    if (g_permission.guard_ctx) {
        safety_guard_destroy(g_permission.guard_ctx);
        g_permission.guard_ctx = NULL;
    }

    AGENTOS_LOG_INFO("PluginPermission: destroyed");
    g_permission.initialized = false;
}

/* ==================== 权限映射 ==================== */

int plugin_permission_map_to_guard(const char *permission,
                                   safety_guard_type_t *out_guard)
{
    if (!permission || !out_guard) return -1;

    /* 权限字符串 → Cupolas 守卫类型映射 */
    if (strcmp(permission, "file_read") == 0) {
        *out_guard = SAFETY_GUARD_FILE_READ;
    } else if (strcmp(permission, "file_write") == 0) {
        *out_guard = SAFETY_GUARD_FILE_WRITE;
    } else if (strcmp(permission, "network_outbound") == 0 ||
               strcmp(permission, "network_inbound") == 0) {
        *out_guard = SAFETY_GUARD_NETWORK;
    } else if (strcmp(permission, "tool_execute") == 0) {
        *out_guard = SAFETY_GUARD_TOOL_EXEC;
    } else if (strcmp(permission, "memory_access") == 0) {
        *out_guard = SAFETY_GUARD_MEMORY;
    } else if (strcmp(permission, "hook_register") == 0) {
        *out_guard = SAFETY_GUARD_HOOK;
    } else if (strcmp(permission, "system_call") == 0) {
        *out_guard = SAFETY_GUARD_SYSTEM;
    } else if (strcmp(permission, "process_spawn") == 0) {
        *out_guard = SAFETY_GUARD_PROCESS;
    } else if (strcmp(permission, "ipc_connect") == 0) {
        *out_guard = SAFETY_GUARD_IPC;
    } else if (strcmp(permission, "service_discovery") == 0) {
        *out_guard = SAFETY_GUARD_SERVICE_DISCOVERY;
    } else if (strcmp(permission, "config_read") == 0 ||
               strcmp(permission, "config_write") == 0) {
        *out_guard = SAFETY_GUARD_CONFIG;
    } else if (strcmp(permission, "log_write") == 0) {
        *out_guard = SAFETY_GUARD_LOGGING;
    } else if (strcmp(permission, "metrics_export") == 0) {
        *out_guard = SAFETY_GUARD_METRICS;
    } else if (strcmp(permission, "audit_trigger") == 0) {
        *out_guard = SAFETY_GUARD_AUDIT;
    } else {
        AGENTOS_LOG_WARN("PluginPermission: unknown permission '%s'",
                         permission);
        return -1;
    }

    return 0;
}

/* ==================== 权限校验 ==================== */

plugin_permission_result_t plugin_permission_check(
    const char (*permissions)[64],
    uint32_t permission_count,
    const char *plugin_name,
    char *out_denied,
    size_t out_denied_size)
{
    if (!permissions || permission_count == 0) {
        /* 无权限声明 — 在严格模式下拒绝 */
        if (g_permission.config.enable_strict_mode) {
            if (out_denied && out_denied_size > 0) {
                safe_strcpy(out_denied, "no permissions declared",
                            out_denied_size);
            }
            AGENTOS_LOG_WARN("PluginPermission: plugin '%s' has no permissions "
                             "(strict mode)", plugin_name);
            return PLUGIN_PERM_DENIED;
        }
        return PLUGIN_PERM_ALLOWED;
    }

    if (!g_permission.initialized) {
        plugin_permission_init(NULL);
    }

    plugin_permission_result_t overall = PLUGIN_PERM_ALLOWED;
    char denied_list[512] = {0};
    size_t denied_offset = 0;

    for (uint32_t i = 0; i < permission_count; i++) {
        const char *perm = permissions[i];
        if (!perm || perm[0] == '\0') continue;

        /* 映射到守卫类型 */
        safety_guard_type_t guard_type;
        int map_ret = plugin_permission_map_to_guard(perm, &guard_type);

        if (map_ret != 0) {
            /* 未知权限 */
            AGENTOS_LOG_WARN("PluginPermission: plugin '%s' declares unknown "
                             "permission '%s'", plugin_name, perm);
            if (g_permission.config.enable_strict_mode) {
                overall = PLUGIN_PERM_UNKNOWN;
                if (denied_offset < sizeof(denied_list) - 1) {
                    denied_offset += (size_t)snprintf(
                        denied_list + denied_offset,
                        sizeof(denied_list) - denied_offset,
                        "%s%s", denied_offset > 0 ? "," : "", perm);
                }
            }
            continue;
        }

        /* 通过 SafetyGuard 检查 */
        if (g_permission.guard_ctx) {
            bool allowed = false;
            int check_ret = safety_guard_check_permission(
                g_permission.guard_ctx, guard_type,
                g_permission.config.agent_id ? g_permission.config.agent_id
                                             : "plugin_d",
                &allowed);

            if (check_ret != 0 || !allowed) {
                AGENTOS_LOG_WARN("PluginPermission: permission '%s' denied "
                                 "for plugin '%s'", perm, plugin_name);
                overall = PLUGIN_PERM_DENIED;
                if (denied_offset < sizeof(denied_list) - 1) {
                    denied_offset += (size_t)snprintf(
                        denied_list + denied_offset,
                        sizeof(denied_list) - denied_offset,
                        "%s%s", denied_offset > 0 ? "," : "", perm);
                }
            } else {
                AGENTOS_LOG_DEBUG("PluginPermission: permission '%s' allowed "
                                  "for plugin '%s'", perm, plugin_name);
            }
        }
    }

    if (out_denied && out_denied_size > 0) {
        safe_strcpy(out_denied, denied_list, out_denied_size);
    }

    if (overall != PLUGIN_PERM_ALLOWED && g_permission.config.enable_audit_log) {
        AGENTOS_LOG_INFO("PluginPermission: AUDIT plugin='%s' result=%d "
                         "denied='%s'",
                         plugin_name, overall, denied_list);
    }

    return overall;
}

/* ==================== 权限描述 ==================== */

const char *plugin_permission_description(const char *permission)
{
    if (!permission) return "unknown";

    if (strcmp(permission, "file_read") == 0)
        return "Read files from the filesystem";
    if (strcmp(permission, "file_write") == 0)
        return "Write files to the filesystem";
    if (strcmp(permission, "network_outbound") == 0)
        return "Make outbound network connections";
    if (strcmp(permission, "network_inbound") == 0)
        return "Accept inbound network connections";
    if (strcmp(permission, "tool_execute") == 0)
        return "Execute tools via tool_d";
    if (strcmp(permission, "memory_access") == 0)
        return "Access agent memory via memory_d";
    if (strcmp(permission, "hook_register") == 0)
        return "Register hooks in hook_d";
    if (strcmp(permission, "system_call") == 0)
        return "Make system calls";
    if (strcmp(permission, "process_spawn") == 0)
        return "Spawn child processes";
    if (strcmp(permission, "ipc_connect") == 0)
        return "Connect to IPC bus";
    if (strcmp(permission, "service_discovery") == 0)
        return "Use service discovery";
    if (strcmp(permission, "config_read") == 0)
        return "Read configuration";
    if (strcmp(permission, "config_write") == 0)
        return "Write configuration";
    if (strcmp(permission, "log_write") == 0)
        return "Write log entries";
    if (strcmp(permission, "metrics_export") == 0)
        return "Export metrics";
    if (strcmp(permission, "audit_trigger") == 0)
        return "Trigger audit events";

    return "unknown permission";
}

int plugin_permission_list_supported(char ***out_permissions,
                                     size_t *out_count)
{
    if (!out_permissions || !out_count) return -1;

    /* 计数 */
    size_t count = 0;
    while (SUPPORTED_PERMISSIONS[count]) count++;

    char **list = (char **)AGENTOS_CALLOC(count, sizeof(char *));
    if (!list) return -1;

    for (size_t i = 0; i < count; i++) {
        list[i] = AGENTOS_STRDUP(SUPPORTED_PERMISSIONS[i]);
    }

    *out_permissions = list;
    *out_count = count;
    return 0;
}