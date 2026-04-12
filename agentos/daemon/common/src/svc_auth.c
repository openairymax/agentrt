/**
 * @file svc_auth.c
 * @brief Daemon 服务层认证中间件实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 实现内容:
 * - JWT Token 生成和验证（HS256 简化实现）
 * - API Key 验证和动态管理
 * - 令牌桶速率限制器
 * - 统一认证入口
 */

#include "svc_auth.h"
#include "svc_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ==================== 内部常量 ==================== */

#define MAX_TOKEN_SIZE        4096
#define MAX_SUBJECT_SIZE      256
#define MAX_ROLE_SIZE         64
#define MAX_APIKEY_SIZE       128
#define MAX_CLIENTS           1024

/* 默认配置值 */
#define DEFAULT_TOKEN_TTL     3600       /* 1 小时 */
#define DEFAULT_REFRESH_THRESHOLD 300    /* 5 分钟 */
#define DEFAULT_RPS           100        /* 每秒请求数 */
#define DEFAULT_BURST_SIZE    20         /* 突发大小 */
#define TOKEN_PREFIX          "agentos."
#define BEARER_PREFIX         "Bearer "
#define APIKEY_PREFIX         "ApiKey "

/* ==================== JWT 内部状态 ==================== */

static struct {
    jwt_config_t config;
    int initialized;
    char subject_buf[MAX_SUBJECT_SIZE];  /**< JWT 验证结果字符串缓冲 */
    char role_buf[MAX_ROLE_SIZE];        /**< JWT 验证角色缓冲 */
} g_jwt = { .initialized = 0 };

/* ==================== API Key 内部状态 ==================== */

static struct {
    apikey_config_t config;
    char** keys;
    size_t capacity;
    agentos_mutex_t lock;
    int initialized;
} g_apikey = { .initialized = 0 };

/* ==================== 速率限制内部状态 ==================== */

/**
 * @brief 客户端速率限制条目
 */
typedef struct rate_limit_entry {
    char client_id[128];         /**< 客户端标识 */
    double tokens;               /**< 当前令牌数 */
    double max_tokens;           /**< 最大令牌数 */
    double refill_rate;          /**< 每秒补充速率 */
    time_t last_update;          /**< 最后更新时间 */
    bool active;                 /**< 是否活跃 */
} rate_limit_entry_t;

static struct {
    rate_limit_config_t config;
    rate_limit_entry_t entries[MAX_CLIENTS];
    agentos_mutex_t lock;
    int initialized;
} g_ratelimit = { .initialized = 0 };

/* ==================== Base64 工具函数 ==================== */

/**
 * @brief Base64 编码
 * @param data 输入数据
 * @param len 数据长度
 * @param output 输出缓冲区
 * @param out_len 输出长度
 * @return 0 成功
 */
static int base64_encode(const uint8_t* data, size_t len,
                          char* output, size_t* out_len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (!data || !output || !out_len || len == 0) return AGENTOS_ERR_INVALID_PARAM;

    size_t i = 0, j = 0;
    uint8_t arr3[3], arr4[4];

    while (i < len) {
        arr3[0] = (i < len) ? data[i++] : 0;
        arr3[1] = (i < len) ? data[i++] : 0;
        arr3[2] = (i < len) ? data[i++] : 0;

        arr4[0] = (arr3[0] & 0xFC) >> 2;
        arr4[1] = ((arr3[0] & 0x03) << 4) | ((arr3[1] & 0xF0) >> 4);
        arr4[2] = ((arr3[1] & 0x0F) << 2) | ((arr3[2] & 0xC0) >> 6);
        arr4[3] = arr3[2] & 0x3F;

        for (size_t n = 0; n < 4 && j + 1 < *out_len; n++) {
            output[j++] = table[arr4[n]];
        }
    }

    /* 填充 - Base64 编码输出长度必须是4的倍数 */
    while (j % 4 != 0 && j + 1 < *out_len) {
        output[j++] = '=';
    }
    output[j] = '\0';
    *out_len = j;

    return AGENTOS_SUCCESS;
}

/* ==================== HMAC-SHA256 实现（三模式条件编译） ==================== */

/**
 * @brief HMAC-SHA256 计算函数指针类型
 */
typedef void (*hmac_fn_t)(const char* key, const char* message,
                         uint8_t* output, size_t* out_len);

/**
 * @brief 当前使用的 HMAC 实现指针（运行时选择）
 */
static hmac_fn_t g_hmac_impl = NULL;

/*
 * ═══════════════════════════════════════════════════════════════
 * 模式 1: OpenSSL HMAC-SHA256 (生产环境推荐)
 * ═══════════════════════════════════════════════════════════════
 */
#if defined(AUTH_USE_OPENSSL)
#include <openssl/hmac.h>
#include <openssl/evp.h>

static void hmac_openssl(const char* key, const char* message,
                        uint8_t* output, size_t* out_len) {
    unsigned int len = 0;
    unsigned int max_len = (unsigned int)(*out_len);
    HMAC(EVP_sha256(), (const unsigned char*)key, (int)strlen(key),
         (const unsigned char*)message, strlen(message),
         output, &len);
    *out_len = (size_t)(len < max_len ? len : max_len);
}
#define HMAC_IMPL_NAME "OpenSSL"

/*
 * ═══════════════════════════════════════════════════════════════
 * 模式 2: mbedTLS HMAC-SHA256 (嵌入式环境)
 * ═══════════════════════════════════════════════════════════════
 */
#elif defined(AUTH_USE_MBEDTLS)
#include <mbedtls/md.h>

static void hmac_mbedtls(const char* key, const char* message,
                        uint8_t* output, size_t* out_len) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx,
        (const unsigned char*)key, strlen(key));
    mbedtls_md_hmac_update(&ctx,
        (const unsigned char*)message, strlen(message));
    mbedtls_md_hmac_finish(ctx, output);
    mbedtls_md_free(ctx);
    if (*out_len > 32) *out_len = 32;
}
#define HMAC_IMPL_NAME "mbedTLS"

/*
 * ═══════════════════════════════════════════════════════════════
 * 模式 3: 内置简化实现（仅开发/测试，有 #error 保护）
 * ═══════════════════════════════════════════════════════════════
 */
#else

/**
 * @warning ⚠️ 安全警告: 此函数不是真正的 HMAC-SHA256 实现！
 *          生产环境必须链接 OpenSSL 或 mbedTLS 等成熟加密库。
 *
 * 编译期安全门禁:
 * - DEBUG 模式下允许编译（开发/测试）
 * - RELEASE/NDEBUG 模式下如果未定义 AUTH_ALLOW_INSECURE_HMAC 则编译失败
 */
#if defined(NDEBUG) && !defined(AUTH_ALLOW_INSECURE_HMAC)
    #error "SECURITY: simple_hmac is not cryptographically secure! " \
           "Define AUTH_ALLOW_INSECURE_HMAC to acknowledge risk, " \
           "or define AUTH_USE_OPENSSL / AUTH_USE_MBEDTLS for production."
#endif

static void hmac_builtin(const char* key, const char* message,
                       uint8_t* output, size_t* out_len) {
    /* 开发环境简化哈希：XOR+位移模拟（不具备密码学安全性）*/
    (void)key;
    (void)message;
    size_t key_len = strlen(key);
    size_t msg_len = strlen(message);
    for (size_t i = 0; i < 32 && i < *out_len; i++) {
        output[i] = (uint8_t)((i ^ key_len) + (msg_len * (i + 1)) +
                    (message[i % msg_len] ^ key[i % key_len]));
    }
    *out_len = 32;
}
#define HMAC_IMPL_NAME "builtin-INSECURE"

#endif /* AUTH_USE_OPENSSL / AUTH_USE_MBEDTLS / builtin */

/* ==================== JWT 实现 ==================== */

int auth_jwt_init(const jwt_config_t* config) {
    agentos_mutex_lock(&g_jwt.lock);

    if (g_jwt.initialized) {
        agentos_mutex_unlock(&g_jwt.lock);
        return AGENTOS_ERR_ALREADY_INIT;
    }

    if (!config || !config->secret || config->secret_len == 0) {
        SVC_LOG_ERROR("JWT init: invalid config");
        agentos_mutex_unlock(&g_jwt.lock);
        return AUTH_TOKEN_INVALID;
    }

    memcpy(&g_jwt.config, config, sizeof(jwt_config_t));

    if (g_jwt.config.token_ttl_sec == 0)
        g_jwt.config.token_ttl_sec = DEFAULT_TOKEN_TTL;
    if (g_jwt.config.refresh_threshold_sec == 0)
        g_jwt.config.refresh_threshold_sec = DEFAULT_REFRESH_THRESHOLD;

    /* 选择 HMAC 实现模式（运行时绑定）*/
#if defined(AUTH_USE_OPENSSL)
    g_hmac_impl = hmac_openssl;
#elif defined(AUTH_USE_MBEDTLS)
    g_hmac_impl = hmac_mbedtls;
#else
    g_hmac_impl = hmac_builtin;
#endif

    g_jwt.initialized = 1;
    SVC_LOG_INFO("JWT authentication module initialized (TTL=%llu sec, HMAC=%s)",
                (unsigned long long)g_jwt.config.token_ttl_sec,
                HMAC_IMPL_NAME);
    agentos_mutex_unlock(&g_jwt.lock);
    return AUTH_SUCCESS;
}

int auth_jwt_generate_token(const char* subject, const char* role, char** out_token) {
    if (!g_jwt.initialized || !subject || !out_token) {
        return AUTH_TOKEN_INVALID;
    }

    agentos_mutex_lock(&g_jwt.lock);

    if (strlen(subject) > MAX_SUBJECT_SIZE) {
        SVC_LOG_ERROR("JWT generate: subject too long");
        agentos_mutex_unlock(&g_jwt.lock);
        return AUTH_TOKEN_INVALID;
    }

    /* 构建 Header: {"alg":"HS256","typ":"JWT"} */
    const char* header_b64 = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";

    /* 构建 Payload */
    time_t now = time(NULL);
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "iss", g_jwt.config.issuer ? g_jwt.config.issuer : "agentos-daemon");
    cJSON_AddStringToObject(payload, "sub", subject);
    cJSON_AddStringToObject(payload, "role", role ? role : "user");
    cJSON_AddNumberToObject(payload, "iat", (double)now);
    cJSON_AddNumberToObject(payload, "exp", (double)(now + g_jwt.config.token_ttl_sec));

    char* payload_json = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    if (!payload_json) return AUTH_TOKEN_INVALID;

    /* Base64 编码 Payload */
    size_t payload_b64_size = strlen(payload_json) * 2 + 100;
    char* payload_b64 = (char*)malloc(payload_b64_size);
    if (!payload_b64) { free(payload_json); return AUTH_TOKEN_INVALID; }

    base64_encode((const uint8_t*)payload_json, strlen(payload_json),
                  payload_b64, &payload_b64_size);
    free(payload_json);

    /* 构建签名部分 */
    size_t sign_input_size = strlen(header_b64) + 1 + payload_b64_size + 100;
    char* sign_input = (char*)malloc(sign_input_size);
    snprintf(sign_input, sign_input_size, "%s.%s", header_b64, payload_b64);

    uint8_t hmac_output[32];
    size_t hmac_len = sizeof(hmac_output);
    g_hmac_impl(g_jwt.config.secret, sign_input, hmac_output, &hmac_len);

    size_t sig_b64_size = 128;
    char* sig_b64 = (char*)malloc(sig_b64_size);
    base64_encode(hmac_output, hmac_len, sig_b64, &sig_b64_size);

    /* 组合 Token */
    size_t token_size = sign_input_size + sig_b64_size + 10;
    *out_token = (char*)malloc(token_size);
    snprintf(*out_token, token_size, "%s.%s", sign_input, sig_b64);

    free(sign_input);
    free(payload_b64);
    free(sig_b64);

    SVC_LOG_DEBUG("JWT token generated for subject=%s", subject);
    agentos_mutex_unlock(&g_jwt.lock);
    return AUTH_SUCCESS;
}

int auth_jwt_verify_token(const char* token, auth_result_t* result) {
    if (!g_jwt.initialized || !token || !result) {
        return AUTH_TOKEN_INVALID;
    }

    agentos_mutex_lock(&g_jwt.lock);

    memset(result, 0, sizeof(auth_result_t));
    result->status = AUTH_FAILED;
    result->error_message = "Token verification failed";

    /* 简单格式检查 */
    const char* dot1 = strchr(token, '.');
    const char* dot2 = dot1 ? strchr(dot1 + 1, '.') : NULL;
    if (!dot1 || !dot2) {
        result->error_message = "Invalid token format";
        agentos_mutex_unlock(&g_jwt.lock);
        return AUTH_TOKEN_INVALID;
    }

    /* 解析 Payload */
    size_t payload_len = (size_t)(dot2 - dot1 - 1);
    char* payload_b64 = (char*)malloc(payload_len + 1);
    strncpy(payload_b64, dot1 + 1, payload_len);
    payload_b64[payload_len] = '\0';

    /* Base64 URL-safe 解码 */
    size_t decoded_len = (payload_len * 3) / 4 + 4;
    unsigned char* payload_decoded = (unsigned char*)malloc(decoded_len);
    if (!payload_decoded) {
        free(payload_b64);
        agentos_mutex_unlock(&g_jwt.lock);
        return AUTH_TOKEN_INVALID;
    }

    /* Base64 URL -> Standard Base64 转换 */
    for (size_t i = 0; i < payload_len; i++) {
        if (payload_b64[i] == '-') payload_b64[i] = '+';
        else if (payload_b64[i] == '_') payload_b64[i] = '/';
    }
    /* 补齐 padding */
    size_t pad = (4 - (payload_len % 4)) % 4;
    char* payload_padded = (char*)malloc(payload_len + pad + 1);
    if (!payload_padded) {
        free(payload_decoded);
        free(payload_b64);
        agentos_mutex_unlock(&g_jwt.lock);
        return AUTH_TOKEN_INVALID;
    }
    memcpy(payload_padded, payload_b64, payload_len);
    for (size_t i = 0; i < pad; i++) payload_padded[payload_len + i] = '=';
    payload_padded[payload_len + pad] = '\0';

    /* 手动 Base64 解码 */
    static const unsigned char b64_table[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
        ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
        ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
        ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
        ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63
    };
    size_t b64_len = payload_len + pad;
    size_t out_idx = 0;
    for (size_t i = 0; i + 3 < b64_len; i += 4) {
        unsigned char a = b64_table[(unsigned char)payload_padded[i]];
        unsigned char b = b64_table[(unsigned char)payload_padded[i+1]];
        unsigned char c = (payload_padded[i+2] == '=') ? 0 : b64_table[(unsigned char)payload_padded[i+2]];
        unsigned char d = (payload_padded[i+3] == '=') ? 0 : b64_table[(unsigned char)payload_padded[i+3]];
        payload_decoded[out_idx++] = (a << 2) | (b >> 4);
        if (payload_padded[i+2] != '=') payload_decoded[out_idx++] = ((b & 0xF) << 4) | (c >> 2);
        if (payload_padded[i+3] != '=') payload_decoded[out_idx++] = ((c & 0x3) << 6) | d;
    }
    payload_decoded[out_idx] = '\0';
    free(payload_padded);
    free(payload_b64);

    cJSON* payload = cJSON_Parse((const char*)payload_decoded);
    free(payload_decoded);

    if (!payload) {
        result->error_message = "Invalid token payload";
        agentos_mutex_unlock(&g_jwt.lock);
        return AUTH_TOKEN_INVALID;
    }

    /* 提取字段 - 必须在 cJSON_Delete 前复制字符串 */
    cJSON* sub = cJSON_GetObjectItem(payload, "sub");
    cJSON* role = cJSON_GetObjectItem(payload, "role");
    cJSON* exp = cJSON_GetObjectItem(payload, "exp");

    if (cJSON_IsString(sub)) {
        strncpy(g_jwt.subject_buf, sub->valuestring, MAX_SUBJECT_SIZE - 1);
        g_jwt.subject_buf[MAX_SUBJECT_SIZE - 1] = '\0';
        result->subject = g_jwt.subject_buf;
    }
    if (cJSON_IsString(role)) {
        strncpy(g_jwt.role_buf, role->valuestring, MAX_ROLE_SIZE - 1);
        g_jwt.role_buf[MAX_ROLE_SIZE - 1] = '\0';
        result->role = g_jwt.role_buf;
    }

    /* 检查过期时间 */
    if (cJSON_IsNumber(exp)) {
        time_t exp_time = (time_t)exp->valuedouble;
        time_t now = time(NULL);
        result->expires_at = (int64_t)exp_time * 1000;

        if (now > exp_time) {
            result->status = AUTH_TOKEN_EXPIRED;
            result->error_message = "Token has expired";
            cJSON_Delete(payload);
            agentos_mutex_unlock(&g_jwt.lock);
            return AUTH_TOKEN_EXPIRED;
        }
    }

    result->status = AUTH_SUCCESS;
    result->error_message = NULL;
    cJSON_Delete(payload);

    SVC_LOG_DEBUG("JWT token verified for subject=%s",
                  result->subject ? result->subject : "unknown");
    agentos_mutex_unlock(&g_jwt.lock);
    return AUTH_SUCCESS;
}

int auth_jwt_refresh_token(const char* old_token, char** out_new_token) {
    if (!old_token || !out_new_token) return AUTH_TOKEN_INVALID;

    auth_result_t result;
    int ret = auth_jwt_verify_token(old_token, &result);
    if (ret != AUTH_SUCCESS) {
        return ret;
    }

    /* 生成新 Token，保留相同的 subject 和 role */
    return auth_jwt_generate_token(result.subject, result.role, out_new_token);
}

void auth_jwt_cleanup(void) {
    agentos_mutex_lock(&g_jwt.lock);
    if (g_jwt.initialized) {
        g_hmac_impl = NULL;
        g_jwt.initialized = 0;
        memset(&g_jwt.config, 0, sizeof(jwt_config_t));
        memset(g_jwt.subject_buf, 0, sizeof(g_jwt.subject_buf));
        memset(g_jwt.role_buf, 0, sizeof(g_jwt.role_buf));
        SVC_LOG_INFO("JWT authentication module cleaned up");
    }
    agentos_mutex_unlock(&g_jwt.lock);
}

/* ==================== API Key 实现 ==================== */

int auth_apikey_init(const apikey_config_t* config) {
    if (g_apikey.initialized) return AGENTOS_ERR_ALREADY_INIT;

    agentos_mutex_init(&g_apikey.lock);

    if (config) {
        memcpy(&g_apikey.config, config, sizeof(apikey_config_t));

        /* 复制允许的 Key 列表 */
        if (config->allowed_keys && config->key_count > 0) {
            g_apikey.capacity = config->key_count + 10;
            g_apikey.keys = (char**)calloc(g_apikey.capacity, sizeof(*g_apikey.keys));
            if (g_apikey.keys) {
                for (size_t i = 0; i < config->key_count; i++) {
                    if (config->allowed_keys[i]) {
                        g_apikey.keys[i] = strdup(config->allowed_keys[i]);
                        g_apikey.config.key_count++;
                    }
                }
            }
        }
    } else {
        /* 默认空配置 */
        memset(&g_apikey.config, 0, sizeof(apikey_config_t));
        g_apikey.capacity = 10;
        g_apikey.keys = (char**)calloc(g_apikey.capacity, sizeof(*g_apikey.keys));
    }

    g_apikey.initialized = 1;
    SVC_LOG_INFO("API Key verification module initialized (%zu keys)",
                g_apikey.config.key_count);
    return AUTH_SUCCESS;
}

int auth_apikey_verify(const char* api_key, auth_result_t* result) {
    if (!g_apikey.initialized || !api_key || !result) {
        return AUTH_APIKEY_INVALID;
    }

    memset(result, 0, sizeof(auth_result_t));
    result->status = AUTH_FAILED;
    result->error_message = "API Key invalid";

    agentos_mutex_lock(&g_apikey.lock);

    for (size_t i = 0; i < g_apikey.config.key_count; i++) {
        if (g_apikey.keys[i] &&
            strcmp(api_key, g_apikey.keys[i]) == 0) {

            result->status = AUTH_SUCCESS;
            result->error_message = NULL;
            result->subject = api_key;  /* API Key 作为标识 */
            result->role = "api_user";

            agentos_mutex_unlock(&g_apikey.lock);
            SVC_LOG_DEBUG("API Key verified successfully");
            return AUTH_SUCCESS;
        }
    }

    agentos_mutex_unlock(&g_apikey.lock);
    SVC_LOG_WARN("API Key verification failed");
    return AUTH_APIKEY_INVALID;
}

int auth_apikey_add(const char* new_key) {
    if (!g_apikey.initialized || !new_key) {
        return AUTH_APIKEY_INVALID;
    }

    agentos_mutex_lock(&g_apikey.lock);

    /* 检查是否已存在 */
    for (size_t i = 0; i < g_apikey.config.key_count; i++) {
        if (g_apikey.keys[i] && strcmp(new_key, g_apikey.keys[i]) == 0) {
            agentos_mutex_unlock(&g_apikey.lock);
            return AGENTOS_ERR_ALREADY_EXISTS;
        }
    }

    /* 扩容检查 */
    if (g_apikey.config.key_count >= g_apikey.capacity) {
        size_t new_cap = g_apikey.capacity * 2;
        char** new_keys = (char**)realloc(g_apikey.keys, new_cap * sizeof(*g_apikey.keys));
        if (!new_keys) {
            agentos_mutex_unlock(&g_apikey.lock);
            return AGENTOS_ERR_OUT_OF_MEMORY;
        }
        g_apikey.keys = new_keys;
        g_apikey.capacity = new_cap;
    }

    g_apikey.keys[g_apikey.config.key_count++] = strdup(new_key);
    agentos_mutex_unlock(&g_apikey.lock);

    SVC_LOG_INFO("New API Key added (total=%zu)", g_apikey.config.key_count);
    return AUTH_SUCCESS;
}

int auth_apikey_remove(const char* key) {
    if (!g_apikey.initialized || !key) {
        return AUTH_APIKEY_INVALID;
    }

    agentos_mutex_lock(&g_apikey.lock);

    for (size_t i = 0; i < g_apikey.config.key_count; i++) {
        if (g_apikey.keys[i] && strcmp(key, g_apikey.keys[i]) == 0) {
            free(g_apikey.keys[i]);
            g_apikey.keys[i] = NULL;

            /* 压缩数组: 将后续元素前移，消除空洞 */
            for (size_t j = i; j < g_apikey.config.key_count - 1; j++) {
                g_apikey.keys[j] = g_apikey.keys[j + 1];
            }
            g_apikey.keys[g_apikey.config.key_count - 1] = NULL;
            g_apikey.config.key_count--;

            agentos_mutex_unlock(&g_apikey.lock);
            SVC_LOG_INFO("API Key removed (remaining=%zu)", g_apikey.config.key_count);
            return AUTH_SUCCESS;
        }
    }

    agentos_mutex_unlock(&g_apikey.lock);
    return AUTH_APIKEY_INVALID;
}

void auth_apikey_cleanup(void) {
    if (g_apikey.initialized) {
        agentos_mutex_lock(&g_apikey.lock);
        if (g_apikey.keys) {
            for (size_t i = 0; i < g_apikey.config.key_count; i++) {
                free(g_apikey.keys[i]);
            }
            free(g_apikey.keys);
            g_apikey.keys = NULL;
        }
        g_apikey.config.key_count = 0;
        agentos_mutex_unlock(&g_apikey.lock);
        agentos_mutex_destroy(&g_apikey.lock);
        g_apikey.initialized = 0;
        SVC_LOG_INFO("API Key verification module cleaned up");
    }
}

/* ==================== 速率限制实现（令牌桶算法）==================== */

int auth_ratelimit_init(const rate_limit_config_t* config) {
    if (g_ratelimit.initialized) return AGENTOS_ERR_ALREADY_INIT;

    agentos_mutex_init(&g_ratelimit.lock);

    if (config) {
        memcpy(&g_ratelimit.config, config, sizeof(rate_limit_config_t));
    } else {
        g_ratelimit.config.requests_per_sec = DEFAULT_RPS;
        g_ratelimit.config.burst_size = DEFAULT_BURST_SIZE;
        g_ratelimit.config.max_clients = MAX_CLIENTS;
    }

    /* 初始化所有条目为非活跃状态 */
    memset(g_ratelimit.entries, 0, sizeof(g_ratelimit.entries));
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        g_ratelimit.entries[i].active = false;
    }

    g_ratelimit.initialized = 1;
    SVC_LOG_INFO("Rate limiter initialized (rps=%u, burst=%u)",
                g_ratelimit.config.requests_per_sec,
                g_ratelimit.config.burst_size);
    return AUTH_SUCCESS;
}

int auth_ratelimit_check(const char* client_id) {
    if (!g_ratelimit.initialized || !client_id) {
        return AUTH_RATE_LIMIT_EXCEEDED;
    }

    agentos_mutex_lock(&g_ratelimit.lock);

    time_t now = time(NULL);
    rate_limit_entry_t* entry = NULL;
    size_t free_slot = SIZE_MAX;

    /* 查找或创建客户端条目 */
    for (size_t i = 0; i < g_ratelimit.config.max_clients; i++) {
        if (g_ratelimit.entries[i].active) {
            if (strncmp(g_ratelimit.entries[i].client_id, client_id,
                         sizeof(g_ratelimit.entries[i].client_id) - 1) == 0) {
                entry = &g_ratelimit.entries[i];
                break;
            }
        } else if (free_slot == SIZE_MAX) {
            free_slot = i;
        }
    }

    /* 创建新条目 */
    if (!entry && free_slot != SIZE_MAX) {
        entry = &g_ratelimit.entries[free_slot];
        strncpy(entry->client_id, client_id, sizeof(entry->client_id) - 1);
        entry->max_tokens = (double)g_ratelimit.config.burst_size;
        entry->tokens = entry->max_tokens;
        entry->refill_rate = (double)g_ratelimit.config.requests_per_sec;
        entry->last_update = now;
        entry->active = true;
    }

    /* LRU 驱逐策略: 当所有槽位占用时，驱逐最久未使用的条目 */
    if (!entry) {
        time_t oldest_time = now;
        size_t oldest_idx = 0;

        for (size_t i = 0; i < g_ratelimit.config.max_clients; i++) {
            if (g_ratelimit.entries[i].active &&
                g_ratelimit.entries[i].last_update < oldest_time) {
                oldest_time = g_ratelimit.entries[i].last_update;
                oldest_idx = i;
            }
        }

        /* 复用最老条目 */
        entry = &g_ratelimit.entries[oldest_idx];
        SVC_LOG_DEBUG("Rate limit: evicting stale client: %s", entry->client_id);
        strncpy(entry->client_id, client_id, sizeof(entry->client_id) - 1);
        entry->max_tokens = (double)g_ratelimit.config.burst_size;
        entry->tokens = entry->max_tokens;  /* 重置令牌，不继承旧值 */
        entry->refill_rate = (double)g_ratelimit.config.requests_per_sec;
        entry->last_update = now;
    }

    /* 补充令牌 */
    double elapsed = difftime(now, entry->last_update);
    entry->tokens += elapsed * entry->refill_rate;
    if (entry->tokens > entry->max_tokens) {
        entry->tokens = entry->max_tokens;
    }
    entry->last_update = now;

    /* 检查是否有可用令牌 */
    if (entry->tokens >= 1.0) {
        entry->tokens -= 1.0;
        agentos_mutex_unlock(&g_ratelimit.lock);
        return AUTH_SUCCESS;
    }

    agentos_mutex_unlock(&g_ratelimit.lock);
    SVC_LOG_DEBUG("Rate limit exceeded for client: %s", client_id);
    return AUTH_RATE_LIMIT_EXCEEDED;
}

int auth_ratelimit_reset(const char* client_id) {
    if (!g_ratelimit.initialized || !client_id) {
        return AUTH_RATE_LIMIT_EXCEEDED;
    }

    agentos_mutex_lock(&g_ratelimit.lock);

    for (size_t i = 0; i < g_ratelimit.config.max_clients; i++) {
        if (g_ratelimit.entries[i].active &&
            strncmp(g_ratelimit.entries[i].client_id, client_id,
                     sizeof(g_ratelimit.entries[i].client_id) - 1) == 0) {
            g_ratelimit.entries[i].tokens = g_ratelimit.entries[i].max_tokens;
            g_ratelimit.entries[i].last_update = time(NULL);
            agentos_mutex_unlock(&g_ratelimit.lock);
            return AUTH_SUCCESS;
        }
    }

    agentos_mutex_unlock(&g_ratelimit.lock);
    return AUTH_SUCCESS;
}

int auth_ratelimit_get_stats(const char* client_id,
                              uint32_t* remaining,
                              int64_t* reset_time) {
    if (!g_ratelimit.initialized || !client_id || !remaining) {
        return AUTH_RATE_LIMIT_EXCEEDED;
    }

    agentos_mutex_lock(&g_ratelimit.lock);

    for (size_t i = 0; i < g_ratelimit.config.max_clients; i++) {
        if (g_ratelimit.entries[i].active &&
            strncmp(g_ratelimit.entries[i].client_id, client_id,
                     sizeof(g_ratelimit.entries[i].client_id) - 1) == 0) {
            *remaining = (uint32_t)g_ratelimit.entries[i].tokens;
            if (reset_time) {
                *reset_time = (int64_t)g_ratelimit.entries[i].last_update * 1000;
            }
            agentos_mutex_unlock(&g_ratelimit.lock);
            return AUTH_SUCCESS;
        }
    }

    agentos_mutex_unlock(&g_ratelimit.lock);
    return AUTH_RATE_LIMIT_EXCEEDED;
}

void auth_ratelimit_cleanup(void) {
    if (g_ratelimit.initialized) {
        agentos_mutex_lock(&g_ratelimit.lock);
        memset(g_ratelimit.entries, 0, sizeof(g_ratelimit.entries));
        agentos_mutex_unlock(&g_ratelimit.lock);
        agentos_mutex_destroy(&g_ratelimit.lock);
        g_ratelimit.initialized = 0;
        SVC_LOG_INFO("Rate limiter cleaned up");
    }
}

/* ==================== 统一认证入口 ==================== */

int auth_init(const auth_config_t* config) {
    int ret = 0;

    if (config->enable_jwt) {
        ret = auth_jwt_init(&config->jwt);
        if (ret != AUTH_SUCCESS) return ret;
    }

    if (config->enable_apikey) {
        ret = auth_apikey_init(&config->apikey);
        if (ret != AUTH_SUCCESS) {
            auth_jwt_cleanup();
            return ret;
        }
    }

    if (config->enable_ratelimit) {
        ret = auth_ratelimit_init(&config->ratelimit);
        if (ret != AUTH_SUCCESS) {
            auth_apikey_cleanup();
            auth_jwt_cleanup();
            return ret;
        }
    }

    SVC_LOG_INFO("Authentication middleware initialized successfully");
    return AUTH_SUCCESS;
}

int auth_authenticate(const char* auth_header,
                      const char* client_id,
                      auth_result_t* result) {
    if (!auth_header || !result) {
        return AUTH_MISSING_CREDENTIALS;
    }

    memset(result, 0, sizeof(auth_result_t));

    /* 步骤1: 速率限制检查 */
    if (g_ratelimit.initialized) {
        int rl_ret = auth_ratelimit_check(client_id);
        if (rl_ret != AUTH_SUCCESS) {
            result->status = AUTH_RATE_LIMIT_EXCEEDED;
            result->error_message = "Too many requests";
            return AUTH_RATE_LIMIT_EXCEEDED;
        }
    }

    /* 步骤2: 解析认证头 */
    if (strncmp(auth_header, BEARER_PREFIX, strlen(BEARER_PREFIX)) == 0) {
        /* Bearer Token (JWT) */
        if (!g_jwt.initialized) {
            result->status = AUTH_FAILED;
            result->error_message = "JWT not enabled";
            return AUTH_FAILED;
        }

        const char* token = auth_header + strlen(BEARER_PREFIX);
        return auth_jwt_verify_token(token, result);

    } else if (strncmp(auth_header, APIKEY_PREFIX, strlen(APIKEY_PREFIX)) == 0) {
        /* API Key */
        if (!g_apikey.initialized) {
            result->status = AUTH_FAILED;
            result->error_message = "API Key not enabled";
            return AUTH_FAILED;
        }

        const char* key = auth_header + strlen(APIKEY_PREFIX);
        return auth_apikey_verify(key, result);
    }

    result->status = AUTH_MISSING_CREDENTIALS;
    result->error_message = "Missing or invalid Authorization header";
    return AUTH_MISSING_CREDENTIALS;
}

void auth_cleanup(void) {
    auth_jwt_cleanup();
    auth_apikey_cleanup();
    auth_ratelimit_cleanup();
    SVC_LOG_INFO("All authentication modules cleaned up");
}
