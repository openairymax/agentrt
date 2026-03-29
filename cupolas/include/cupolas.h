/**
 * @file cupolas.h
 * @brief cupolas 模块公共接口 - AgentOS 安全穹顶
 * @author Spharx
 * @date 2024
 * 
 * cupolas 是 AgentOS 的安全穹顶模块，提供：
 * - 权限裁决（Permission）：基于规则的访问控制
 * - 输入净化（Sanitizer）：防止注入攻击
 * - 审计日志（Audit）：操作追踪与合规
 * - 虚拟工位（Workbench）：隔离的执行环境
 * 
 * 设计原则：
 * - 安全内生：每个 Agent 运行在穹顶中，权限最小化
 * - 高性能：缓存 + 异步写入
 * - 跨平台：Windows/Linux/macOS
 * 
 * 错误码说明：
 * - 所有函数返回 agentos_error_t 类型错误码
 * - 成功返回 AGENTOS_OK (0)
 * - 错误码定义参考 atoms/corekern/include/error.h
 * 
 * @note 为保持向后兼容，cupolas_ERROR_* 别名仍然可用
 */

#ifndef CUPOLAS_H
#define CUPOLAS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 统一错误码（兼容 AgentOS 标准）
 * ============================================================================ */

typedef int agentos_error_t;

#define AGENTOS_OK                     0
#define AGENTOS_ERR_UNKNOWN           -1
#define AGENTOS_ERR_INVALID_PARAM     -2
#define AGENTOS_ERR_NULL_POINTER      -3
#define AGENTOS_ERR_OUT_OF_MEMORY     -4
#define AGENTOS_ERR_BUFFER_TOO_SMALL  -5
#define AGENTOS_ERR_NOT_FOUND         -6
#define AGENTOS_ERR_ALREADY_EXISTS    -7
#define AGENTOS_ERR_TIMEOUT           -8
#define AGENTOS_ERR_NOT_SUPPORTED     -9
#define AGENTOS_ERR_PERMISSION_DENIED -10
#define AGENTOS_ERR_IO               -11
#define AGENTOS_ERR_STATE_ERROR      -13
#define AGENTOS_ERR_OVERFLOW         -14

#define cupolas_OK                    AGENTOS_OK
#define cupolas_ERROR_UNKNOWN         AGENTOS_ERR_UNKNOWN
#define cupolas_ERROR_INVALID_ARG     AGENTOS_ERR_INVALID_PARAM
#define cupolas_ERROR_NO_MEMORY       AGENTOS_ERR_OUT_OF_MEMORY
#define cupolas_ERROR_NOT_FOUND       AGENTOS_ERR_NOT_FOUND
#define cupolas_ERROR_PERMISSION      AGENTOS_ERR_PERMISSION_DENIED
#define cupolas_ERROR_BUSY            AGENTOS_ERR_STATE_ERROR
#define cupolas_ERROR_TIMEOUT         AGENTOS_ERR_TIMEOUT
#define cupolas_ERROR_WOULD_BLOCK     AGENTOS_ERR_STATE_ERROR
#define cupolas_ERROR_OVERFLOW        AGENTOS_ERR_OVERFLOW
#define cupolas_ERROR_NOT_SUPPORTED   AGENTOS_ERR_NOT_SUPPORTED
#define cupolas_ERROR_IO             AGENTOS_ERR_IO

/* ============================================================================
 * 初始化与清理
 * ============================================================================ */

/**
 * @brief 初始化 cupolas 模块
 * @param config_path 配置文件路径（可选，NULL 使用默认配置）
 * @return 0 成功，其他失败
 */
int cupolas_init(const char* config_path);

/**
 * @brief 清理 cupolas 模块
 */
void cupolas_cleanup(void);

/**
 * @brief 获取版本号
 * @return 版本字符串
 */
const char* cupolas_version(void);

/* ============================================================================
 * 权限管理
 * ============================================================================ */

/**
 * @brief 检查权限
 * @param agent_id Agent ID
 * @param action 操作类型（如 "read", "write", "execute"）
 * @param resource 资源路径
 * @param context 上下文信息（可选）
 * @return 1 允许，0 拒绝
 */
int cupolas_check_permission(const char* agent_id, const char* action,
                           const char* resource, const char* context);

/**
 * @brief 添加权限规则
 * @param agent_id Agent ID（NULL 或 "*" 表示通配）
 * @param action 操作（NULL 或 "*" 表示通配）
 * @param resource 资源模式（支持 glob 模式）
 * @param allow 1 允许，0 拒绝
 * @param priority 优先级（数值越大优先级越高）
 * @return 0 成功，其他失败
 */
int cupolas_add_permission_rule(const char* agent_id, const char* action,
                               const char* resource, int allow, int priority);

/**
 * @brief 清空权限缓存
 */
void cupolas_clear_permission_cache(void);

/* ============================================================================
 * 输入净化
 * ============================================================================ */

/**
 * @brief 净化输入
 * @param input 输入字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 0 成功，其他失败
 */
int cupolas_sanitize_input(const char* input, char* output, size_t output_size);

/* ============================================================================
 * 命令执行
 * ============================================================================ */

/**
 * @brief 执行命令
 * @param command 命令路径
 * @param argv 参数数组（以 NULL 结尾）
 * @param exit_code 退出码输出
 * @param stdout_buf 标准输出缓冲区
 * @param stdout_size 标准输出缓冲区大小
 * @param stderr_buf 标准错误缓冲区
 * @param stderr_size 标准错误缓冲区大小
 * @return 0 成功，其他失败
 */
int cupolas_execute_command(const char* command, char* const argv[],
                          int* exit_code, char* stdout_buf, size_t stdout_size,
                          char* stderr_buf, size_t stderr_size);

/* ============================================================================
 * 审计日志
 * ============================================================================ */

/**
 * @brief 刷新审计日志
 */
void cupolas_flush_audit_log(void);

/* ============================================================================
 * iOS级安全模块
 * ============================================================================ */

/* Security 子模块头文件 - 位于 src/security/ 目录 */
/* 用户应直接包含具体头文件：
 * #include "cupolas_signature.h"
 * #include "cupolas_vault.h"
 * #include "cupolas_entitlements.h"
 * #include "cupolas_runtime_protection.h"
 * #include "cupolas_network_security.h"
 */

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_H */
