/**
 * @file cupolas_signature.h
 * @brief 代码签名验证 - 确保 Agent 代码的完整性和可信性
 * @author Spharx
 * @date 2026
 *
 * 设计原则：
 * - 零信任：所有代码必须验证签名
 * - 多算法支持：RSA、ECDSA、Ed25519
 * - 证书链验证：完整的信任链检查
 * - 防篡改：运行时完整性校验
 */

#ifndef cupolas_SIGNATURE_H
#define cupolas_SIGNATURE_H

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
 * @brief 签名验证结果
 */
typedef enum {
    cupolas_SIG_OK = 0,               /**< 签名有效 */
    cupolas_SIG_INVALID = -1,         /**< 签名无效 */
    cupolas_SIG_EXPIRED = -2,         /**< 签名过期 */
    cupolas_SIG_REVOKED = -3,         /**< 签名吊销 */
    cupolas_SIG_UNTRUSTED = -4,       /**< 不可信签名者 */
    cupolas_SIG_TAMPERED = -5,        /**< 代码被篡改 */
    cupolas_SIG_NO_SIGNATURE = -6,    /**< 无签名 */
    cupolas_SIG_CERT_INVALID = -7,    /**< 证书无效 */
    cupolas_SIG_CERT_EXPIRED = -8,    /**< 证书过期 */
    cupolas_SIG_ALGO_UNSUPPORTED = -9 /**< 算法不支持 */
} cupolas_sig_result_t;

/**
 * @brief 签名算法类型
 */
typedef enum {
    cupolas_SIG_ALGO_RSA_SHA256 = 1,  /**< RSA with SHA-256 */
    cupolas_SIG_ALGO_RSA_SHA384 = 2,  /**< RSA with SHA-384 */
    cupolas_SIG_ALGO_RSA_SHA512 = 3,  /**< RSA with SHA-512 */
    cupolas_SIG_ALGO_ECDSA_P256 = 4,  /**< ECDSA P-256 */
    cupolas_SIG_ALGO_ECDSA_P384 = 5,  /**< ECDSA P-384 */
    cupolas_SIG_ALGO_ED25519 = 6      /**< Ed25519 */
} cupolas_sig_algo_t;

/**
 * @brief 签名者信息
 */
typedef struct {
    char* subject_cn;               /**< 通用名称 */
    char* subject_org;              /**< 组织 */
    char* subject_ou;               /**< 组织单位 */
    char* issuer_cn;                /**< 颁发者 CN */
    char* serial_number;            /**< 序列号 */
    uint64_t not_before;            /**< 有效期起始 */
    uint64_t not_after;             /**< 有效期截止 */
    bool is_ca;                     /**< 是否为 CA */
    uint32_t key_usage;             /**< 密钥用途 */
} cupolas_signer_info_t;

/**
 * @brief 代码签名上下文
 */
typedef struct cupolas_signature cupolas_signature_t;

/**
 * @brief 签名验证配置
 */
typedef struct {
    bool check_cert_chain;          /**< 检查证书链 */
    bool check_revocation;          /**< 检查吊销状态 */
    bool check_timestamp;           /**< 检查时间戳 */
    bool allow_self_signed;         /**< 允许自签名 */
    bool allow_expired_test;        /**< 允许过期测试证书 */
    const char* trusted_ca_path;    /**< 受信任 CA 路径 */
    const char* crl_path;           /**< CRL 路径 */
    uint32_t max_chain_depth;       /**< 最大证书链深度 */
} cupolas_sig_config_t;

/* ============================================================================
 * 核心函数
 * ============================================================================ */

/**
 * @brief 初始化签名验证模块
 * @param manager 配置参数 (NULL 使用默认配置)
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_init(const cupolas_sig_config_t* manager);

/**
 * @brief 清理签名验证模块
 */
void cupolas_signature_cleanup(void);

/**
 * @brief 验证文件签名
 * @param file_path 文件路径
 * @param expected_signer 预期签名者 CN (可选，NULL 不检查)
 * @param result 验证结果输出
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_verify_file(const char* file_path,
                                 const char* expected_signer,
                                 cupolas_sig_result_t* result);

/**
 * @brief 验证内存数据签名
 * @param data 数据指针
 * @param data_len 数据长度
 * @param signature 签名数据
 * @param sig_len 签名长度
 * @param algo 签名算法
 * @param public_key 公钥 (PEM 格式)
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_verify_data(const uint8_t* data, size_t data_len,
                                 const uint8_t* signature, size_t sig_len,
                                 cupolas_sig_algo_t algo,
                                 const char* public_key);

/**
 * @brief 验证代码完整性
 * @param file_path 文件路径
 * @param expected_hash 预期哈希值 (SHA-256, 32字节)
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_verify_integrity(const char* file_path,
                                      const uint8_t* expected_hash);

/**
 * @brief 计算文件哈希
 * @param file_path 文件路径
 * @param hash_out 哈希输出缓冲区 (32字节)
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_compute_hash(const char* file_path, uint8_t* hash_out);

/* ============================================================================
 * 签名者信息
 * ============================================================================ */

/**
 * @brief 获取签名者信息
 * @param file_path 文件路径
 * @param info 签名者信息输出
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_get_signer_info(const char* file_path,
                                     cupolas_signer_info_t* info);

/**
 * @brief 释放签名者信息
 * @param info 签名者信息
 */
void cupolas_signature_free_signer_info(cupolas_signer_info_t* info);

/**
 * @brief 检查签名者是否受信任
 * @param signer_cn 签名者 CN
 * @return true 受信任，false 不受信任
 */
bool cupolas_signature_is_trusted_signer(const char* signer_cn);

/**
 * @brief 添加受信任签名者
 * @param signer_cn 签名者 CN
 * @param public_key 公钥 (PEM 格式)
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_add_trusted_signer(const char* signer_cn,
                                        const char* public_key);

/* ============================================================================
 * 签名操作
 * ============================================================================ */

/**
 * @brief 对文件签名
 * @param file_path 文件路径
 * @param private_key 私钥 (PEM 格式)
 * @param algo 签名算法
 * @param signature_out 签名输出缓冲区
 * @param sig_len 缓冲区大小/实际长度
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_sign_file(const char* file_path,
                               const char* private_key,
                               cupolas_sig_algo_t algo,
                               uint8_t* signature_out,
                               size_t* sig_len);

/**
 * @brief 对数据签名
 * @param data 数据指针
 * @param data_len 数据长度
 * @param private_key 私钥 (PEM 格式)
 * @param algo 签名算法
 * @param signature_out 签名输出缓冲区
 * @param sig_len 缓冲区大小/实际长度
 * @return cupolas_SIG_OK 成功，其他失败
 */
int cupolas_signature_sign_data(const uint8_t* data, size_t data_len,
                               const char* private_key,
                               cupolas_sig_algo_t algo,
                               uint8_t* signature_out,
                               size_t* sig_len);

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief 获取错误描述
 * @param result 错误码
 * @return 错误描述字符串
 */
const char* cupolas_signature_result_string(cupolas_sig_result_t result);

/**
 * @brief 获取算法名称
 * @param algo 算法类型
 * @return 算法名称字符串
 */
const char* cupolas_signature_algo_string(cupolas_sig_algo_t algo);

/**
 * @brief 获取当前时间戳
 * @return Unix 时间戳 (秒)
 */
uint64_t cupolas_signature_get_timestamp(void);

/**
 * @brief 检查证书有效期
 * @param not_before 有效期起始
 * @param not_after 有效期截止
 * @return cupolas_SIG_OK 有效，其他无效
 */
int cupolas_signature_check_validity(uint64_t not_before, uint64_t not_after);

#ifdef __cplusplus
}
#endif

#endif /* cupolas_SIGNATURE_H */
