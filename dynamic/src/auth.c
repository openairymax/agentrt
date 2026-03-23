/**
 * @file auth.c
 * @brief API认证模块实现
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "auth.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 认证上下文结构 */
struct auth_context {
    char*    api_key;       /**< API密钥 */
    size_t   api_key_len;   /**< 密钥长度 */
    bool     enabled;       /**< 是否启用认证 */
};

auth_context_t* auth_context_create(const char* api_key) {
    auth_context_t* ctx = (auth_context_t*)calloc(1, sizeof(auth_context_t));
    if (!ctx) return NULL;
    
    if (api_key && api_key[0] != '\0') {
        ctx->api_key = strdup(api_key);
        if (!ctx->api_key) {
            free(ctx);
            return NULL;
        }
        ctx->api_key_len = strlen(api_key);
        ctx->enabled = true;
    } else {
        ctx->api_key = NULL;
        ctx->api_key_len = 0;
        ctx->enabled = false;
    }
    
    AGENTOS_LOG_INFO("Auth context created: %s", 
        ctx->enabled ? "enabled" : "disabled");
    
    return ctx;
}

void auth_context_destroy(auth_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->api_key) {
        /* 安全清除密钥内存 */
        memset(ctx->api_key, 0, ctx->api_key_len);
        free(ctx->api_key);
    }
    
    free(ctx);
    AGENTOS_LOG_INFO("Auth context destroyed");
}

auth_result_t auth_validate(auth_context_t* ctx, const char* auth_header) {
    if (!ctx) return AUTH_RESULT_DISABLED;
    
    if (!ctx->enabled) return AUTH_RESULT_DISABLED;
    
    if (!auth_header) return AUTH_RESULT_DENIED;
    
    /* 检查 Bearer Token 格式 */
    const char* prefix = "Bearer ";
    size_t prefix_len = strlen(prefix);
    
    if (strncmp(auth_header, prefix, prefix_len) != 0) {
        AGENTOS_LOG_WARN("Invalid auth header format");
        return AUTH_RESULT_DENIED;
    }
    
    const char* token = auth_header + prefix_len;
    size_t token_len = strlen(token);
    
    /* 常量时间比较，防止时序攻击 */
    if (token_len != ctx->api_key_len) {
        return AUTH_RESULT_DENIED;
    }
    
    volatile int result = 0;
    for (size_t i = 0; i < token_len; i++) {
        result |= (token[i] ^ ctx->api_key[i]);
    }
    
    if (result != 0) {
        AGENTOS_LOG_WARN("Auth token mismatch");
        return AUTH_RESULT_DENIED;
    }
    
    return AUTH_RESULT_ALLOWED;
}

bool auth_is_enabled(auth_context_t* ctx) {
    return ctx ? ctx->enabled : false;
}

agentos_error_t auth_set_api_key(auth_context_t* ctx, const char* api_key) {
    if (!ctx) return AGENTOS_EINVAL;
    
    /* 清除旧密钥 */
    if (ctx->api_key) {
        memset(ctx->api_key, 0, ctx->api_key_len);
        free(ctx->api_key);
    }
    
    if (api_key && api_key[0] != '\0') {
        ctx->api_key = strdup(api_key);
        if (!ctx->api_key) {
            ctx->api_key_len = 0;
            ctx->enabled = false;
            return AGENTOS_ENOMEM;
        }
        ctx->api_key_len = strlen(api_key);
        ctx->enabled = true;
    } else {
        ctx->api_key = NULL;
        ctx->api_key_len = 0;
        ctx->enabled = false;
    }
    
    AGENTOS_LOG_INFO("API key updated: %s", 
        ctx->enabled ? "enabled" : "disabled");
    
    return AGENTOS_SUCCESS;
}

agentos_error_t auth_generate_error_response(
    auth_context_t* ctx,
    char** out_json) {
    
    (void)ctx;
    
    if (!out_json) return AGENTOS_EINVAL;
    
    *out_json = strdup(
        "{\"jsonrpc\":\"2.0\","
        "\"error\":{\"code\":-32600,\"message\":\"Authentication required\"},"
        "\"id\":null}"
    );
    
    return *out_json ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}
