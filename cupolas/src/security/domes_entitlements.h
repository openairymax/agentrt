/**
 * @file domes_entitlements.h
 * @brief Entitlements 权限声明 - 细粒度权限声明机制
 * @author Spharx
 * @date 2026
 *
 * 设计原则：
 * - 声明式权限：所有权限必须在 Entitlements 中显式声明
 * - 最小权限原则：默认拒绝所有未声明的权限
 * - 签名验证：Entitlements 文件必须签名
 * - 运行时强制：所有操作都检查 Entitlements
 */

#ifndef DOMES_ENTITLEMENTS_H
#define DOMES_ENTITLEMENTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief Entitlements 验证结果
 */
typedef enum {
    DOMES_ENT_OK = 0,               /**< 验证成功 */
    DOMES_ENT_INVALID = -1,         /**< 无效格式 */
    DOMES_ENT_SIGNATURE_INVALID = -2, /**< 签名无效 */
    DOMES_ENT_EXPIRED = -3,         /**< 已过期 */
    DOMES_ENT_DENIED = -4,          /**< 权限拒绝 */
    DOMES_ENT_NOT_FOUND = -5,       /**< 未找到 */
    DOMES_ENT_PARSE_ERROR = -6      /**< 解析错误 */
} domes_ent_result_t;

/**
 * @brief 文件系统权限
 */
typedef struct {
    char* path;                     /**< 路径模式 (支持通配符) */
    char** permissions;             /**< 权限列表 (read, write, create, delete, execute) */
    size_t perm_count;              /**< 权限数量 */
} domes_ent_fs_permission_t;

/**
 * @brief 网络权限
 */
typedef struct {
    char* host;                     /**< 主机名 (支持通配符) */
    uint16_t port;                  /**< 端口 (0 表示任意) */
    char* protocol;                 /**< 协议 (tcp, udp, http, https) */
    char* direction;                /**< 方向 (inbound, outbound, both) */
} domes_ent_net_permission_t;

/**
 * @brief IPC 权限
 */
typedef struct {
    char* target;                   /**< 目标服务 */
    char** permissions;             /**< 权限列表 (send, receive, call) */
    size_t perm_count;              /**< 权限数量 */
} domes_ent_ipc_permission_t;

/**
 * @brief 资源限制
 */
typedef struct {
    uint32_t max_cpu_percent;       /**< 最大 CPU 百分比 */
    uint32_t max_cpu_cores;         /**< 最大 CPU 核心数 */
    uint64_t max_memory_bytes;      /**< 最大内存 (字节) */
    uint64_t max_disk_bytes;        /**< 最大磁盘 (字节) */
    uint32_t max_processes;         /**< 最大进程数 */
    uint32_t max_threads;           /**< 最大线程数 */
    uint32_t max_open_files;        /**< 最大打开文件数 */
    uint32_t max_network_connections; /**< 最大网络连接数 */
} domes_ent_resource_limits_t;

/**
 * @brief 凭证访问权限
 */
typedef struct {
    char* cred_id;                  /**< 凭证标识 */
    char** permissions;             /**< 权限列表 (read, write, delete) */
    size_t perm_count;              /**< 权限数量 */
} domes_ent_vault_permission_t;

/**
 * @brief Entitlements 上下文
 */
typedef struct domes_entitlements domes_entitlements_t;

/**
 * @brief Entitlements 完整结构
 */
typedef struct {
    char* agent_id;                 /**< Agent 标识 */
    char* version;                  /**< 版本号 */
    uint64_t not_before;            /**< 有效期起始 */
    uint64_t not_after;             /**< 有效期截止 */
    
    domes_ent_fs_permission_t* fs_permissions;
    size_t fs_count;
    
    domes_ent_net_permission_t* net_permissions;
    size_t net_count;
    
    domes_ent_ipc_permission_t* ipc_permissions;
    size_t ipc_count;
    
    domes_ent_resource_limits_t resources;
    
    domes_ent_vault_permission_t* vault_permissions;
    size_t vault_count;
    
    char** allowed_syscalls;
    size_t syscall_count;
    
    char** allowed_capabilities;
    size_t cap_count;
} domes_entitlements_info_t;

/* ============================================================================
 * 生命周期管理
 * ============================================================================ */

/**
 * @brief 初始化 Entitlements 模块
 * @return 0 成功，非0 失败
 */
int domes_entitlements_init(void);

/**
 * @brief 清理 Entitlements 模块
 */
void domes_entitlements_cleanup(void);

/**
 * @brief 从 YAML 文件加载 Entitlements
 * @param yaml_path YAML 文件路径
 * @param entitlements 输出上下文
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_load(const char* yaml_path,
                             domes_entitlements_t** entitlements);

/**
 * @brief 从 JSON 文件加载 Entitlements
 * @param json_path JSON 文件路径
 * @param entitlements 输出上下文
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_load_json(const char* json_path,
                                  domes_entitlements_t** entitlements);

/**
 * @brief 从字符串加载 Entitlements
 * @param yaml_content YAML 内容字符串
 * @param entitlements 输出上下文
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_load_string(const char* yaml_content,
                                    domes_entitlements_t** entitlements);

/**
 * @brief 释放 Entitlements
 * @param entitlements Entitlements 上下文
 */
void domes_entitlements_free(domes_entitlements_t* entitlements);

/* ============================================================================
 * 签名验证
 * ============================================================================ */

/**
 * @brief 验证 Entitlements 签名
 * @param entitlements Entitlements 上下文
 * @param public_key 验证公钥 (PEM 格式)
 * @return DOMES_ENT_OK 有效，其他无效
 */
int domes_entitlements_verify(domes_entitlements_t* entitlements,
                               const char* public_key);

/**
 * @brief 对 Entitlements 签名
 * @param entitlements Entitlements 上下文
 * @param private_key 签名私钥 (PEM 格式)
 * @param signature_out 签名输出
 * @param sig_len 签名长度
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_sign(domes_entitlements_t* entitlements,
                             const char* private_key,
                             char* signature_out, size_t* sig_len);

/**
 * @brief 检查 Entitlements 是否已签名
 * @param entitlements Entitlements 上下文
 * @return true 已签名，false 未签名
 */
bool domes_entitlements_is_signed(domes_entitlements_t* entitlements);

/* ============================================================================
 * 权限检查
 * ============================================================================ */

/**
 * @brief 检查文件系统权限
 * @param entitlements Entitlements 上下文
 * @param path 文件路径
 * @param operation 操作类型 (read, write, create, delete, execute)
 * @return 1 允许，0 拒绝
 */
int domes_entitlements_check_fs(domes_entitlements_t* entitlements,
                                 const char* path,
                                 const char* operation);

/**
 * @brief 检查网络权限
 * @param entitlements Entitlements 上下文
 * @param host 目标主机
 * @param port 端口
 * @param protocol 协议
 * @param direction 方向 (inbound, outbound)
 * @return 1 允许，0 拒绝
 */
int domes_entitlements_check_net(domes_entitlements_t* entitlements,
                                  const char* host,
                                  uint16_t port,
                                  const char* protocol,
                                  const char* direction);

/**
 * @brief 检查 IPC 权限
 * @param entitlements Entitlements 上下文
 * @param target 目标服务
 * @param operation 操作类型
 * @return 1 允许，0 拒绝
 */
int domes_entitlements_check_ipc(domes_entitlements_t* entitlements,
                                  const char* target,
                                  const char* operation);

/**
 * @brief 检查系统调用权限
 * @param entitlements Entitlements 上下文
 * @param syscall_name 系统调用名称
 * @return 1 允许，0 拒绝
 */
int domes_entitlements_check_syscall(domes_entitlements_t* entitlements,
                                      const char* syscall_name);

/**
 * @brief 检查能力权限
 * @param entitlements Entitlements 上下文
 * @param capability 能力名称
 * @return 1 允许，0 拒绝
 */
int domes_entitlements_check_capability(domes_entitlements_t* entitlements,
                                         const char* capability);

/**
 * @brief 检查凭证访问权限
 * @param entitlements Entitlements 上下文
 * @param cred_id 凭证标识
 * @param operation 操作类型
 * @return 1 允许，0 拒绝
 */
int domes_entitlements_check_vault(domes_entitlements_t* entitlements,
                                    const char* cred_id,
                                    const char* operation);

/* ============================================================================
 * 资源限制
 * ============================================================================ */

/**
 * @brief 获取资源限制
 * @param entitlements Entitlements 上下文
 * @param limits 资源限制输出
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_get_resource_limits(domes_entitlements_t* entitlements,
                                            domes_ent_resource_limits_t* limits);

/**
 * @brief 检查资源使用是否超限
 * @param entitlements Entitlements 上下文
 * @param resource_type 资源类型 (cpu, memory, disk, process, thread, file, connection)
 * @param current_value 当前使用值
 * @return 1 未超限，0 超限
 */
int domes_entitlements_check_resource(domes_entitlements_t* entitlements,
                                       const char* resource_type,
                                       uint64_t current_value);

/* ============================================================================
 * 信息获取
 * ============================================================================ */

/**
 * @brief 获取 Entitlements 完整信息
 * @param entitlements Entitlements 上下文
 * @param info 信息输出
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_get_info(domes_entitlements_t* entitlements,
                                 domes_entitlements_info_t* info);

/**
 * @brief 释放 Entitlements 信息
 * @param info 信息指针
 */
void domes_entitlements_free_info(domes_entitlements_info_t* info);

/**
 * @brief 获取 Agent ID
 * @param entitlements Entitlements 上下文
 * @return Agent ID 字符串 (不要释放)
 */
const char* domes_entitlements_get_agent_id(domes_entitlements_t* entitlements);

/**
 * @brief 检查有效期
 * @param entitlements Entitlements 上下文
 * @return DOMES_ENT_OK 有效，其他无效
 */
int domes_entitlements_check_validity(domes_entitlements_t* entitlements);

/* ============================================================================
 * 导出
 * ============================================================================ */

/**
 * @brief 导出为 YAML 格式
 * @param entitlements Entitlements 上下文
 * @param yaml_out YAML 输出缓冲区
 * @param len 缓冲区大小/实际长度
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_export_yaml(domes_entitlements_t* entitlements,
                                    char* yaml_out, size_t* len);

/**
 * @brief 导出为 JSON 格式
 * @param entitlements Entitlements 上下文
 * @param json_out JSON 输出缓冲区
 * @param len 缓冲区大小/实际长度
 * @return DOMES_ENT_OK 成功，其他失败
 */
int domes_entitlements_export_json(domes_entitlements_t* entitlements,
                                    char* json_out, size_t* len);

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief 获取错误描述
 * @param result 错误码
 * @return 错误描述字符串
 */
const char* domes_entitlements_result_string(domes_ent_result_t result);

/**
 * @brief 匹配路径模式
 * @param pattern 模式 (支持 * 和 ?)
 * @param path 实际路径
 * @return 1 匹配，0 不匹配
 */
int domes_entitlements_match_path(const char* pattern, const char* path);

/**
 * @brief 匹配主机模式
 * @param pattern 模式 (支持 * 前缀通配)
 * @param host 实际主机
 * @return 1 匹配，0 不匹配
 */
int domes_entitlements_match_host(const char* pattern, const char* host);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_ENTITLEMENTS_H */
