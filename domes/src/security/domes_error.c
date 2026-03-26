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
 * 模块错误码 -> 统一错误码
 * ============================================================================ */

domes_error_t domes_error_from_sig(domes_sig_error_t sig_error) {
    switch (sig_error) {
        case DOMES_SIG_ERR_OK:           return DOMES_ERR_OK;
        case DOMES_SIG_ERR_INVALID:      return DOMES_ERR_SIGNATURE_INVALID;
        case DOMES_SIG_ERR_EXPIRED:      return DOMES_ERR_CERT_EXPIRED;
        case DOMES_SIG_ERR_REVOKED:       return DOMES_ERR_CERT_INVALID;
        case DOMES_SIG_ERR_UNTRUSTED:     return DOMES_ERR_PERMISSION_DENIED;
        case DOMES_SIG_ERR_TAMPERED:      return DOMES_ERR_TAMPERED;
        case DOMES_SIG_ERR_NO_SIGNATURE:  return DOMES_ERR_NOT_FOUND;
        case DOMES_SIG_ERR_CERT_INVALID:  return DOMES_ERR_CERT_INVALID;
        case DOMES_SIG_ERR_CERT_EXPIRED:  return DOMES_ERR_CERT_EXPIRED;
        case DOMES_SIG_ERR_ALGO_UNSUPPORTED: return DOMES_ERR_NOT_SUPPORTED;
        default:                           return DOMES_ERR_UNKNOWN;
    }
}

domes_error_t domes_error_from_ent(domes_ent_error_t ent_error) {
    switch (ent_error) {
        case DOMES_ENT_ERR_OK:               return DOMES_ERR_OK;
        case DOMES_ENT_ERR_INVALID:           return DOMES_ERR_INVALID_PARAM;
        case DOMES_ENT_ERR_SIGNATURE_INVALID: return DOMES_ERR_SIGNATURE_INVALID;
        case DOMES_ENT_ERR_EXPIRED:           return DOMES_ERR_CERT_EXPIRED;
        case DOMES_ENT_ERR_DENIED:           return DOMES_ERR_PERMISSION_DENIED;
        case DOMES_ENT_ERR_NOT_FOUND:         return DOMES_ERR_NOT_FOUND;
        case DOMES_ENT_ERR_PARSE_ERROR:       return DOMES_ERR_INVALID_PARAM;
        default:                               return DOMES_ERR_UNKNOWN;
    }
}

domes_error_t domes_error_from_vault(domes_vault_error_t vault_error) {
    switch (vault_error) {
        case DOMES_VAULT_ERR_OK:             return DOMES_ERR_OK;
        case DOMES_VAULT_ERR_INVALID:         return DOMES_ERR_INVALID_PARAM;
        case DOMES_VAULT_ERR_LOCKED:          return DOMES_ERR_PERMISSION_DENIED;
        case DOMES_VAULT_ERR_ACCESS_DENIED:  return DOMES_ERR_PERMISSION_DENIED;
        case DOMES_VAULT_ERR_NOT_FOUND:       return DOMES_ERR_NOT_FOUND;
        case DOMES_VAULT_ERR_ALREADY_EXISTS:  return DOMES_ERR_ALREADY_EXISTS;
        case DOMES_VAULT_ERR_CORRUPT:         return DOMES_ERR_IO;
        case DOMES_VAULT_ERR_DECRYPT_FAILED:  return DOMES_ERR_AUTH_FAILED;
        case DOMES_VAULT_ERR_ENCRYPT_FAILED:  return DOMES_ERR_IO;
        default:                               return DOMES_ERR_UNKNOWN;
    }
}

domes_error_t domes_error_from_net(domes_net_error_t net_error) {
    switch (net_error) {
        case DOMES_NET_ERR_OK:            return DOMES_ERR_OK;
        case DOMES_NET_ERR_INVALID:       return DOMES_ERR_INVALID_PARAM;
        case DOMES_NET_ERR_CERT_INVALID:   return DOMES_ERR_CERT_INVALID;
        case DOMES_NET_ERR_CERT_EXPIRED:   return DOMES_ERR_CERT_EXPIRED;
        case DOMES_NET_ERR_CERT_REVOKED:   return DOMES_ERR_CERT_INVALID;
        case DOMES_NET_ERR_UNTRUSTED:      return DOMES_ERR_PERMISSION_DENIED;
        case DOMES_NET_ERR_HOST_MISMATCH:  return DOMES_ERR_INVALID_PARAM;
        case DOMES_NET_ERR_DENIED:         return DOMES_ERR_PERMISSION_DENIED;
        case DOMES_NET_ERR_TIMEOUT:         return DOMES_ERR_TIMEOUT;
        default:                            return DOMES_ERR_UNKNOWN;
    }
}

domes_error_t domes_error_from_runtime(domes_runtime_error_t runtime_error) {
    switch (runtime_error) {
        case DOMES_RUNTIME_ERR_OK:           return DOMES_ERR_OK;
        case DOMES_RUNTIME_ERR_INVALID:      return DOMES_ERR_INVALID_PARAM;
        case DOMES_RUNTIME_ERR_VIOLATION:     return DOMES_ERR_PERMISSION_DENIED;
        case DOMES_RUNTIME_ERR_COMPROMISED:   return DOMES_ERR_TAMPERED;
        case DOMES_RUNTIME_ERR_NOT_SUPPORTED:  return DOMES_ERR_NOT_SUPPORTED;
        default:                               return DOMES_ERR_UNKNOWN;
    }
}

/* ============================================================================
 * 统一错误码 -> 模块错误码
 * ============================================================================ */

domes_sig_error_t domes_error_to_sig(domes_error_t error) {
    switch (error) {
        case DOMES_ERR_OK:                return DOMES_SIG_ERR_OK;
        case DOMES_ERR_SIGNATURE_INVALID:  return DOMES_SIG_ERR_INVALID;
        case DOMES_ERR_CERT_EXPIRED:       return DOMES_SIG_ERR_EXPIRED;
        case DOMES_ERR_CERT_INVALID:       return DOMES_SIG_ERR_CERT_INVALID;
        case DOMES_ERR_PERMISSION_DENIED:  return DOMES_SIG_ERR_UNTRUSTED;
        case DOMES_ERR_TAMPERED:           return DOMES_SIG_ERR_TAMPERED;
        case DOMES_ERR_NOT_FOUND:          return DOMES_SIG_ERR_NO_SIGNATURE;
        case DOMES_ERR_NOT_SUPPORTED:      return DOMES_SIG_ERR_ALGO_UNSUPPORTED;
        default:                            return DOMES_SIG_ERR_INVALID;
    }
}

domes_ent_error_t domes_error_to_ent(domes_error_t error) {
    switch (error) {
        case DOMES_ERR_OK:                return DOMES_ENT_ERR_OK;
        case DOMES_ERR_INVALID_PARAM:      return DOMES_ENT_ERR_INVALID;
        case DOMES_ERR_SIGNATURE_INVALID:  return DOMES_ENT_ERR_SIGNATURE_INVALID;
        case DOMES_ERR_CERT_EXPIRED:       return DOMES_ENT_ERR_EXPIRED;
        case DOMES_ERR_PERMISSION_DENIED:  return DOMES_ENT_ERR_DENIED;
        case DOMES_ERR_NOT_FOUND:          return DOMES_ENT_ERR_NOT_FOUND;
        default:                            return DOMES_ENT_ERR_INVALID;
    }
}

domes_vault_error_t domes_error_to_vault(domes_error_t error) {
    switch (error) {
        case DOMES_ERR_OK:                return DOMES_VAULT_ERR_OK;
        case DOMES_ERR_INVALID_PARAM:      return DOMES_VAULT_ERR_INVALID;
        case DOMES_ERR_PERMISSION_DENIED:  return DOMES_VAULT_ERR_ACCESS_DENIED;
        case DOMES_ERR_NOT_FOUND:          return DOMES_VAULT_ERR_NOT_FOUND;
        case DOMES_ERR_ALREADY_EXISTS:     return DOMES_VAULT_ERR_ALREADY_EXISTS;
        case DOMES_ERR_AUTH_FAILED:         return DOMES_VAULT_ERR_DECRYPT_FAILED;
        case DOMES_ERR_IO:                 return DOMES_VAULT_ERR_CORRUPT;
        default:                            return DOMES_VAULT_ERR_INVALID;
    }
}

domes_net_error_t domes_error_to_net(domes_error_t error) {
    switch (error) {
        case DOMES_ERR_OK:                return DOMES_NET_ERR_OK;
        case DOMES_ERR_INVALID_PARAM:      return DOMES_NET_ERR_INVALID;
        case DOMES_ERR_CERT_INVALID:       return DOMES_NET_ERR_CERT_INVALID;
        case DOMES_ERR_CERT_EXPIRED:       return DOMES_NET_ERR_CERT_EXPIRED;
        case DOMES_ERR_PERMISSION_DENIED:  return DOMES_NET_ERR_DENIED;
        case DOMES_ERR_TIMEOUT:             return DOMES_NET_ERR_TIMEOUT;
        default:                            return DOMES_NET_ERR_INVALID;
    }
}

domes_runtime_error_t domes_error_to_runtime(domes_error_t error) {
    switch (error) {
        case DOMES_ERR_OK:                return DOMES_RUNTIME_ERR_OK;
        case DOMES_ERR_INVALID_PARAM:      return DOMES_RUNTIME_ERR_INVALID;
        case DOMES_ERR_PERMISSION_DENIED:  return DOMES_RUNTIME_ERR_VIOLATION;
        case DOMES_ERR_TAMPERED:           return DOMES_RUNTIME_ERR_COMPROMISED;
        case DOMES_ERR_NOT_SUPPORTED:      return DOMES_RUNTIME_ERR_NOT_SUPPORTED;
        default:                            return DOMES_RUNTIME_ERR_INVALID;
    }
}
