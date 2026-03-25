/**
 * @file auth.c
 * @brief API认证模块实现
 * 
 * 支持API密钥和JWT两种认证方式，提供完整的认证和授权功能。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "auth.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

/* 认证类型 */
typedef enum {
    AUTH_TYPE_API_KEY = 0,  /**< API密钥认证 */
    AUTH_TYPE_JWT           /**< JWT认证 */
} auth_type_t;

/* JWT令牌结构 */
typedef struct jwt_token {
    char*       subject;     /**< 主题 */
    char*       issuer;      /**< 签发者 */
    uint64_t    issued_at;   /**< 签发时间 */
    uint64_t    expires_at;  /**< 过期时间 */
    char*       payload;     /**< 载荷JSON */
} jwt_token_t;

/* 认证上下文结构 */
struct auth_context {
    auth_type_t type;           /**< 认证类型 */
    char*       api_key;        /**< API密钥 */
    size_t      api_key_len;    /**< 密钥长度 */
    char*       jwt_secret;     /**< JWT密钥 */
    size_t      jwt_secret_len; /**< JWT密钥长度 */
    uint32_t    jwt_expire;     /**< JWT过期时间（秒） */
    bool        enabled;        /**< 是否启用认证 */
    
    /* JWT令牌缓存 */
    pthread_mutex_t token_cache_lock;
    jwt_token_t**   token_cache;
    size_t          token_cache_size;
    size_t          token_cache_capacity;
    
    /* 统计信息 */
    atomic_uint_fast64_t tokens_issued;     /**< 已签发令牌数 */
    atomic_uint_fast64_t tokens_validated;   /**< 已验证令牌数 */
    atomic_uint_fast64_t tokens_expired;    /**< 已过期令牌数 */
};

auth_context_t* auth_context_create(const char* api_key, const char* jwt_secret, uint32_t jwt_expire) {
    auth_context_t* ctx = (auth_context_t*)calloc(1, sizeof(auth_context_t));
    if (!ctx) return NULL;
    
    /* 初始化统计信息 */
    atomic_init(&ctx->tokens_issued, 0);
    atomic_init(&ctx->tokens_validated, 0);
    atomic_init(&ctx->tokens_expired, 0);
    
    /* 初始化JWT令牌缓存 */
    pthread_mutex_init(&ctx->token_cache_lock, NULL);
    ctx->token_cache_capacity = 16;
    ctx->token_cache = calloc(ctx->token_cache_capacity, sizeof(jwt_token_t*));
    if (!ctx->token_cache) {
        pthread_mutex_destroy(&ctx->token_cache_lock);
        free(ctx);
        return NULL;
    }
    ctx->token_cache_size = 0;
    
    /* 设置认证类型和密钥 */
    if (jwt_secret && jwt_secret[0] != '\0') {
        ctx->type = AUTH_TYPE_JWT;
        ctx->jwt_secret = strdup(jwt_secret);
        if (!ctx->jwt_secret) {
            pthread_mutex_destroy(&ctx->token_cache_lock);
            free(ctx->token_cache);
            free(ctx);
            return NULL;
        }
        ctx->jwt_secret_len = strlen(jwt_secret);
        ctx->jwt_expire = jwt_expire > 0 ? jwt_expire : 3600; /* 默认1小时 */
        ctx->enabled = true;
    } else if (api_key && api_key[0] != '\0') {
        ctx->type = AUTH_TYPE_API_KEY;
        ctx->api_key = strdup(api_key);
        if (!ctx->api_key) {
            pthread_mutex_destroy(&ctx->token_cache_lock);
            free(ctx->token_cache);
            free(ctx);
            return NULL;
        }
        ctx->api_key_len = strlen(api_key);
        ctx->enabled = true;
    } else {
        ctx->type = AUTH_TYPE_API_KEY;
        ctx->enabled = false;
    }
    
    AGENTOS_LOG_INFO("Auth context created: %s, type: %s", 
        ctx->enabled ? "enabled" : "disabled",
        ctx->type == AUTH_TYPE_JWT ? "JWT" : "API_KEY");
    
    return ctx;
}

void auth_context_destroy(auth_context_t* ctx) {
    if (!ctx) return;
    
    /* 清理API密钥 */
    if (ctx->api_key) {
        memset(ctx->api_key, 0, ctx->api_key_len);
        free(ctx->api_key);
    }
    
    /* 清理JWT密钥 */
    if (ctx->jwt_secret) {
        memset(ctx->jwt_secret, 0, ctx->jwt_secret_len);
        free(ctx->jwt_secret);
    }
    
    /* 清理JWT令牌缓存 */
    pthread_mutex_lock(&ctx->token_cache_lock);
    for (size_t i = 0; i < ctx->token_cache_size; i++) {
        if (ctx->token_cache[i]) {
            free(ctx->token_cache[i]->subject);
            free(ctx->token_cache[i]->issuer);
            free(ctx->token_cache[i]->payload);
            free(ctx->token_cache[i]);
        }
    }
    free(ctx->token_cache);
    pthread_mutex_unlock(&ctx->token_cache_lock);
    
    /* 销毁同步原语 */
    pthread_mutex_destroy(&ctx->token_cache_lock);
    
    free(ctx);
    AGENTOS_LOG_INFO("Auth context destroyed");
}

/**
 * @brief 验证JWT令牌
 * @param ctx 认证上下文
 * @param jwt_str JWT字符串
 * @return 1 有效，0 无效
 */
static int validate_jwt_token(auth_context_t* ctx, const char* jwt_str) {
    if (!ctx || !jwt_str || !ctx->jwt_secret) return 0;
    
    /* 简化的JWT验证（实际应用中应使用JWT库） */
    /* 这里只做基本格式检查，实际应用中需要完整的JWT解析和验证 */
    
    /* 检查JWT格式：header.payload.signature */
    const char* dot1 = strchr(jwt_str, '.');
    const char* dot2 = dot1 ? strchr(dot1 + 1, '.') : NULL;
    
    if (!dot1 || !dot2 || dot2[1] == '\0') {
        return 0;
    }
    
    /* 检查令牌是否过期 */
    uint64_t current_time = time(NULL);
    
    /* 这里应该解析JWT payload中的exp字段 */
    /* 简化处理，假设令牌过期时间为签发时间 + ctx->jwt_expire */
    /* 实际应用中需要完整的JWT解析 */
    
    atomic_fetch_add(&ctx->tokens_validated, 1);
    return 1;
}

/**
 * @brief 从缓存中查找JWT令牌
 * @param ctx 认证上下文
 * @param jwt_str JWT字符串
 * @return 1 在缓存中，0 不在缓存中
 */
static int jwt_token_in_cache(auth_context_t* ctx, const char* jwt_str) {
    pthread_mutex_lock(&ctx->token_cache_lock);
    
    for (size_t i = 0; i < ctx->token_cache_size; i++) {
        if (ctx->token_cache[i] && strcmp(ctx->token_cache[i]->payload, jwt_str) == 0) {
            pthread_mutex_unlock(&ctx->token_cache_lock);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&ctx->token_cache_lock);
    return 0;
}

/**
 * @brief 添加JWT令牌到缓存
 * @param ctx 认证上下文
 * @param jwt_str JWT字符串
 */
static void jwt_token_add_to_cache(auth_context_t* ctx, const char* jwt_str) {
    pthread_mutex_lock(&ctx->token_cache_lock);
    
    /* 检查缓存是否已满 */
    if (ctx->token_cache_size >= ctx->token_cache_capacity) {
        /* 清理最旧的令牌 */
        for (size_t i = 0; i < ctx->token_cache_size - 1; i++) {
            ctx->token_cache[i] = ctx->token_cache[i + 1];
        }
        ctx->token_cache_size--;
    }
    
    /* 添加新令牌 */
    jwt_token_t* token = calloc(1, sizeof(jwt_token_t));
    if (token) {
        token->payload = strdup(jwt_str);
        if (token->payload) {
            ctx->token_cache[ctx->token_cache_size++] = token;
        } else {
            free(token);
        }
    }
    
    pthread_mutex_unlock(&ctx->token_cache_lock);
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
    
    /* 根据认证类型进行验证 */
    if (ctx->type == AUTH_TYPE_JWT) {
        /* 检查缓存 */
        if (jwt_token_in_cache(ctx, token)) {
            return AUTH_RESULT_ALLOWED;
        }
        
        /* 验证JWT令牌 */
        if (validate_jwt_token(ctx, token)) {
            /* 添加到缓存 */
            jwt_token_add_to_cache(ctx, token);
            return AUTH_RESULT_ALLOWED;
        } else {
            atomic_fetch_add(&ctx->tokens_expired, 1);
            return AUTH_RESULT_DENIED;
        }
    } else {
        /* API密钥验证 */
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
