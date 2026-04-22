// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file guard_integration.c
 * @brief SafetyGuard与Cupolas组件集成
 * 
 * 将SafetyGuard框架集成到现有Cupolas组件中：
 * 1. 权限检查后置守卫
 * 2. 命令执行前守卫
 * 3. 输入净化守卫
 * 4. 审计日志增强
 */

#include "guard_core.h"
#include "../../include/cupolas.h"
#include "../permission/permission.h"
#include "../sanitizer/sanitizer.h"
#include "../workbench/workbench.h"
#include "../audit/audit.h"
#include "../utils/cupolas_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// 全局守卫管理器实例
// ============================================================================

static guard_manager_t* g_guard_manager = NULL;
static bool g_guards_enabled = false;
static audit_logger_t* g_guard_audit_logger = NULL;
static char g_current_agent_id[64] = "system";

/**
 * @brief 记录安全守卫审计日志
 */
static void guard_log_security(const char* agent_id, const char* action,
                                const char* resource, int risk_level,
                                const char* detail) {
    if (!g_guard_audit_logger) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "risk_level=%d %s", risk_level, detail ? detail : "");
    audit_logger_log(g_guard_audit_logger, AUDIT_EVENT_PERMISSION,
                     agent_id, action, resource, buf, 0);
}

// ============================================================================
// Cupolas钩子函数
// ============================================================================

/**
 * @brief 权限检查守卫钩子
 * @note [SECURITY] 保留供未来权限增强使用
 */
static int permission_guard_hook(const char* agent_id, const char* action,
                                const char* resource, const char* context,
                                int permission_result) {
    if (!g_guard_manager || !g_guards_enabled) {
        return permission_result; // 守卫未启用，直接返回权限结果
    }
    
    // 只对允许的操作进行安全检查
    if (permission_result != 1) {
        return permission_result; // 权限已拒绝，无需进一步检查
    }
    
    // 创建检测上下文
    guard_context_t guard_ctx = {
        .operation = "permission_check",
        .resource = resource,
        .agent_id = agent_id,
        .session_id = context, // 使用context作为session_id
        .input_data = (void*)action,
        .input_size = action ? strlen(action) + 1 : 0,
        .context_data = NULL,
        .timestamp = cupolas_get_timestamp_ns()
    };
    
    // 执行安全检测
    const size_t MAX_RESULTS = 8;
    guard_result_t results[MAX_RESULTS];
    size_t actual_results = 0;
    
    int guard_result = guard_manager_check_sync(g_guard_manager, &guard_ctx,
                                               results, MAX_RESULTS,
                                               &actual_results);
    
    if (guard_result != CUPOLAS_OK) {
        // 守卫检测失败，记录日志但允许操作
        return permission_result;
    }
    
    // 检查检测结果
    for (size_t i = 0; i < actual_results; i++) {
        guard_result_t* result = &results[i];
        
        // 根据风险等级决定动作
        switch (result->risk_level) {
            case RISK_LEVEL_SAFE:
            case RISK_LEVEL_INFO:
                // 安全或信息级风险，允许操作
                break;
                
            case RISK_LEVEL_LOW:
                // 低风险，记录警告但允许操作
                guard_log_security(agent_id, action, resource, RISK_LEVEL_LOW, "low_risk_allowed");
                break;

            case RISK_LEVEL_MEDIUM:
            case RISK_LEVEL_HIGH:
            case RISK_LEVEL_CRITICAL:
                // 中高风险，根据推荐动作处理
                if (result->recommended_action == GUARD_ACTION_BLOCK ||
                    result->recommended_action == GUARD_ACTION_ISOLATE ||
                    result->recommended_action == GUARD_ACTION_TERMINATE) {
                    // 阻断操作
                    guard_log_security(agent_id, action, resource, result->risk_level, "blocked_by_guard");
                    return 0; // 拒绝访问
                }
                break;
                
            default:
                break;
        }
    }
    
    return permission_result; // 安全检测通过，允许操作
}

/**
 * @brief 命令执行守卫钩子
 * @note [SECURITY] 保留供未来命令安全增强使用
 */
static int command_execution_guard_hook(const char* command, char* const argv[]) {
    if (!g_guard_manager || !g_guards_enabled) {
        return CUPOLAS_OK; // 守卫未启用，允许执行
    }
    
    // 构建命令字符串用于检测
    char cmd_buffer[1024] = {0};
    size_t pos = 0;
    
    // 添加命令
    if (command) {
        pos += snprintf(cmd_buffer + pos, sizeof(cmd_buffer) - pos, "%s", command);
    }
    
    // 添加参数
    if (argv) {
        for (int i = 0; argv[i] && pos < sizeof(cmd_buffer) - 1; i++) {
            pos += snprintf(cmd_buffer + pos, sizeof(cmd_buffer) - pos, " %s", argv[i]);
        }
    }
    
    // 创建检测上下文
    guard_context_t guard_ctx = {
        .operation = "command_execution",
        .resource = "workbench",
        .agent_id = g_current_agent_id,
        .session_id = NULL,
        .input_data = cmd_buffer,
        .input_size = strlen(cmd_buffer) + 1,
        .context_data = NULL,
        .timestamp = cupolas_get_timestamp_ns()
    };
    
    // 执行安全检测
    const size_t MAX_RESULTS = 8;
    guard_result_t results[MAX_RESULTS];
    size_t actual_results = 0;
    
    int guard_result = guard_manager_check_sync(g_guard_manager, &guard_ctx,
                                               results, MAX_RESULTS,
                                               &actual_results);
    
    if (guard_result != CUPOLAS_OK) {
        // 守卫检测失败，记录日志但允许执行
        return CUPOLAS_OK;
    }
    
    // 检查检测结果
    for (size_t i = 0; i < actual_results; i++) {
        guard_result_t* result = &results[i];
        
        // 根据风险等级决定动作
        switch (result->risk_level) {
            case RISK_LEVEL_SAFE:
            case RISK_LEVEL_INFO:
                // 安全或信息级风险，允许执行
                break;
                
            case RISK_LEVEL_LOW:
                // 低风险，记录警告但允许执行
                guard_log_security("system", command, "command_execute", RISK_LEVEL_LOW, "low_risk_cmd_allowed");
                break;

            case RISK_LEVEL_MEDIUM:
            case RISK_LEVEL_HIGH:
            case RISK_LEVEL_CRITICAL:
                // 中高风险，根据推荐动作处理
                if (result->recommended_action == GUARD_ACTION_BLOCK ||
                    result->recommended_action == GUARD_ACTION_ISOLATE ||
                    result->recommended_action == GUARD_ACTION_TERMINATE) {
                    // 阻断命令执行
                    guard_log_security("system", command, "command_execute", result->risk_level, "cmd_blocked_by_guard");
                    return cupolas_ERROR_PERMISSION;
                }
                break;
                
            default:
                break;
        }
    }
    
    return CUPOLAS_OK; // 安全检测通过，允许执行
}

/**
 * @brief 输入净化守卫钩子
 * @note [SECURITY] 保留供未来输入净化增强使用
 */
static int sanitizer_guard_hook(const char* input,
                               char* output, size_t output_size,
                               int sanitizer_result) {
    (void)input;
    if (!g_guard_manager || !g_guards_enabled) {
        return sanitizer_result; // 守卫未启用
    }
    
    // 只在净化成功后检查输出
    if (sanitizer_result != CUPOLAS_OK || !output) {
        return sanitizer_result;
    }
    
    // 创建检测上下文
    guard_context_t guard_ctx = {
        .operation = "input_sanitization",
        .resource = "sanitizer",
        .agent_id = g_current_agent_id,
        .session_id = NULL,
        .input_data = (void*)output,
        .input_size = strlen(output) + 1,
        .context_data = NULL,
        .timestamp = cupolas_get_timestamp_ns()
    };
    
    // 执行安全检测
    const size_t MAX_RESULTS = 8;
    guard_result_t results[MAX_RESULTS];
    size_t actual_results = 0;
    
    int guard_result = guard_manager_check_sync(g_guard_manager, &guard_ctx,
                                               results, MAX_RESULTS,
                                               &actual_results);
    
    if (guard_result != CUPOLAS_OK) {
        return sanitizer_result;
    }
    
    // 检查检测结果
    for (size_t i = 0; i < actual_results; i++) {
        guard_result_t* result = &results[i];
        
        // 高风险输入需要特殊处理
        if (result->risk_level >= RISK_LEVEL_MEDIUM) {
            // 记录安全事件
            guard_log_security("system", "sanitize_input", "input_data", result->risk_level, "high_risk_input_detected");
            
            // 对于严重风险，可以清空输出或返回错误
            if (result->risk_level == RISK_LEVEL_CRITICAL) {
                if (output_size > 0) output[0] = '\0';
                return cupolas_ERROR_INVALID_ARG;
            }
        }
    }
    
    return sanitizer_result;
}

// ============================================================================
// 公共集成API
// ============================================================================

/**
 * @brief 初始化Cupolas守卫集成
 * @param config 守卫管理器配置
 * @return 错误码
 */
CUPOLAS_API int cupolas_guards_init(const guard_manager_config_t* config) {
    if (g_guard_manager) {
        return cupolas_ERROR_BUSY; // 已初始化
    }
    
    // 创建守卫管理器
    g_guard_manager = guard_manager_create(config);
    if (!g_guard_manager) {
        return cupolas_ERROR_NO_MEMORY;
    }

    g_guards_enabled = true;

    // 创建审计日志记录器
    if (!g_guard_audit_logger) {
        g_guard_audit_logger = audit_logger_create("/tmp/cupolas_audit", "guard",
                                                    1024 * 1024, 10);
    }

    return CUPOLAS_OK;
}

/**
 * @brief 清理Cupolas守卫集成
 */
CUPOLAS_API void cupolas_guards_cleanup(void) {
    if (g_guard_audit_logger) {
        audit_logger_flush(g_guard_audit_logger);
        audit_logger_destroy(g_guard_audit_logger);
        g_guard_audit_logger = NULL;
    }
    if (g_guard_manager) {
        guard_manager_destroy(g_guard_manager);
        g_guard_manager = NULL;
    }
    memset(g_current_agent_id, 0, sizeof(g_current_agent_id));
    strncpy(g_current_agent_id, "system", sizeof(g_current_agent_id) - 1);
    g_guards_enabled = false;
}

/**
 * @brief 设置当前代理ID（供外部调用者设置真实代理身份）
 * @param agent_id 代理标识符
 */
CUPOLAS_API void cupolas_guards_set_agent_id(const char* agent_id) {
    if (!agent_id) return;
    strncpy(g_current_agent_id, agent_id, sizeof(g_current_agent_id) - 1);
    g_current_agent_id[sizeof(g_current_agent_id) - 1] = '\0';
}

/**
 * @brief 启用守卫
 */
CUPOLAS_API void cupolas_guards_enable(void) {
    g_guards_enabled = true;
}

/**
 * @brief 禁用守卫
 */
CUPOLAS_API void cupolas_guards_disable(void) {
    g_guards_enabled = false;
}

/**
 * @brief 检查守卫是否启用
 * @return 1启用，0禁用
 */
CUPOLAS_API int cupolas_guards_is_enabled(void) {
    return g_guards_enabled ? 1 : 0;
}

/**
 * @brief 获取守卫管理器实例
 * @return 守卫管理器句柄
 */
CUPOLAS_API guard_manager_t* cupolas_guards_get_manager(void) {
    return g_guard_manager;
}

/**
 * @brief 注册守卫到Cupolas
 * @param guard 守卫实例
 * @return 错误码
 */
CUPOLAS_API int cupolas_guards_register_guard(guard_t* guard) {
    if (!g_guard_manager) {
        return cupolas_ERROR_BUSY;
    }
    
    return guard_manager_register_guard(g_guard_manager, guard);
}

/**
 * @brief 执行安全检测（针对Cupolas操作）
 * @param operation 操作名称
 * @param resource 资源标识
 * @param agent_id 代理ID
 * @param input_data 输入数据
 * @param input_size 输入数据大小
 * @param results 结果数组（输出）
 * @param max_results 最大结果数
 * @param actual_results 实际结果数（输出）
 * @return 错误码
 */
CUPOLAS_API int cupolas_guards_check(
    const char* operation,
    const char* resource,
    const char* agent_id,
    const void* input_data,
    size_t input_size,
    guard_result_t* results,
    size_t max_results,
    size_t* actual_results)
{
    if (!g_guard_manager || !g_guards_enabled) {
        if (actual_results) *actual_results = 0;
        return cupolas_ERROR_BUSY;
    }
    
    // 创建检测上下文
    guard_context_t guard_ctx = {
        .operation = operation,
        .resource = resource,
        .agent_id = agent_id,
        .session_id = NULL,
        .input_data = (void*)input_data,
        .input_size = input_size,
        .context_data = NULL,
        .timestamp = cupolas_get_timestamp_ns()
    };
    
    return guard_manager_check_sync(g_guard_manager, &guard_ctx,
                                   results, max_results, actual_results);
}

// ============================================================================
// 钩子注册函数
// ============================================================================

/**
 * @brief 注册Cupolas钩子
 *
 * 将守卫钩子注册到Cupolas各个组件。
 * 注意：此函数需要在Cupolas初始化后调用。
 */
CUPOLAS_API int cupolas_guards_register_hooks(void) {
    if (!g_guard_manager) {
        return cupolas_ERROR_BUSY;
    }

    static int hooks_registered = 0;
    if (hooks_registered) {
        return CUPOLAS_OK;
    }

    printf("[GUARD] Registering safety hooks to Cupolas components...\n");

    int registered_count = 0;

#ifdef CUPOLAS_HAS_PERMISSION_HOOK
    extern int permission_register_post_check_hook(int (*hook)(const char*, const char*, const char*, const char*, int));
    if (permission_register_post_check_hook(permission_guard_hook) == 0) {
        registered_count++;
        printf("[GUARD] ✓ Permission post-check hook registered\n");
    } else {
        printf("[GUARD] ✗ Failed to register permission hook\n");
    }
#endif

#ifdef CUPOLAS_HAS_WORKBENCH_HOOK
    extern int workbench_register_pre_exec_hook(int (*hook)(const char*, char* const[]));
    if (workbench_register_pre_exec_hook(command_execution_guard_hook) == 0) {
        registered_count++;
        printf("[GUARD] ✓ Workbench pre-execution hook registered\n");
    } else {
        printf("[GUARD] ✗ Failed to register workbench hook\n");
    }
#endif

#ifdef CUPOLAS_HAS_SANITIZER_HOOK
    extern int sanitizer_register_post_process_hook(int (*hook)(const char*, char*, size_t, int));
    if (sanitizer_register_post_process_hook(sanitizer_guard_hook) == 0) {
        registered_count++;
        printf("[GUARD] ✓ Sanitizer post-process hook registered\n");
    } else {
        printf("[GUARD] ✗ Failed to register sanitizer hook\n");
    }
#endif

    if (registered_count > 0) {
        hooks_registered = 1;
        g_guards_enabled = true;
        printf("[GUARD] Successfully registered %d safety hooks\n", registered_count);
        return CUPOLAS_OK;
    }

    printf("[GUARD] Warning: No hooks registered (component hook APIs not available)\n");
    printf("[GUARD] Guards will work in standalone mode (explicit checks only)\n");
    return CUPOLAS_OK;
}

/**
 * @brief 注销Cupolas钩子
 */
CUPOLAS_API void cupolas_guards_unregister_hooks(void) {
    printf("[GUARD] Unregistering safety hooks from Cupolas components...\n");

#ifdef CUPOLAS_HAS_PERMISSION_HOOK
    extern void permission_unregister_post_check_hook(void);
    permission_unregister_post_check_hook();
    printf("[GUARD] ✓ Permission hook unregistered\n");
#endif

#ifdef CUPOLAS_HAS_WORKBENCH_HOOK
    extern void workbench_unregister_pre_exec_hook(void);
    workbench_unregister_pre_exec_hook();
    printf("[GUARD] ✓ Workbench hook unregistered\n");
#endif

#ifdef CUPOLAS_HAS_SANITIZER_HOOK
    extern void sanitizer_unregister_post_process_hook(void);
    sanitizer_unregister_post_process_hook();
    printf("[GUARD] ✓ Sanitizer hook unregistered\n");
#endif

    g_guards_enabled = false;
    printf("[GUARD] All safety hooks unregistered\n");
}