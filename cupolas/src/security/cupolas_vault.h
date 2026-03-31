/**
 * @file cupolas_vault.h
 * @brief 安全凭证存储 - 类似 iOS Keychain 的安全存储
 * @author Spharx
 * @date 2026
 *
 * 设计原则：
 * - 加密存储：所有凭证使用 AES-256-GCM 加密
 * - 访问控制：基于 Agent ID 的细粒度权限控制
 * - 审计追踪：所有访问操作记录审计日志
 * - 防篡改：完整性校验防止数据篡改
 */

#ifndef cupolas_VAULT_H
#define cupolas_VAULT_H

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
 * @brief 凭证类型
 */
typedef enum {
    cupolas_VAULT_CRED_PASSWORD = 1,     /**< 密码 */
    cupolas_VAULT_CRED_TOKEN = 2,        /**< 令牌 (API Key, OAuth Token) */
    cupolas_VAULT_CRED_KEY = 3,          /**< 密钥 (私钥) */
    cupolas_VAULT_CRED_CERTIFICATE = 4,  /**< 证书 */
    cupolas_VAULT_CRED_SECRET = 5,       /**< 通用秘密 */
    cupolas_VAULT_CRED_NOTE = 6          /**< 安全笔记 */
} cupolas_vault_cred_type_t;

/**
 * @brief 访问操作类型
 */
typedef enum {
    cupolas_VAULT_OP_READ = 1,
    cupolas_VAULT_OP_WRITE = 2,
    cupolas_VAULT_OP_DELETE = 4,
    cupolas_VAULT_OP_EXPORT = 8
} cupolas_vault_operation_t;

/**
 * @brief 凭证元数据
 */
typedef struct {
    char* cred_id;                      /**< 凭证标识 */
    cupolas_vault_cred_type_t type;       /**< 凭证类型 */
    char* description;                  /**< 描述 */
    char* service;                      /**< 关联服务 */
    char* account;                      /**< 关联账户 */
    uint64_t created_at;                /**< 创建时间 */
    uint64_t updated_at;                /**< 更新时间 */
    uint64_t expires_at;                /**< 过期时间 */
    bool is_accessible;                 /**< 是否可访问 */
} cupolas_vault_metadata_t;

/**
 * @brief 访问控制条目
 */
typedef struct {
    char* agent_id;                     /**< Agent ID */
    uint32_t operations;                /**< 允许的操作 (位掩码) */
    uint64_t expires_at;                /**< 访问过期时间 */
    uint32_t access_count;              /**< 访问次数 */
    uint32_t max_access_count;          /**< 最大访问次数 */
} cupolas_vault_acl_entry_t;

/**
 * @brief 访问控制列表
 */
typedef struct {
    cupolas_vault_acl_entry_t* entries;
    size_t count;
} cupolas_vault_acl_t;

/**
 * @brief Vault 配置
 */
typedef struct {
    const char* storage_path;           /**< 存储路径 */
    const char* master_key_path;        /**< 主密钥路径 */
    bool enable_audit;                  /**< 启用审计 */
    bool enable_auto_lock;              /**< 启用自动锁定 */
    uint32_t auto_lock_seconds;         /**< 自动锁定时间 */
    uint32_t max_retry_count;           /**< 最大重试次数 */
} cupolas_vault_config_t;

/**
 * @brief Vault 句柄
 */
typedef struct cupolas_vault cupolas_vault_t;

/* ============================================================================
 * Vault 生命周期
 * ============================================================================ */

/**
 * @brief 初始化 Vault 模块
 * @param manager 配置参数 (NULL 使用默认配置)
 * @return 0 成功，非0 失败
 */
int cupolas_vault_init(const cupolas_vault_config_t* manager);

/**
 * @brief 清理 Vault 模块
 */
void cupolas_vault_cleanup(void);

/**
 * @brief 打开 Vault
 * @param vault_id Vault 标识
 * @param password 解锁密码 (可选)
 * @param vault Vault 句柄输出
 * @return 0 成功，非0 失败
 */
int cupolas_vault_open(const char* vault_id, const char* password, cupolas_vault_t** vault);

/**
 * @brief 关闭 Vault
 * @param vault Vault 句柄
 */
void cupolas_vault_close(cupolas_vault_t* vault);

/**
 * @brief 锁定 Vault
 * @param vault Vault 句柄
 * @return 0 成功，非0 失败
 */
int cupolas_vault_lock(cupolas_vault_t* vault);

/**
 * @brief 解锁 Vault
 * @param vault Vault 句柄
 * @param password 解锁密码
 * @return 0 成功，非0 失败
 */
int cupolas_vault_unlock(cupolas_vault_t* vault, const char* password);

/**
 * @brief 检查 Vault 是否锁定
 * @param vault Vault 句柄
 * @return true 锁定，false 未锁定
 */
bool cupolas_vault_is_locked(cupolas_vault_t* vault);

/* ============================================================================
 * 凭证操作
 * ============================================================================ */

/**
 * @brief 存储凭证
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param type 凭证类型
 * @param data 凭证数据
 * @param data_len 数据长度
 * @param acl 访问控制列表 (可选)
 * @return 0 成功，非0 失败
 */
int cupolas_vault_store(cupolas_vault_t* vault,
                      const char* cred_id,
                      cupolas_vault_cred_type_t type,
                      const uint8_t* data, size_t data_len,
                      const cupolas_vault_acl_t* acl);

/**
 * @brief 检索凭证
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param agent_id 请求者 Agent ID
 * @param data_out 数据输出缓冲区
 * @param data_len 缓冲区大小/实际长度
 * @return 0 成功，非0 失败
 */
int cupolas_vault_retrieve(cupolas_vault_t* vault,
                         const char* cred_id,
                         const char* agent_id,
                         uint8_t* data_out, size_t* data_len);

/**
 * @brief 删除凭证
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param agent_id 请求者 Agent ID
 * @return 0 成功，非0 失败
 */
int cupolas_vault_delete(cupolas_vault_t* vault,
                       const char* cred_id,
                       const char* agent_id);

/**
 * @brief 检查凭证是否存在
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @return true 存在，false 不存在
 */
bool cupolas_vault_exists(cupolas_vault_t* vault, const char* cred_id);

/**
 * @brief 更新凭证
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param data 新数据
 * @param data_len 数据长度
 * @param agent_id 请求者 Agent ID
 * @return 0 成功，非0 失败
 */
int cupolas_vault_update(cupolas_vault_t* vault,
                       const char* cred_id,
                       const uint8_t* data, size_t data_len,
                       const char* agent_id);

/* ============================================================================
 * 元数据操作
 * ============================================================================ */

/**
 * @brief 获取凭证元数据
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param metadata 元数据输出
 * @return 0 成功，非0 失败
 */
int cupolas_vault_get_metadata(cupolas_vault_t* vault,
                              const char* cred_id,
                              cupolas_vault_metadata_t* metadata);

/**
 * @brief 释放元数据
 * @param metadata 元数据指针
 */
void cupolas_vault_free_metadata(cupolas_vault_metadata_t* metadata);

/**
 * @brief 列出所有凭证
 * @param vault Vault 句柄
 * @param type 凭证类型过滤 (0 表示所有类型)
 * @param metadata_array 元数据数组输出
 * @param count 数量输出
 * @return 0 成功，非0 失败
 */
int cupolas_vault_list(cupolas_vault_t* vault,
                     cupolas_vault_cred_type_t type,
                     cupolas_vault_metadata_t** metadata_array,
                     size_t* count);

/**
 * @brief 释放凭证列表
 * @param metadata_array 元数据数组
 * @param count 数量
 */
void cupolas_vault_free_list(cupolas_vault_metadata_t* metadata_array, size_t count);

/* ============================================================================
 * 访问控制
 * ============================================================================ */

/**
 * @brief 检查访问权限
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param agent_id Agent ID
 * @param operation 操作类型
 * @return true 允许，false 拒绝
 */
bool cupolas_vault_check_access(cupolas_vault_t* vault,
                               const char* cred_id,
                               const char* agent_id,
                               cupolas_vault_operation_t operation);

/**
 * @brief 授予访问权限
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param agent_id Agent ID
 * @param operations 允许的操作 (位掩码)
 * @param expires_at 过期时间 (0 表示永不过期)
 * @return 0 成功，非0 失败
 */
int cupolas_vault_grant_access(cupolas_vault_t* vault,
                              const char* cred_id,
                              const char* agent_id,
                              uint32_t operations,
                              uint64_t expires_at);

/**
 * @brief 撤销访问权限
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param agent_id Agent ID
 * @return 0 成功，非0 失败
 */
int cupolas_vault_revoke_access(cupolas_vault_t* vault,
                               const char* cred_id,
                               const char* agent_id);

/**
 * @brief 获取访问控制列表
 * @param vault Vault 句柄
 * @param cred_id 凭证标识
 * @param acl ACL 输出
 * @return 0 成功，非0 失败
 */
int cupolas_vault_get_acl(cupolas_vault_t* vault,
                        const char* cred_id,
                        cupolas_vault_acl_t* acl);

/**
 * @brief 释放 ACL
 * @param acl ACL 指针
 */
void cupolas_vault_free_acl(cupolas_vault_acl_t* acl);

/* ============================================================================
 * 批量操作
 * ============================================================================ */

/**
 * @brief 导出凭证
 * @param vault Vault 句柄
 * @param export_path 导出路径
 * @param password 导出密码
 * @param agent_id 请求者 Agent ID
 * @return 0 成功，非0 失败
 */
int cupolas_vault_export(cupolas_vault_t* vault,
                        const char* export_path,
                        const char* password,
                        const char* agent_id);

/**
 * @brief 导入凭证
 * @param vault Vault 句柄
 * @param import_path 导入路径
 * @param password 导入密码
 * @param agent_id 请求者 Agent ID
 * @return 0 成功，非0 失败
 */
int cupolas_vault_import(cupolas_vault_t* vault,
                        const char* import_path,
                        const char* password,
                        const char* agent_id);

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief 获取凭证类型名称
 * @param type 凭证类型
 * @return 类型名称字符串
 */
const char* cupolas_vault_cred_type_string(cupolas_vault_cred_type_t type);

/**
 * @brief 获取操作名称
 * @param op 操作类型
 * @return 操作名称字符串
 */
const char* cupolas_vault_operation_string(cupolas_vault_operation_t op);

/**
 * @brief 生成随机密码
 * @param password_out 密码输出缓冲区
 * @param length 密码长度
 * @return 0 成功，非0 失败
 */
int cupolas_vault_generate_password(char* password_out, size_t length);

/**
 * @brief 生成密钥对
 * @param public_key_out 公钥输出缓冲区
 * @param pub_len 缓冲区大小/实际长度
 * @param private_key_out 私钥输出缓冲区
 * @param priv_len 缓冲区大小/实际长度
 * @return 0 成功，非0 失败
 */
int cupolas_vault_generate_keypair(char* public_key_out, size_t* pub_len,
                                  char* private_key_out, size_t* priv_len);

#ifdef __cplusplus
}
#endif

#endif /* cupolas_VAULT_H */
