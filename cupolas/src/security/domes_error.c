/**
 * @file domes_error.c
 * @brief 统一错误码实现
 * @author Spharx
 * @date 2026-03-26
 */

#include "domes_error.h"
#include <string.h>

/* ============================================================================
 * 错误码字符串映射
 * ============================================================================ */

static const char* g_error_strings[] = {
    [0 - DOMES_ERR_OK]               = "Success",
    [0 - DOMES_ERR_UNKNOWN]          = "Unknown error",
    [0 - DOMES_ERR_INVALID_PARAM]    = "Invalid parameter",
    [0 - DOMES_ERR_NULL_POINTER]    = "Null pointer",
    [0 - DOMES_ERR_OUT_OF_MEMORY]   = "Out of memory",
    [0 - DOMES_ERR_BUFFER_TOO_SMALL] = "Buffer too small",
    [0 - DOMES_ERR_NOT_FOUND]        = "Not found",
    [0 - DOMES_ERR_ALREADY_EXISTS]  = "Already exists",
    [0 - DOMES_ERR_TIMEOUT]          = "Timeout",
    [0 - DOMES_ERR_NOT_SUPPORTED]   = "Not supported",
    [0 - DOMES_ERR_PERMISSION_DENIED] = "Permission denied",
    [0 - DOMES_ERR_IO]              = "I/O error",
    [0 - DOMES_ERR_STATE_ERROR]     = "State error",
    [0 - DOMES_ERR_OVERFLOW]        = "Overflow",
    [0 - DOMES_ERR_TRY_AGAIN]       = "Try again",
    [0 - DOMES_ERR_AUTH_FAILED]     = "Authentication failed",
    [0 - DOMES_ERR_CERT_INVALID]    = "Certificate invalid",
    [0 - DOMES_ERR_CERT_EXPIRED]    = "Certificate expired",
    [0 - DOMES_ERR_SIGNATURE_INVALID] = "Signature invalid",
    [0 - DOMES_ERR_TAMPERED]        = "Data tampered"
};

const char* domes_error_string(domes_error_t error) {
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
#define ERROR_MAP_DEFAULT return DOMES_ERR_UNKNOWN

/* ============================================================================
 * 模块错误码 -> 统一错误码
 * ============================================================================ */

domes_error_t domes_error_from_sig(domes_sig_error_t sig_error) {
    switch (sig_error) {
        ERROR_MAP(DOMES_SIG_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_SIG_ERR_INVALID, DOMES_ERR_SIGNATURE_INVALID);
        ERROR_MAP(DOMES_SIG_ERR_EXPIRED, DOMES_ERR_CERT_EXPIRED);
        ERROR_MAP(DOMES_SIG_ERR_REVOKED, DOMES_ERR_CERT_INVALID);
        ERROR_MAP(DOMES_SIG_ERR_UNTRUSTED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_SIG_ERR_TAMPERED, DOMES_ERR_TAMPERED);
        ERROR_MAP(DOMES_SIG_ERR_NO_SIGNATURE, DOMES_ERR_NOT_FOUND);
        ERROR_MAP(DOMES_SIG_ERR_CERT_INVALID, DOMES_ERR_CERT_INVALID);
        ERROR_MAP(DOMES_SIG_ERR_CERT_EXPIRED, DOMES_ERR_CERT_EXPIRED);
        ERROR_MAP(DOMES_SIG_ERR_ALGO_UNSUPPORTED, DOMES_ERR_NOT_SUPPORTED);
        default: ERROR_MAP_DEFAULT;
    }
}

domes_error_t domes_error_from_ent(domes_ent_error_t ent_error) {
    switch (ent_error) {
        ERROR_MAP(DOMES_ENT_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_ENT_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_ENT_ERR_SIGNATURE_INVALID, DOMES_ERR_SIGNATURE_INVALID);
        ERROR_MAP(DOMES_ENT_ERR_EXPIRED, DOMES_ERR_CERT_EXPIRED);
        ERROR_MAP(DOMES_ENT_ERR_DENIED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_ENT_ERR_NOT_FOUND, DOMES_ERR_NOT_FOUND);
        ERROR_MAP(DOMES_ENT_ERR_PARSE_ERROR, DOMES_ERR_INVALID_PARAM);
        default: ERROR_MAP_DEFAULT;
    }
}

domes_error_t domes_error_from_vault(domes_vault_error_t vault_error) {
    switch (vault_error) {
        ERROR_MAP(DOMES_VAULT_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_VAULT_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_VAULT_ERR_LOCKED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_VAULT_ERR_ACCESS_DENIED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_VAULT_ERR_NOT_FOUND, DOMES_ERR_NOT_FOUND);
        ERROR_MAP(DOMES_VAULT_ERR_ALREADY_EXISTS, DOMES_ERR_ALREADY_EXISTS);
        ERROR_MAP(DOMES_VAULT_ERR_CORRUPT, DOMES_ERR_IO);
        ERROR_MAP(DOMES_VAULT_ERR_DECRYPT_FAILED, DOMES_ERR_AUTH_FAILED);
        ERROR_MAP(DOMES_VAULT_ERR_ENCRYPT_FAILED, DOMES_ERR_IO);
        default: ERROR_MAP_DEFAULT;
    }
}

domes_error_t domes_error_from_net(domes_net_error_t net_error) {
    switch (net_error) {
        ERROR_MAP(DOMES_NET_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_NET_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_NET_ERR_CERT_INVALID, DOMES_ERR_CERT_INVALID);
        ERROR_MAP(DOMES_NET_ERR_CERT_EXPIRED, DOMES_ERR_CERT_EXPIRED);
        ERROR_MAP(DOMES_NET_ERR_CERT_REVOKED, DOMES_ERR_CERT_INVALID);
        ERROR_MAP(DOMES_NET_ERR_UNTRUSTED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_NET_ERR_HOST_MISMATCH, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_NET_ERR_DENIED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_NET_ERR_TIMEOUT, DOMES_ERR_TIMEOUT);
        default: ERROR_MAP_DEFAULT;
    }
}

domes_error_t domes_error_from_runtime(domes_runtime_error_t runtime_error) {
    switch (runtime_error) {
        ERROR_MAP(DOMES_RUNTIME_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_RUNTIME_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_RUNTIME_ERR_VIOLATION, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_RUNTIME_ERR_COMPROMISED, DOMES_ERR_TAMPERED);
        ERROR_MAP(DOMES_RUNTIME_ERR_NOT_SUPPORTED, DOMES_ERR_NOT_SUPPORTED);
        default: ERROR_MAP_DEFAULT;
    }
}

/* ============================================================================
 * 统一错误码 -> 模块错误码
 * ============================================================================ */

#undef ERROR_MAP
#undef ERROR_MAP_DEFAULT

#define ERROR_MAP(to, from) case from: return to
#define ERROR_MAP_DEFAULT(to) default: return to

domes_sig_error_t domes_error_to_sig(domes_error_t error) {
    switch (error) {
        ERROR_MAP(DOMES_SIG_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_SIG_ERR_INVALID, DOMES_ERR_SIGNATURE_INVALID);
        ERROR_MAP(DOMES_SIG_ERR_EXPIRED, DOMES_ERR_CERT_EXPIRED);
        ERROR_MAP(DOMES_SIG_ERR_CERT_INVALID, DOMES_ERR_CERT_INVALID);
        ERROR_MAP(DOMES_SIG_ERR_UNTRUSTED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_SIG_ERR_TAMPERED, DOMES_ERR_TAMPERED);
        ERROR_MAP(DOMES_SIG_ERR_NO_SIGNATURE, DOMES_ERR_NOT_FOUND);
        ERROR_MAP(DOMES_SIG_ERR_ALGO_UNSUPPORTED, DOMES_ERR_NOT_SUPPORTED);
        ERROR_MAP_DEFAULT(DOMES_SIG_ERR_INVALID);
    }
}

domes_ent_error_t domes_error_to_ent(domes_error_t error) {
    switch (error) {
        ERROR_MAP(DOMES_ENT_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_ENT_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_ENT_ERR_SIGNATURE_INVALID, DOMES_ERR_SIGNATURE_INVALID);
        ERROR_MAP(DOMES_ENT_ERR_EXPIRED, DOMES_ERR_CERT_EXPIRED);
        ERROR_MAP(DOMES_ENT_ERR_DENIED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_ENT_ERR_NOT_FOUND, DOMES_ERR_NOT_FOUND);
        ERROR_MAP_DEFAULT(DOMES_ENT_ERR_INVALID);
    }
}

domes_vault_error_t domes_error_to_vault(domes_error_t error) {
    switch (error) {
        ERROR_MAP(DOMES_VAULT_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_VAULT_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_VAULT_ERR_ACCESS_DENIED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_VAULT_ERR_NOT_FOUND, DOMES_ERR_NOT_FOUND);
        ERROR_MAP(DOMES_VAULT_ERR_ALREADY_EXISTS, DOMES_ERR_ALREADY_EXISTS);
        ERROR_MAP(DOMES_VAULT_ERR_DECRYPT_FAILED, DOMES_ERR_AUTH_FAILED);
        ERROR_MAP(DOMES_VAULT_ERR_CORRUPT, DOMES_ERR_IO);
        ERROR_MAP_DEFAULT(DOMES_VAULT_ERR_INVALID);
    }
}

domes_net_error_t domes_error_to_net(domes_error_t error) {
    switch (error) {
        ERROR_MAP(DOMES_NET_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_NET_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_NET_ERR_CERT_INVALID, DOMES_ERR_CERT_INVALID);
        ERROR_MAP(DOMES_NET_ERR_CERT_EXPIRED, DOMES_ERR_CERT_EXPIRED);
        ERROR_MAP(DOMES_NET_ERR_DENIED, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_NET_ERR_TIMEOUT, DOMES_ERR_TIMEOUT);
        ERROR_MAP_DEFAULT(DOMES_NET_ERR_INVALID);
    }
}

domes_runtime_error_t domes_error_to_runtime(domes_error_t error) {
    switch (error) {
        ERROR_MAP(DOMES_RUNTIME_ERR_OK, DOMES_ERR_OK);
        ERROR_MAP(DOMES_RUNTIME_ERR_INVALID, DOMES_ERR_INVALID_PARAM);
        ERROR_MAP(DOMES_RUNTIME_ERR_VIOLATION, DOMES_ERR_PERMISSION_DENIED);
        ERROR_MAP(DOMES_RUNTIME_ERR_COMPROMISED, DOMES_ERR_TAMPERED);
        ERROR_MAP(DOMES_RUNTIME_ERR_NOT_SUPPORTED, DOMES_ERR_NOT_SUPPORTED);
        ERROR_MAP_DEFAULT(DOMES_RUNTIME_ERR_INVALID);
    }
}
