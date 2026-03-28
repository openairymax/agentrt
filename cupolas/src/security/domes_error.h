/**
 * @file domes_error.h
 * @brief 统一错误码定义与转换函数
 * @author Spharx
 * @date 2026-03-26
 *
 * 设计原则：
 * - 向后兼容：保持现有 API 契约不变
 * - 精确诊断：模块特定错误码保留详细诊断信息
 * - 统一接口：提供标准转换函数用于公共 API 层
 */

#ifndef DOMES_ERROR_H
#define DOMES_ERROR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 统一错误码（兼容 AgentOS 标准）
 * ============================================================================ */

/**
 * @brief 统一错误码定义
 *
 * 用于公共 API 层，与 AgentOS 标准错误码保持一致
 */
typedef enum {
    DOMES_ERR_OK                 =   0,
    DOMES_ERR_UNKNOWN            =  -1,
    DOMES_ERR_INVALID_PARAM      =  -2,
    DOMES_ERR_NULL_POINTER       =  -3,
    DOMES_ERR_OUT_OF_MEMORY      =  -4,
    DOMES_ERR_BUFFER_TOO_SMALL   =  -5,
    DOMES_ERR_NOT_FOUND          =  -6,
    DOMES_ERR_ALREADY_EXISTS     =  -7,
    DOMES_ERR_TIMEOUT            =  -8,
    DOMES_ERR_NOT_SUPPORTED      =  -9,
    DOMES_ERR_PERMISSION_DENIED  = -10,
    DOMES_ERR_IO                = -11,
    DOMES_ERR_STATE_ERROR        = -13,
    DOMES_ERR_OVERFLOW          = -14,
    DOMES_ERR_TRY_AGAIN         = -15,
    DOMES_ERR_AUTH_FAILED       = -16,
    DOMES_ERR_CERT_INVALID       = -17,
    DOMES_ERR_CERT_EXPIRED       = -18,
    DOMES_ERR_SIGNATURE_INVALID  = -19,
    DOMES_ERR_TAMPERED          = -20
} domes_error_t;

/* ============================================================================
 * 模块特定错误码转换目标
 * ============================================================================ */

/**
 * @brief 代码签名错误码
 */
typedef enum {
    DOMES_SIG_ERR_OK             =   0,
    DOMES_SIG_ERR_INVALID        =  -1,
    DOMES_SIG_ERR_EXPIRED        =  -2,
    DOMES_SIG_ERR_REVOKED        =  -3,
    DOMES_SIG_ERR_UNTRUSTED      =  -4,
    DOMES_SIG_ERR_TAMPERED       =  -5,
    DOMES_SIG_ERR_NO_SIGNATURE   =  -6,
    DOMES_SIG_ERR_CERT_INVALID   =  -7,
    DOMES_SIG_ERR_CERT_EXPIRED   =  -8,
    DOMES_SIG_ERR_ALGO_UNSUPPORTED = -9
} domes_sig_error_t;

/**
 * @brief Entitlements 错误码
 */
typedef enum {
    DOMES_ENT_ERR_OK             =   0,
    DOMES_ENT_ERR_INVALID        =  -1,
    DOMES_ENT_ERR_SIGNATURE_INVALID = -2,
    DOMES_ENT_ERR_EXPIRED        =  -3,
    DOMES_ENT_ERR_DENIED         =  -4,
    DOMES_ENT_ERR_NOT_FOUND      =  -5,
    DOMES_ENT_ERR_PARSE_ERROR    =  -6
} domes_ent_error_t;

/**
 * @brief Vault 错误码
 */
typedef enum {
    DOMES_VAULT_ERR_OK           =   0,
    DOMES_VAULT_ERR_INVALID       =  -1,
    DOMES_VAULT_ERR_LOCKED       =  -2,
    DOMES_VAULT_ERR_ACCESS_DENIED = -3,
    DOMES_VAULT_ERR_NOT_FOUND    =  -4,
    DOMES_VAULT_ERR_ALREADY_EXISTS = -5,
    DOMES_VAULT_ERR_CORRUPT      =  -6,
    DOMES_VAULT_ERR_DECRYPT_FAILED = -7,
    DOMES_VAULT_ERR_ENCRYPT_FAILED = -8
} domes_vault_error_t;

/**
 * @brief 网络安全错误码
 */
typedef enum {
    DOMES_NET_ERR_OK              =   0,
    DOMES_NET_ERR_INVALID         =  -1,
    DOMES_NET_ERR_CERT_INVALID   =  -2,
    DOMES_NET_ERR_CERT_EXPIRED   =  -3,
    DOMES_NET_ERR_CERT_REVOKED   =  -4,
    DOMES_NET_ERR_UNTRUSTED      =  -5,
    DOMES_NET_ERR_HOST_MISMATCH  =  -6,
    DOMES_NET_ERR_DENIED         =  -7,
    DOMES_NET_ERR_TIMEOUT         =  -8
} domes_net_error_t;

/**
 * @brief 运行时保护错误码
 */
typedef enum {
    DOMES_RUNTIME_ERR_OK         =   0,
    DOMES_RUNTIME_ERR_INVALID    =  -1,
    DOMES_RUNTIME_ERR_VIOLATION  =  -2,
    DOMES_RUNTIME_ERR_COMPROMISED = -3,
    DOMES_RUNTIME_ERR_NOT_SUPPORTED = -4
} domes_runtime_error_t;

/* ============================================================================
 * 错误码转换函数
 * ============================================================================ */

/**
 * @brief 将统一错误码转换为字符串描述
 * @param error 统一错误码
 * @return 错误描述字符串
 */
const char* domes_error_string(domes_error_t error);

/**
 * @brief 将签名错误码转换为统一错误码
 * @param sig_error 签名错误码
 * @return 统一错误码
 */
domes_error_t domes_error_from_sig(domes_sig_error_t sig_error);

/**
 * @brief 将 Entitlements 错误码转换为统一错误码
 * @param ent_error Entitlements 错误码
 * @return 统一错误码
 */
domes_error_t domes_error_from_ent(domes_ent_error_t ent_error);

/**
 * @brief 将 Vault 错误码转换为统一错误码
 * @param vault_error Vault 错误码
 * @return 统一错误码
 */
domes_error_t domes_error_from_vault(domes_vault_error_t vault_error);

/**
 * @brief 将网络安全错误码转换为统一错误码
 * @param net_error 网络安全错误码
 * @return 统一错误码
 */
domes_error_t domes_error_from_net(domes_net_error_t net_error);

/**
 * @brief 将运行时保护错误码转换为统一错误码
 * @param runtime_error 运行时保护错误码
 * @return 统一错误码
 */
domes_error_t domes_error_from_runtime(domes_runtime_error_t runtime_error);

/**
 * @brief 将统一错误码转换为签名错误码
 * @param error 统一错误码
 * @return 签名错误码
 */
domes_sig_error_t domes_error_to_sig(domes_error_t error);

/**
 * @brief 将统一错误码转换为 Entitlements 错误码
 * @param error 统一错误码
 * @return Entitlements 错误码
 */
domes_ent_error_t domes_error_to_ent(domes_error_t error);

/**
 * @brief 将统一错误码转换为 Vault 错误码
 * @param error 统一错误码
 * @return Vault 错误码
 */
domes_vault_error_t domes_error_to_vault(domes_error_t error);

/**
 * @brief 将统一错误码转换为网络安全错误码
 * @param error 统一错误码
 * @return 网络安全错误码
 */
domes_net_error_t domes_error_to_net(domes_error_t error);

/**
 * @brief 将统一错误码转换为运行时保护错误码
 * @param error 统一错误码
 * @return 运行时保护错误码
 */
domes_runtime_error_t domes_error_to_runtime(domes_error_t error);

/* ============================================================================
 * 错误码判断工具宏
 * ============================================================================ */

/**
 * @brief 判断是否为成功状态
 */
#define DOMES_ERROR_IS_SUCCESS(e)   ((e) == DOMES_ERR_OK)

/**
 * @brief 判断是否为致命错误
 */
#define DOMES_ERROR_IS_FATAL(e)     ((e) <  DOMES_ERR_OUT_OF_MEMORY)

/**
 * @brief 判断是否为参数错误
 */
#define DOMES_ERROR_IS_PARAM(e)     ((e) == DOMES_ERR_INVALID_PARAM || \
                                    (e) == DOMES_ERR_NULL_POINTER)

/* ============================================================================
 * Backward Compatibility Aliases
 * ============================================================================ */

/* 保持向后兼容的别名 */
#define AGENTOS_OK                    DOMES_ERR_OK
#define AGENTOS_ERR_UNKNOWN          DOMES_ERR_UNKNOWN
#define AGENTOS_ERR_INVALID_PARAM   DOMES_ERR_INVALID_PARAM
#define AGENTOS_ERR_NULL_POINTER    DOMES_ERR_NULL_POINTER
#define AGENTOS_ERR_OUT_OF_MEMORY   DOMES_ERR_OUT_OF_MEMORY
#define AGENTOS_ERR_BUFFER_TOO_SMALL DOMES_ERR_BUFFER_TOO_SMALL
#define AGENTOS_ERR_NOT_FOUND        DOMES_ERR_NOT_FOUND
#define AGENTOS_ERR_ALREADY_EXISTS   DOMES_ERR_ALREADY_EXISTS
#define AGENTOS_ERR_TIMEOUT          DOMES_ERR_TIMEOUT
#define AGENTOS_ERR_NOT_SUPPORTED    DOMES_ERR_NOT_SUPPORTED
#define AGENTOS_ERR_PERMISSION_DENIED DOMES_ERR_PERMISSION_DENIED
#define AGENTOS_ERR_IO              DOMES_ERR_IO
#define AGENTOS_ERR_STATE_ERROR     DOMES_ERR_STATE_ERROR
#define AGENTOS_ERR_OVERFLOW        DOMES_ERR_OVERFLOW

#define DOMES_OK                    DOMES_ERR_OK
#define DOMES_ERROR_UNKNOWN         DOMES_ERR_UNKNOWN
#define DOMES_ERROR_INVALID_ARG     DOMES_ERR_INVALID_PARAM
#define DOMES_ERROR_NO_MEMORY       DOMES_ERR_OUT_OF_MEMORY
#define DOMES_ERROR_NOT_FOUND       DOMES_ERR_NOT_FOUND
#define DOMES_ERROR_PERMISSION      DOMES_ERR_PERMISSION_DENIED
#define DOMES_ERROR_BUSY            DOMES_ERR_STATE_ERROR
#define DOMES_ERROR_TIMEOUT         DOMES_ERR_TIMEOUT
#define DOMES_ERROR_WOULD_BLOCK     DOMES_ERR_TRY_AGAIN
#define DOMES_ERROR_OVERFLOW         DOMES_ERR_OVERFLOW
#define DOMES_ERROR_NOT_SUPPORTED   DOMES_ERR_NOT_SUPPORTED
#define DOMES_ERROR_IO             DOMES_ERR_IO

#ifdef __cplusplus
}
#endif

#endif /* DOMES_ERROR_H */
