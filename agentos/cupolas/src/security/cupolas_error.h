/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_error.h - Unified Error Code Definitions and Conversion Functions
 *
 * Design Principles:
 * - Backward Compatibility: Preserve existing API contracts unchanged
 * - Precise Diagnostics: Module-specific error codes retain detailed diagnostic info
 * - Unified Interface: Standard conversion functions for public API layer
 */

#ifndef cupolas_ERROR_H
#define cupolas_ERROR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Unified Error Codes (Compatible with AgentOS Standard)
 * ============================================================================ */

/**
 * @brief Unified Error Codes
 *
 * Used in public API layer, compatible with AgentOS standard error codes.
 * Error codes follow these conventions:
 * - Success: 0
 * - Invalid parameters: -1 to -99
 * - Security errors: -100 to -199
 * - Module-specific errors: -200 to -299
 */
typedef enum {
    cupolas_ERR_OK                 =   0,
    cupolas_ERR_UNKNOWN            =  -1,
    cupolas_ERR_INVALID_PARAM      =  -2,
    cupolas_ERR_NULL_POINTER       =  -3,
    cupolas_ERR_OUT_OF_MEMORY      =  -4,
    cupolas_ERR_BUFFER_TOO_SMALL   =  -5,
    cupolas_ERR_NOT_FOUND          =  -6,
    cupolas_ERR_ALREADY_EXISTS     =  -7,
    cupolas_ERR_TIMEOUT            =  -8,
    cupolas_ERR_NOT_SUPPORTED      =  -9,
    cupolas_ERR_PERMISSION_DENIED  = -10,
    cupolas_ERR_IO                = -11,
    cupolas_ERR_STATE_ERROR        = -13,
    cupolas_ERR_OVERFLOW          = -14,
    cupolas_ERR_TRY_AGAIN         = -15,
    cupolas_ERR_AUTH_FAILED       = -16,
    cupolas_ERR_CERT_INVALID       = -17,
    cupolas_ERR_CERT_EXPIRED       = -18,
    cupolas_ERR_SIGNATURE_INVALID  = -19,
    cupolas_ERR_TAMPERED          = -20
} cupolas_error_t;

/* ============================================================================
 * Module-Specific Error Code Enumerations
 * ============================================================================ */

/**
 * @brief Code Signature Error Codes
 *
 * Error codes for code signature verification module.
 */
typedef enum {
    cupolas_SIG_ERR_OK             =   0,
    cupolas_SIG_ERR_INVALID        =  -1,
    cupolas_SIG_ERR_EXPIRED        =  -2,
    cupolas_SIG_ERR_REVOKED        =  -3,
    cupolas_SIG_ERR_UNTRUSTED      =  -4,
    cupolas_SIG_ERR_TAMPERED       =  -5,
    cupolas_SIG_ERR_NO_SIGNATURE   =  -6,
    cupolas_SIG_ERR_CERT_INVALID   =  -7,
    cupolas_SIG_ERR_CERT_EXPIRED   =  -8,
    cupolas_SIG_ERR_ALGO_UNSUPPORTED = -9
} cupolas_sig_error_t;

/**
 * @brief Entitlements Error Codes
 *
 * Error codes for entitlements permission declaration module.
 */
typedef enum {
    cupolas_ENT_ERR_OK             =   0,
    cupolas_ENT_ERR_INVALID        =  -1,
    cupolas_ENT_ERR_SIGNATURE_INVALID = -2,
    cupolas_ENT_ERR_EXPIRED        =  -3,
    cupolas_ENT_ERR_DENIED         =  -4,
    cupolas_ENT_ERR_NOT_FOUND      =  -5,
    cupolas_ENT_ERR_PARSE_ERROR    =  -6
} cupolas_ent_error_t;

/**
 * @brief Vault Error Codes
 *
 * Error codes for secure credential storage module.
 */
typedef enum {
    cupolas_VAULT_ERR_OK           =   0,
    cupolas_VAULT_ERR_INVALID       =  -1,
    cupolas_VAULT_ERR_LOCKED       =  -2,
    cupolas_VAULT_ERR_ACCESS_DENIED = -3,
    cupolas_VAULT_ERR_NOT_FOUND    =  -4,
    cupolas_VAULT_ERR_ALREADY_EXISTS = -5,
    cupolas_VAULT_ERR_CORRUPT      =  -6,
    cupolas_VAULT_ERR_DECRYPT_FAILED = -7,
    cupolas_VAULT_ERR_ENCRYPT_FAILED = -8
} cupolas_vault_error_t;

/**
 * @brief Network Security Error Codes
 *
 * Error codes for network security module (TLS, firewall).
 */
typedef enum {
    cupolas_NET_ERR_OK              =   0,
    cupolas_NET_ERR_INVALID         =  -1,
    cupolas_NET_ERR_CERT_INVALID   =  -2,
    cupolas_NET_ERR_CERT_EXPIRED   =  -3,
    cupolas_NET_ERR_CERT_REVOKED   =  -4,
    cupolas_NET_ERR_UNTRUSTED      =  -5,
    cupolas_NET_ERR_HOST_MISMATCH  =  -6,
    cupolas_NET_ERR_DENIED         =  -7,
    cupolas_NET_ERR_TIMEOUT         =  -8
} cupolas_net_error_t;

/**
 * @brief Runtime Protection Error Codes
 *
 * Error codes for runtime protection module (seccomp, CFI).
 */
typedef enum {
    cupolas_RUNTIME_ERR_OK         =   0,
    cupolas_RUNTIME_ERR_INVALID    =  -1,
    cupolas_RUNTIME_ERR_VIOLATION  =  -2,
    cupolas_RUNTIME_ERR_COMPROMISED = -3,
    cupolas_RUNTIME_ERR_NOT_SUPPORTED = -4
} cupolas_runtime_error_t;

/* ============================================================================
 * Error Code Conversion Functions
 * ============================================================================ */

/**
 * @brief Convert unified error code to string description
 * @param[in] error Unified error code
 * @return Error description string (static, do not free)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
const char* cupolas_error_string(cupolas_error_t error);

/**
 * @brief Convert signature error to unified error
 * @param[in] sig_error Signature error code
 * @return Unified error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_error_t cupolas_error_from_sig(cupolas_sig_error_t sig_error);

/**
 * @brief Convert entitlements error to unified error
 * @param[in] ent_error Entitlements error code
 * @return Unified error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_error_t cupolas_error_from_ent(cupolas_ent_error_t ent_error);

/**
 * @brief Convert vault error to unified error
 * @param[in] vault_error Vault error code
 * @return Unified error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_error_t cupolas_error_from_vault(cupolas_vault_error_t vault_error);

/**
 * @brief Convert network security error to unified error
 * @param[in] net_error Network security error code
 * @return Unified error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_error_t cupolas_error_from_net(cupolas_net_error_t net_error);

/**
 * @brief Convert runtime protection error to unified error
 * @param[in] runtime_error Runtime protection error code
 * @return Unified error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_error_t cupolas_error_from_runtime(cupolas_runtime_error_t runtime_error);

/**
 * @brief Convert unified error to signature error
 * @param[in] error Unified error code
 * @return Signature error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_sig_error_t cupolas_error_to_sig(cupolas_error_t error);

/**
 * @brief Convert unified error to entitlements error
 * @param[in] error Unified error code
 * @return Entitlements error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_ent_error_t cupolas_error_to_ent(cupolas_error_t error);

/**
 * @brief Convert unified error to vault error
 * @param[in] error Unified error code
 * @return Vault error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_vault_error_t cupolas_error_to_vault(cupolas_error_t error);

/**
 * @brief Convert unified error to network security error
 * @param[in] error Unified error code
 * @return Network security error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_net_error_t cupolas_error_to_net(cupolas_error_t error);

/**
 * @brief Convert unified error to runtime protection error
 * @param[in] error Unified error code
 * @return Runtime protection error code
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_runtime_error_t cupolas_error_to_runtime(cupolas_error_t error);

/* ============================================================================
 * Error Code Utility Macros
 * ============================================================================ */

/**
 * @brief Check if error indicates success
 * @param[in] e Error code
 * @return true if success, false otherwise
 */
#define cupolas_ERROR_IS_SUCCESS(e)   ((e) == cupolas_ERR_OK)

/**
 * @brief Check if error is fatal (cannot recover)
 * @param[in] e Error code
 * @return true if fatal, false otherwise
 */
#define cupolas_ERROR_IS_FATAL(e)     ((e) <  cupolas_ERR_OUT_OF_MEMORY)

/**
 * @brief Check if error is a parameter validation error
 * @param[in] e Error code
 * @return true if parameter error, false otherwise
 */
#define cupolas_ERROR_IS_PARAM(e)     ((e) == cupolas_ERR_INVALID_PARAM || \
                                    (e) == cupolas_ERR_NULL_POINTER)

/* ============================================================================
 * Backward Compatibility Aliases
 * ============================================================================ */

#ifndef AGENTOS_OK
#define AGENTOS_OK                    cupolas_ERR_OK
#endif
#ifndef AGENTOS_ERR_UNKNOWN
#define AGENTOS_ERR_UNKNOWN          cupolas_ERR_UNKNOWN
#endif
#ifndef AGENTOS_ERR_INVALID_PARAM
#define AGENTOS_ERR_INVALID_PARAM   cupolas_ERR_INVALID_PARAM
#endif
#ifndef AGENTOS_ERR_NULL_POINTER
#define AGENTOS_ERR_NULL_POINTER    cupolas_ERR_NULL_POINTER
#endif
#ifndef AGENTOS_ERR_OUT_OF_MEMORY
#define AGENTOS_ERR_OUT_OF_MEMORY   cupolas_ERR_OUT_OF_MEMORY
#endif
#ifndef AGENTOS_ERR_BUFFER_TOO_SMALL
#define AGENTOS_ERR_BUFFER_TOO_SMALL cupolas_ERR_BUFFER_TOO_SMALL
#endif
#ifndef AGENTOS_ERR_NOT_FOUND
#define AGENTOS_ERR_NOT_FOUND        cupolas_ERR_NOT_FOUND
#endif
#ifndef AGENTOS_ERR_ALREADY_EXISTS
#define AGENTOS_ERR_ALREADY_EXISTS   cupolas_ERR_ALREADY_EXISTS
#endif
#ifndef AGENTOS_ERR_TIMEOUT
#define AGENTOS_ERR_TIMEOUT          cupolas_ERR_TIMEOUT
#endif
#ifndef AGENTOS_ERR_NOT_SUPPORTED
#define AGENTOS_ERR_NOT_SUPPORTED    cupolas_ERR_NOT_SUPPORTED
#endif
#ifndef AGENTOS_ERR_PERMISSION_DENIED
#define AGENTOS_ERR_PERMISSION_DENIED cupolas_ERR_PERMISSION_DENIED
#endif
#ifndef AGENTOS_ERR_IO
#define AGENTOS_ERR_IO              cupolas_ERR_IO
#endif
#ifndef AGENTOS_ERR_STATE_ERROR
#define AGENTOS_ERR_STATE_ERROR     cupolas_ERR_STATE_ERROR
#endif
#ifndef AGENTOS_ERR_OVERFLOW
#define AGENTOS_ERR_OVERFLOW        cupolas_ERR_OVERFLOW
#endif

#ifndef cupolas_OK
#define cupolas_OK                    cupolas_ERR_OK
#endif
#ifndef cupolas_ERROR_UNKNOWN
#define cupolas_ERROR_UNKNOWN         cupolas_ERR_UNKNOWN
#endif
#ifndef cupolas_ERROR_INVALID_ARG
#define cupolas_ERROR_INVALID_ARG     cupolas_ERR_INVALID_PARAM
#endif
#ifndef cupolas_ERROR_NO_MEMORY
#define cupolas_ERROR_NO_MEMORY       cupolas_ERR_OUT_OF_MEMORY
#endif
#ifndef cupolas_ERROR_NOT_FOUND
#define cupolas_ERROR_NOT_FOUND       cupolas_ERR_NOT_FOUND
#endif
#ifndef cupolas_ERROR_PERMISSION
#define cupolas_ERROR_PERMISSION      cupolas_ERR_PERMISSION_DENIED
#endif
#ifndef cupolas_ERROR_BUSY
#define cupolas_ERROR_BUSY            cupolas_ERR_STATE_ERROR
#endif
#ifndef cupolas_ERROR_TIMEOUT
#define cupolas_ERROR_TIMEOUT         cupolas_ERR_TIMEOUT
#endif
#ifndef cupolas_ERROR_WOULD_BLOCK
#define cupolas_ERROR_WOULD_BLOCK     cupolas_ERR_TRY_AGAIN
#endif
#ifndef cupolas_ERROR_OVERFLOW
#define cupolas_ERROR_OVERFLOW         cupolas_ERR_OVERFLOW
#endif
#ifndef cupolas_ERROR_NOT_SUPPORTED
#define cupolas_ERROR_NOT_SUPPORTED   cupolas_ERR_NOT_SUPPORTED
#endif
#ifndef cupolas_ERROR_IO
#define cupolas_ERROR_IO             cupolas_ERR_IO
#endif

#ifdef __cplusplus
}
#endif

#endif /* cupolas_ERROR_H */
