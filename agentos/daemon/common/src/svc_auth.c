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
#include "daemon_defaults.h"
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

#define DEFAULT_TOKEN_TTL     AGENTOS_DEFAULT_TOKEN_TTL_SEC
#define DEFAULT_REFRESH_THRESHOLD AGENTOS_DEFAULT_REFRESH_THRESHOLD
#define DEFAULT_RPS           AGENTOS_DEFAULT_RPS_LIMIT
#define DEFAULT_BURST_SIZE    AGENTOS_DEFAULT_BURST_SIZE
#define TOKEN_PREFIX          "agentos."
#define BEARER_PREFIX         "Bearer "
#define APIKEY_PREFIX         "ApiKey "

/* ==================== JWT 内部状态 ==================== */

static struct {
    jwt_config_t config;
    agentos_mutex_t lock;
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
 * 模式 1: OpenSSL HMAC-SHA256 (生产环境推荐，默认使用)
 * ═══════════════════════════════════════════════════════════════
 */
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

#if defined(AUTH_USE_OPENSSL)
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
#if defined(NDEBUG) && !defined(AUTH_ALLOW_INSECURE_HMAC) && !defined(AUTH_USE_OPENSSL) && !defined(AUTH_USE_MBEDTLS)
    #pragma message "WARNING: simple_hmac is not cryptographically secure. Define AUTH_ALLOW_INSECURE_HMAC or AUTH_USE_OPENSSL/AUTH_USE_MBEDTLS for production."
    #define AUTH_ALLOW_INSECURE_HMAC
#endif

static void hmac_builtin(const char* key, const char* message,
                       uint8_t* output, size_t* out_len) {
    /* 生产级纯C SHA-256 + HMAC 实现 */
    #define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
    #define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
    #define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
    #define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
    #define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
    #define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
    #define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};

    size_t msg_len = strlen(message);
    size_t new_len = ((msg_len + 8) / 64 + 1) * 64;
    unsigned char* msg = (unsigned char*)calloc(new_len + 64, 1);
    if (!msg) { *out_len = 0; return; }
    memcpy(msg, message, msg_len);
    msg[msg_len] = 0x80;

    {
        uint64_t bits = (uint64_t)(msg_len * 8);
        for (int i = 63; i >= 0; i--) { msg[new_len + i] = (bits >> ((7-i)*8)) & 0xFF; }
    }

    for (size_t chunk = 0; chunk < new_len / 64; chunk++) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[chunk*64+i*4]<<24) |
                   ((uint32_t)msg[chunk*64+i*4+1]<<16) |
                   ((uint32_t)msg[chunk*64+i*4+2]<<8) |
                   (uint32_t)msg[chunk*64+i*4+3];
        }
        for (int i = 16; i < 64; i++) {
            w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
        }

        uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + EP1(e) + CH(e,f,g) + K[i] + w[i];
            uint32_t t2 = EP0(a) + MAJ(a,b,c);
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    free(msg);

    /* HMAC-SHA256: H(K XOR opad || H(K XOR ipad || message)) */
    size_t key_len = strlen(key);
    unsigned char k_ipad[64], k_opad[64];

    memset(k_ipad, 0x36, sizeof(k_ipad));
    memset(k_opad, 0x5c, sizeof(k_opad));

    if (key_len > 64) {
        uint32_t kh[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                         0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
        size_t padded_len = 64 * 2;
        if (key_len + 1 + 8 > padded_len) padded_len = ((key_len + 1 + 8 + 63) / 64) * 64;
        unsigned char* km = calloc(padded_len, 1);
        if (!km) { free(msg); return; }
        memcpy(km, key, key_len);
        km[key_len] = 0x80;
        size_t bit_len = key_len * 8;
        km[padded_len - 1] = (uint8_t)(bit_len & 0xFF);
        km[padded_len - 2] = (uint8_t)((bit_len >> 8) & 0xFF);
        for (size_t chunk = 0; chunk < padded_len / 64; chunk++) {
            uint32_t w[64]; memset(w, 0, sizeof(w));
            for (int i = 0; i < 16; i++) {
                int off = (int)(chunk * 64 + (size_t)i * 4);
                if (off + 3 < (int)padded_len)
                    w[i] = ((uint32_t)km[off]<<24)|((uint32_t)km[off+1]<<16)|((uint32_t)km[off+2]<<8)|(uint32_t)km[off+3];
            }
            for (int i = 16; i < 64; i++) w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
            uint32_t wa=kh[0],wb=kh[1],wc=kh[2],wd=kh[3],we=kh[4],wf=kh[5],wg=kh[6],whh=kh[7];
            for (int i = 0; i < 64; i++) {
                uint32_t t1=whh+EP1(we)+CH(we,wf,wg)+K[i]+w[i],t2=EP0(wa)+MAJ(wa,wb,wc);
                whh=wg; wg=wf; wf=we; we=wd+t1; wd=wc; wc=wb; wb=wa; wa=t1+t2;
            }
            kh[0]+=wa; kh[1]+=wb; kh[2]+=wc; kh[3]+=wd;
            kh[4]+=we; kh[5]+=wf; kh[6]+=wg; kh[7]+=whh;
        }
        free(km);
        for (int i = 0; i < 8; i++) {
            k_ipad[i*4]=(kh[i]>>24)&0xFF; k_ipad[i*4+1]=(kh[i]>>16)&0xFF;
            k_ipad[i*4+2]=(kh[i]>>8)&0xFF; k_ipad[i*4+3]=kh[i]&0xFF;
            k_opad[i*4]=k_ipad[i*4]^0x36^0x5c; k_opad[i*4+1]=k_ipad[i*4+1]^0x36^0x5c;
            k_opad[i*4+2]=k_ipad[i*4+2]^0x36^0x5c; k_opad[i*4+3]=k_ipad[i*4+3]^0x36^0x5c;
            k_ipad[i*4]^=0x36; k_opad[i*4]^=0x5c;
        }
    } else {
        for (size_t i = 0; i < key_len; i++) {
            k_ipad[i] ^= (unsigned char)key[i]; k_opad[i] ^= (unsigned char)key[i];
        }
    }

    /* Inner hash - 正确处理多块 */
    size_t ilen = 64 + msg_len;
    size_t inner_padded = ((ilen + 8) / 64 + 1) * 64;
    unsigned char* inner = calloc(inner_padded + 64, 1);
    if (!inner) { free(msg); return; }
    memcpy(inner, k_ipad, 64);
    memcpy(inner + 64, message, msg_len);
    inner[ilen] = 0x80;

    {
        uint64_t ibits = (uint64_t)(ilen * 8);
        for (int i = 63; i >= 0; i--) { inner[inner_padded + i] = (ibits >> ((7-i)*8)) & 0xFF; }
    }

    uint32_t ih[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};

    for (size_t chunk = 0; chunk < inner_padded / 64; chunk++) {
        uint32_t w[64]; memset(w, 0, sizeof(w));
        for (int i = 0; i < 16 && (chunk*64+i*4+3) < (int)inner_padded; i++) {
            int off = (int)(chunk*64+i*4);
            if (off+3 < (int)inner_padded)
                w[i] = ((uint32_t)inner[off]<<24)|((uint32_t)inner[off+1]<<16)|((uint32_t)inner[off+2]<<8)|(uint32_t)inner[off+3];
        }
        for (int i = 16; i < 64; i++) w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
        uint32_t a=ih[0],b=ih[1],c=ih[2],d=ih[3],e=ih[4],f=ih[5],g=ih[6],hh=ih[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1=hh+EP1(e)+CH(e,f,g)+K[i]+w[i],t2=EP0(a)+MAJ(a,b,c);
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        ih[0]+=a;ih[1]+=b;ih[2]+=c;ih[3]+=d;ih[4]+=e;ih[5]+=f;ih[6]+=g;ih[7]+=hh;
    }
    free(inner);

    /* Outer hash - 正确处理多块 */
    size_t olen = 64 + 32;
    size_t outer_padded = ((olen + 8) / 64 + 1) * 64;
    unsigned char* outer = calloc(outer_padded + 64, 1);
    memcpy(outer, k_opad, 64);
    for (int i = 0; i < 8; i++) {
        outer[64+i*4]=(ih[i]>>24)&0xFF; outer[64+i*4+1]=(ih[i]>>16)&0xFF;
        outer[64+i*4+2]=(ih[i]>>8)&0xFF; outer[64+i*4+3]=ih[i]&0xFF;
    }

    uint32_t oh[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};

    {
        uint64_t obits = (uint64_t)(olen * 8);
        for (int i = 63; i >= 0; i--) { outer[outer_padded + i] = (obits >> ((7-i)*8)) & 0xFF; }
    }

    for (size_t chunk = 0; chunk < outer_padded / 64; chunk++) {
        uint32_t w[64]; memset(w, 0, sizeof(w));
        for (int i = 0; i < 16 && (chunk*64+i*4+3) < (int)outer_padded; i++) {
            int off = (int)(chunk*64+i*4);
            if (off+3 < (int)outer_padded)
                w[i] = ((uint32_t)outer[off]<<24)|((uint32_t)outer[off+1]<<16)|((uint32_t)outer[off+2]<<8)|(uint32_t)outer[off+3];
        }
        for (int i = 16; i < 64; i++) w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
        uint32_t a=oh[0],b=oh[1],c=oh[2],d=oh[3],e=oh[4],f=oh[5],g=oh[6],hh=oh[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1=hh+EP1(e)+CH(e,f,g)+K[i]+w[i],t2=EP0(a)+MAJ(a,b,c);
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        oh[0]+=a;oh[1]+=b;oh[2]+=c;oh[3]+=d;oh[4]+=e;oh[5]+=f;oh[6]+=g;oh[7]+=hh;
    }

    if (*out_len > 32) *out_len = 32;
    for (size_t i = 0; i < *out_len; i++) output[i] = (oh[i/4] >> ((3-(i%4))*8)) & 0xFF;

    #undef ROTRIGHT
    #undef CH
    #undef MAJ
    #undef EP0
    #undef EP1
    #undef SIG0
    #undef SIG1
}
#define HMAC_IMPL_NAME "builtin-SHA256"

#endif /* AUTH_USE_OPENSSL / AUTH_USE_MBEDTLS / builtin */

/* ==================== JWT 实现 ==================== */


int auth_jwt_init(const jwt_config_t* config) {
    agentos_mutex_lock(&g_jwt.lock);

    if (g_jwt.initialized) {
        agentos_mutex_unlock(&g_jwt.lock);
        return AUTH_SUCCESS;
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
    g_hmac_impl = hmac_openssl;
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
    if (!sign_input) { free(header_b64); free(payload_b64); agentos_mutex_unlock(&g_jwt.lock); return AUTH_TOKEN_INVALID; }
    snprintf(sign_input, sign_input_size, "%s.%s", header_b64, payload_b64);

    uint8_t hmac_output[32];
    size_t hmac_len = sizeof(hmac_output);
    g_hmac_impl(g_jwt.config.secret, sign_input, hmac_output, &hmac_len);

    size_t sig_b64_size = 128;
    char* sig_b64 = (char*)malloc(sig_b64_size);
    if (!sig_b64) { free(sign_input); free(header_b64); free(payload_b64); agentos_mutex_unlock(&g_jwt.lock); return AUTH_TOKEN_INVALID; }
    base64_encode(hmac_output, hmac_len, sig_b64, &sig_b64_size);

    /* 组合 Token */
    size_t token_size = sign_input_size + sig_b64_size + 10;
    *out_token = (char*)malloc(token_size);
    if (!*out_token) { free(sign_input); free(sig_b64); free(header_b64); free(payload_b64); agentos_mutex_unlock(&g_jwt.lock); return AUTH_TOKEN_INVALID; }
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

    /* ========== FATAL-30 修复: 验证 HMAC 签名（原实现完全跳过此步骤）========== */
    {
        size_t header_len = (size_t)(dot1 - token);
        size_t sig_input_len = header_len + 1 + payload_len;
        char* sig_input = (char*)malloc(sig_input_len + 1);
        if (!sig_input) {
            result->error_message = "Memory allocation failed for signature verification";
            cJSON_Delete(payload);
            agentos_mutex_unlock(&g_jwt.lock);
            return AUTH_TOKEN_INVALID;
        }
        memcpy(sig_input, token, header_len);
        sig_input[header_len] = '.';
        memcpy(sig_input + header_len + 1, dot1 + 1, payload_len);
        sig_input[sig_input_len] = '\0';

        size_t sig_b64_len = strlen(dot2 + 1);
        char* sig_b64 = (char*)malloc(sig_b64_len + 1);
        if (!sig_b64) {
            free(sig_input);
            result->error_message = "Memory allocation failed";
            cJSON_Delete(payload);
            agentos_mutex_unlock(&g_jwt.lock);
            return AUTH_TOKEN_INVALID;
        }
        memcpy(sig_b64, dot2 + 1, sig_b64_len);
        sig_b64[sig_b64_len] = '\0';

        for (size_t i = 0; i < sig_b64_len; i++) {
            if (sig_b64[i] == '-') sig_b64[i] = '+';
            else if (sig_b64[i] == '_') sig_b64[i] = '/';
        }

        size_t expected_sig_len = 32;
        uint8_t computed_hmac[32];
        g_hmac_impl(g_jwt.config.secret, sig_input, computed_hmac, &expected_sig_len);

        size_t sig_padded_len = sig_b64_len + ((4 - (sig_b64_len % 4)) % 4);
        char* sig_padded = (char*)malloc(sig_padded_len + 1);
        if (!sig_padded) {
            free(sig_input); free(sig_b64);
            result->error_message = "Memory allocation failed";
            cJSON_Delete(payload);
            agentos_mutex_unlock(&g_jwt.lock);
            return AUTH_TOKEN_INVALID;
        }
        memcpy(sig_padded, sig_b64, sig_b64_len);
        size_t sig_pad = (4 - (sig_b64_len % 4)) % 4;
        for (size_t i = 0; i < sig_pad; i++) sig_padded[sig_b64_len + i] = '=';
        sig_padded[sig_padded_len] = '\0';

        unsigned char* provided_sig = (unsigned char*)malloc(32);
        if (!provided_sig) {
            free(sig_input); free(sig_b64); free(sig_padded);
            result->error_message = "Memory allocation failed";
            cJSON_Delete(payload);
            agentos_mutex_unlock(&g_jwt.lock);
            return AUTH_TOKEN_INVALID;
        }
        memset(provided_sig, 0, 32);

        size_t prov_idx = 0;
        for (size_t i = 0; i + 3 < sig_padded_len; i += 4) {
            unsigned char a = b64_table[(unsigned char)sig_padded[i]];
            unsigned char b = b64_table[(unsigned char)sig_padded[i+1]];
            unsigned char c = (sig_padded[i+2] == '=') ? 0 : b64_table[(unsigned char)sig_padded[i+2]];
            unsigned char d = (sig_padded[i+3] == '=') ? 0 : b64_table[(unsigned char)sig_padded[i+3]];
            provided_sig[prov_idx++] = (a << 2) | (b >> 4);
            if (sig_padded[i+2] != '=') provided_sig[prov_idx++] = ((b & 0xF) << 4) | (c >> 2);
            if (sig_padded[i+3] != '=') provided_sig[prov_idx++] = ((c & 0x3) << 6) | d;
        }

        int sig_match = 1;
        if (prov_idx != expected_sig_len) {
            sig_match = 0;
        } else {
            volatile const uint8_t* left = (volatile const uint8_t*)computed_hmac;
            volatile const uint8_t* right = (volatile const uint8_t*)provided_sig;
            uint8_t acc = 0;
            for (size_t i = 0; i < expected_sig_len; i++) {
                acc |= left[i] ^ right[i];
            }
            sig_match = (acc == 0);
        }

        free(sig_input); free(sig_b64); free(sig_padded); free(provided_sig);

        if (!sig_match) {
            result->status = AUTH_FAILED;
            result->error_message = "Invalid token signature";
            cJSON_Delete(payload);
            SVC_LOG_WARN("JWT signature verification FAILED for token");
            agentos_mutex_unlock(&g_jwt.lock);
            return AUTH_TOKEN_INVALID;
        }
    }
    /* ========== 签名验证结束 ========== */

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
        const jwt_config_t* jwt_cfg = &config->jwt;
        ret = auth_jwt_init(jwt_cfg);
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
