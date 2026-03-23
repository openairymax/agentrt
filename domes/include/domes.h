/**
 * @file domes.h
 * @brief domes 模块公共接口 - AgentOS 安全沙箱
 * @author Spharx
 * @date 2024
 * 
 * domes 是 AgentOS 的安全沙箱模块，提供：
 * - 权限裁决（Permission）：基于规则的访问控制
 * - 输入净化（Sanitizer）：防止注入攻击
 * - 审计日志（Audit）：操作追踪与合规
 * - 虚拟工位（Workbench）：隔离的执行环境
 * 
 * 设计原则：
 * - 安全内生：每个 Agent 运行在沙箱中，权限最小化
 * - 高性能：缓存 + 异步写入
 * - 跨平台：Windows/Linux/macOS
 */

#ifndef DOMES_H
#define DOMES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 初始化与清理
 * ============================================================================ */

/**
 * @brief 初始化 domes 模块
 * @param config_path 配置文件路径（可选，NULL 使用默认配置）
 * @return 0 成功，其他失败
 */
int domes_init(const char* config_path);

/**
 * @brief 清理 domes 模块
 */
void domes_cleanup(void);

/**
 * @brief 获取版本号
 * @return 版本字符串
 */
const char* domes_version(void);

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
int domes_check_permission(const char* agent_id, const char* action,
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
int domes_add_permission_rule(const char* agent_id, const char* action,
                               const char* resource, int allow, int priority);

/**
 * @brief 清空权限缓存
 */
void domes_clear_permission_cache(void);

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
int domes_sanitize_input(const char* input, char* output, size_t output_size);

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
int domes_execute_command(const char* command, char* const argv[],
                          int* exit_code, char* stdout_buf, size_t stdout_size,
                          char* stderr_buf, size_t stderr_size);

/* ============================================================================
 * 审计日志
 * ============================================================================ */

/**
 * @brief 刷新审计日志
 */
void domes_flush_audit_log(void);

/* ============================================================================
 * 错误码
 * ============================================================================ */

#define DOMES_OK                    0
#define DOMES_ERROR_UNKNOWN         -1
#define DOMES_ERROR_INVALID_ARG     -2
#define DOMES_ERROR_NO_MEMORY       -3
#define DOMES_ERROR_NOT_FOUND       -4
#define DOMES_ERROR_PERMISSION      -5
#define DOMES_ERROR_BUSY            -6
#define DOMES_ERROR_TIMEOUT         -7
#define DOMES_ERROR_WOULD_BLOCK     -8
#define DOMES_ERROR_OVERFLOW        -9
#define DOMES_ERROR_NOT_SUPPORTED   -10
#define DOMES_ERROR_IO              -11

#ifdef __cplusplus
}
#endif

#endif /* DOMES_H */
