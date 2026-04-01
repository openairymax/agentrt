/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_error.c - Unified Error Code Implementation
 */

/**
 * @file cupolas_error.c
 * @brief Unified Error Code Implementation
 * @author Spharx
 * @date 2026-03-26
 */

#include "cupolas_error.h"
#include <string.h>

/* ============================================================================
 * 错误码字符串映射
 * ============================================================================ */

static const char* g_error_strings[] = {
    [0 - cupolas_ERR_OK]               = "Success",
    [0 - cupolas_ERR_UNKNOWN]          = "Unknown error",
    [0 - cupolas_ERR_INVALID_PARAM]    = "Invalid parameter",
    [0 - cupolas_ERR_NULL_POINTER]    = "Null pointer",
    [0 - cupolas_ERR_OUT_OF_MEMORY]   = "Out of memory",
    [0 - cupolas_ERR_BUFFER_TOO_SMALL] = "Buffer too small",
    [0 - cupolas_ERR_NOT_FOUND]        = "Not found",
    [0 - cupolas_ERR_ALREADY_EXISTS]  = "Already exists",
    [0 - cupolas_ERR_TIMEOUT]          = "Timeout",
    [0 - cupolas_ERR_NOT_SUPPORTED]   = "Not supported",
    [0 - cupolas_ERR_PERMISSION_DENIED] = "Permission denied",
    [0 - cupolas_ERR_IO]              = "I/O error",
    [0 - cupolas_ERR_STATE_ERROR]     = "State error",
    [0 - cupolas_ERR_OVERFLOW]        = "Overflow",
    [0 - cupolas_ERR_TRY_AGAIN]       = "Try again",
    [0 - cupolas_ERR_AUTH_FAILED]     = "Authentication failed",
    [0 - cupolas_ERR_CERT_INVALID]    = "Certificate invalid",
    [0 - cupolas_ERR_CERT_EXPIRED]    = "Certificate expired",
    [0 - cupolas_ERR_SIGNATURE_INVALID] = "Signature invalid",
    [0 - cupolas_ERR_TAMPERED]        = "Data tampered"
};

const char* cupolas_error_string(cupolas_error_t error) {
    int index = 0 - error;
    if (index < 0 || (size_t)index >= sizeof(g_error_strings) / sizeof(g_error_strings[0])) {
        return "Unknown error";
    }
    if (g_error_strings[index] == NULL) {
        return "Unknown error";
    }
    return g_error_strings[index];
}

/* ============================================================================
 * 错误码映射宏 - 减少重复代码
 * ============================================================================ */

#define ERROR_MAP(from, to) case from: return to
#define ERROR_MAP_DEFAULT return cupolas_ERR_UNKNOWN

/* ============================================================================
 * 模块错误�?-> 统一错误�? * ============================================================================ */

cupolas_error_t cupolas_error_from_sig(cupolas_sig_error_t sig_error) {
    switch (sig_error) {
        ERROR_MAP(cupolas_SIG_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_SIG_ERR_INVALID, cupolas_ERR_SIGNATURE_INVALID);
        ERROR_MAP(cupolas_SIG_ERR_EXPIRED, cupolas_ERR_CERT_EXPIRED);
        ERROR_MAP(cupolas_SIG_ERR_REVOKED, cupolas_ERR_CERT_INVALID);
        ERROR_MAP(cupolas_SIG_ERR_UNTRUSTED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_SIG_ERR_TAMPERED, cupolas_ERR_TAMPERED);
        ERROR_MAP(cupolas_SIG_ERR_NO_SIGNATURE, cupolas_ERR_NOT_FOUND);
        ERROR_MAP(cupolas_SIG_ERR_CERT_INVALID, cupolas_ERR_CERT_INVALID);
        ERROR_MAP(cupolas_SIG_ERR_CERT_EXPIRED, cupolas_ERR_CERT_EXPIRED);
        ERROR_MAP(cupolas_SIG_ERR_ALGO_UNSUPPORTED, cupolas_ERR_NOT_SUPPORTED);
        default: ERROR_MAP_DEFAULT;
    }
}

cupolas_error_t cupolas_error_from_ent(cupolas_ent_error_t ent_error) {
    switch (ent_error) {
        ERROR_MAP(cupolas_ENT_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_ENT_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_ENT_ERR_SIGNATURE_INVALID, cupolas_ERR_SIGNATURE_INVALID);
        ERROR_MAP(cupolas_ENT_ERR_EXPIRED, cupolas_ERR_CERT_EXPIRED);
        ERROR_MAP(cupolas_ENT_ERR_DENIED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_ENT_ERR_NOT_FOUND, cupolas_ERR_NOT_FOUND);
        ERROR_MAP(cupolas_ENT_ERR_PARSE_ERROR, cupolas_ERR_INVALID_PARAM);
        default: ERROR_MAP_DEFAULT;
    }
}

cupolas_error_t cupolas_error_from_vault(cupolas_vault_error_t vault_error) {
    switch (vault_error) {
        ERROR_MAP(cupolas_VAULT_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_VAULT_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_VAULT_ERR_LOCKED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_VAULT_ERR_ACCESS_DENIED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_VAULT_ERR_NOT_FOUND, cupolas_ERR_NOT_FOUND);
        ERROR_MAP(cupolas_VAULT_ERR_ALREADY_EXISTS, cupolas_ERR_ALREADY_EXISTS);
        ERROR_MAP(cupolas_VAULT_ERR_CORRUPT, cupolas_ERR_IO);
        ERROR_MAP(cupolas_VAULT_ERR_DECRYPT_FAILED, cupolas_ERR_AUTH_FAILED);
        ERROR_MAP(cupolas_VAULT_ERR_ENCRYPT_FAILED, cupolas_ERR_IO);
        default: ERROR_MAP_DEFAULT;
    }
}

cupolas_error_t cupolas_error_from_net(cupolas_net_error_t net_error) {
    switch (net_error) {
        ERROR_MAP(cupolas_NET_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_NET_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_NET_ERR_CERT_INVALID, cupolas_ERR_CERT_INVALID);
        ERROR_MAP(cupolas_NET_ERR_CERT_EXPIRED, cupolas_ERR_CERT_EXPIRED);
        ERROR_MAP(cupolas_NET_ERR_CERT_REVOKED, cupolas_ERR_CERT_INVALID);
        ERROR_MAP(cupolas_NET_ERR_UNTRUSTED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_NET_ERR_HOST_MISMATCH, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_NET_ERR_DENIED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_NET_ERR_TIMEOUT, cupolas_ERR_TIMEOUT);
        default: ERROR_MAP_DEFAULT;
    }
}

cupolas_error_t cupolas_error_from_runtime(cupolas_runtime_error_t runtime_error) {
    switch (runtime_error) {
        ERROR_MAP(cupolas_RUNTIME_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_RUNTIME_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_RUNTIME_ERR_VIOLATION, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_RUNTIME_ERR_COMPROMISED, cupolas_ERR_TAMPERED);
        ERROR_MAP(cupolas_RUNTIME_ERR_NOT_SUPPORTED, cupolas_ERR_NOT_SUPPORTED);
        default: ERROR_MAP_DEFAULT;
    }
}

/* ============================================================================
 * 统一错误�?-> 模块错误�? * ============================================================================ */

#undef ERROR_MAP
#undef ERROR_MAP_DEFAULT

#define ERROR_MAP(to, from) case from: return to
#define ERROR_MAP_DEFAULT(to) default: return to

cupolas_sig_error_t cupolas_error_to_sig(cupolas_error_t error) {
    switch (error) {
        ERROR_MAP(cupolas_SIG_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_SIG_ERR_INVALID, cupolas_ERR_SIGNATURE_INVALID);
        ERROR_MAP(cupolas_SIG_ERR_EXPIRED, cupolas_ERR_CERT_EXPIRED);
        ERROR_MAP(cupolas_SIG_ERR_CERT_INVALID, cupolas_ERR_CERT_INVALID);
        ERROR_MAP(cupolas_SIG_ERR_UNTRUSTED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_SIG_ERR_TAMPERED, cupolas_ERR_TAMPERED);
        ERROR_MAP(cupolas_SIG_ERR_NO_SIGNATURE, cupolas_ERR_NOT_FOUND);
        ERROR_MAP(cupolas_SIG_ERR_ALGO_UNSUPPORTED, cupolas_ERR_NOT_SUPPORTED);
        ERROR_MAP_DEFAULT(cupolas_SIG_ERR_INVALID);
    }
}

cupolas_ent_error_t cupolas_error_to_ent(cupolas_error_t error) {
    switch (error) {
        ERROR_MAP(cupolas_ENT_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_ENT_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_ENT_ERR_SIGNATURE_INVALID, cupolas_ERR_SIGNATURE_INVALID);
        ERROR_MAP(cupolas_ENT_ERR_EXPIRED, cupolas_ERR_CERT_EXPIRED);
        ERROR_MAP(cupolas_ENT_ERR_DENIED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_ENT_ERR_NOT_FOUND, cupolas_ERR_NOT_FOUND);
        ERROR_MAP_DEFAULT(cupolas_ENT_ERR_INVALID);
    }
}

cupolas_vault_error_t cupolas_error_to_vault(cupolas_error_t error) {
    switch (error) {
        ERROR_MAP(cupolas_VAULT_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_VAULT_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_VAULT_ERR_ACCESS_DENIED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_VAULT_ERR_NOT_FOUND, cupolas_ERR_NOT_FOUND);
        ERROR_MAP(cupolas_VAULT_ERR_ALREADY_EXISTS, cupolas_ERR_ALREADY_EXISTS);
        ERROR_MAP(cupolas_VAULT_ERR_DECRYPT_FAILED, cupolas_ERR_AUTH_FAILED);
        ERROR_MAP(cupolas_VAULT_ERR_CORRUPT, cupolas_ERR_IO);
        ERROR_MAP_DEFAULT(cupolas_VAULT_ERR_INVALID);
    }
}

cupolas_net_error_t cupolas_error_to_net(cupolas_error_t error) {
    switch (error) {
        ERROR_MAP(cupolas_NET_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_NET_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_NET_ERR_CERT_INVALID, cupolas_ERR_CERT_INVALID);
        ERROR_MAP(cupolas_NET_ERR_CERT_EXPIRED, cupolas_ERR_CERT_EXPIRED);
        ERROR_MAP(cupolas_NET_ERR_DENIED, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_NET_ERR_TIMEOUT, cupolas_ERR_TIMEOUT);
        ERROR_MAP_DEFAULT(cupolas_NET_ERR_INVALID);
    }
}

cupolas_runtime_error_t cupolas_error_to_runtime(cupolas_error_t error) {
    switch (error) {
        ERROR_MAP(cupolas_RUNTIME_ERR_OK, cupolas_ERR_OK);
        ERROR_MAP(cupolas_RUNTIME_ERR_INVALID, cupolas_ERR_INVALID_PARAM);
        ERROR_MAP(cupolas_RUNTIME_ERR_VIOLATION, cupolas_ERR_PERMISSION_DENIED);
        ERROR_MAP(cupolas_RUNTIME_ERR_COMPROMISED, cupolas_ERR_TAMPERED);
        ERROR_MAP(cupolas_RUNTIME_ERR_NOT_SUPPORTED, cupolas_ERR_NOT_SUPPORTED);
        ERROR_MAP_DEFAULT(cupolas_RUNTIME_ERR_INVALID);
    }
}
