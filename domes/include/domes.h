/**
 * @file domes.h
 * @brief Domes 安全隔离层公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_DOMES_H
#define AGENTOS_DOMES_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define DOMES_API_VERSION_MAJOR 1
#define DOMES_API_VERSION_MINOR 0
#define DOMES_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include <stddef.h>
#include <stdint.h>

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
// From data intelligence emerges. by spharx
#endif

/* 前向声明 */
typedef struct domes_core domes_t;

/**
 * @brief 域配置（所有路径均为绝对路径或相对工作目录）
 */
typedef struct domes_config {
    // 虚拟工位
    const char* workbench_type;          /**< "process", "container", "wasm" */
    uint64_t    workbench_memory_bytes;  /**< 内存限制（字节） */
    float       workbench_cpu_quota;     /**< CPU 配额（核心数，0 表示无限制） */
    int         workbench_network;       /**< 是否启用网络（0/1） */
    const char* workbench_rootfs;        /**< 容器根文件系统路径（容器模式有效） */

    // 权限
    const char* permission_rules_path;   /**< 权限规则文件路径（YAML） */
    uint32_t    permission_cache_ttl_ms; /**< 权限缓存 TTL（毫秒） */

    // 审计
    const char* audit_log_path;          /**< 审计日志路径（如 "/var/log/agentos/audit.log"） */
    uint64_t    audit_max_size_bytes;    /**< 单日志文件最大字节，0 不轮转 */
    uint32_t    audit_max_files;         /**< 最大保留文件数 */
    const char* audit_format;            /**< "json" 或 "csv" */

    // 净化
    uint32_t    sanitizer_max_input_len; /**< 最大输入长度 */
    const char* sanitizer_rules_path;    /**< 净化规则文件路径（JSON） */
} domes_config_t;

/**
 * @brief 初始化 Domain 模块
 * @param config 配置（若为 NULL 使用默认值）
 * @param out_domain 输出句柄
 * @return 0 成功，-1 失败（错误信息写入日志）
 */
AGENTOS_API int domes_init(const domes_config_t* config, domes_t** out_domain);

/**
 * @brief 销毁 Domain 模块，等待所有异步操作完成
 * @param domain 句柄
 */
AGENTOS_API void domes_destroy(domes_t* domain);

/* ==================== 虚拟工位接口 ==================== */

/**
 * @brief 为指定 Agent 创建虚拟工位
 * @param domain 句柄
 * @param agent_id Agent ID
 * @param out_workbench_id 输出工位 ID（需调用者 free）
 * @return 0 成功，-1 失败
 */
AGENTOS_API int domes_workbench_create(domes_t* domain, const char* agent_id, char** out_workbench_id);

/**
 * @brief 在工位中执行命令
 * @param domain 句柄
 * @param workbench_id 工位 ID
 * @param argv 命令及参数（以 NULL 结尾的字符串数组）
 * @param timeout_ms 超时（毫秒）
 * @param out_stdout 输出 stdout（需调用者 free）
 * @param out_stderr 输出 stderr（需调用者 free）
 * @param out_exit_code 退出码
 * @param out_error 详细错误信息（需调用者 free）
 * @return 0 成功，-1 失败（工位不存在或执行错误）
 */
AGENTOS_API int domes_workbench_exec(domes_t* domain, const char* workbench_id,
                          const char* const* argv, uint32_t timeout_ms,
                          char** out_stdout, char** out_stderr,
                          int* out_exit_code, char** out_error);

/**
 * @brief 销毁虚拟工位
 * @param domain 句柄
 * @param workbench_id 工位 ID
 */
AGENTOS_API void domes_workbench_destroy(domes_t* domain, const char* workbench_id);

/**
 * @brief 列出所有活跃工位
 * @param domain 句柄
 * @param out_workbench_ids 输出工位 ID 数组（需调用者 free 每个元素及数组）
 * @param out_count 输出数量
 * @return 0 成功，-1 失败
 */
AGENTOS_API int domes_workbench_list(domes_t* domain, char*** out_workbench_ids, size_t* out_count);

/* ==================== 权限裁决接口 ==================== */

/**
 * @brief 检查权限
 * @param domain 句柄
 * @param agent_id Agent ID
 * @param action 操作类型（如 "file:read"）
 * @param resource 资源标识（如 "/etc/passwd"）
 * @param context 额外上下文（JSON 字符串，可为 NULL）
 * @return 1 允许，0 拒绝，-1 错误（如引擎未初始化）
 */
AGENTOS_API int domes_permission_check(domes_t* domain, const char* agent_id,
                          const char* action, const char* resource,
                          const char* context);

/**
 * @brief 重新加载权限规则（热更新）
 * @param domain 句柄
 * @return 0 成功，-1 失败
 */
AGENTOS_API int domes_permission_reload(domes_t* domain);

/* ==================== 审计接口 ==================== */

/**
 * @brief 记录一条审计事件（异步，立即返回）
 * @param domain 句柄
 * @param agent_id Agent ID
 * @param tool_name 工具名称
 * @param input 输入（JSON 字符串，会被复制）
 * @param output 输出（JSON 字符串，会被复制）
 * @param duration_ms 耗时（毫秒）
 * @param success 是否成功
 * @param error_msg 错误信息（可为 NULL）
 * @return 0 成功（已入队），-1 失败（队列满等）
 */
AGENTOS_API int domes_audit_record(domes_t* domain, const char* agent_id,
                        const char* tool_name, const char* input,
                        const char* output, uint32_t duration_ms,
                        int success, const char* error_msg);

/**
 * @brief 同步查询审计日志（可能阻塞）
 * @param domain 句柄
 * @param agent_id Agent ID（可为 NULL 表示所有）
 * @param start_time 起始时间戳（0 表示不限制）
 * @param end_time 结束时间戳（0 表示不限制）
 * @param limit 最大条数
 * @param out_events 输出事件数组（JSON 字符串数组，需调用者 free 每个元素）
 * @param out_count 输出数量
 * @return 0 成功，-1 失败
 */
AGENTOS_API int domes_audit_query(domes_t* domain, const char* agent_id,
                       uint64_t start_time, uint64_t end_time,
                       uint32_t limit, char*** out_events, size_t* out_count);

/* ==================== 输入净化接口 ==================== */

/**
 * @brief 净化输入字符串
 * @param domain 句柄
 * @param input 原始输入
 * @param out_cleaned 输出净化后的字符串（需调用者 free）
 * @param out_risk_level 输出风险等级（0 无风险，1 低，2 中，3 高）
 * @return 0 成功，-1 失败
 */
AGENTOS_API int domes_sanitize(domes_t* domain, const char* input,
                    char** out_cleaned, int* out_risk_level);

#ifdef __cplusplus
}
#endif
#endif /* AGENTOS_DOMES_H */