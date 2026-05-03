/**
 * @file license.c
 * @brief MemoryRovol License 离线验证实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现基于 RSA-4096 公钥的许可证离线验证系统。
 * 支持 4 级分层授权（Trial/Pro/Enterprise/Enterprise+）。
 * 通过 __attribute__((constructor)) 在 main() 前自动初始化。
 *
 * 安全特性：
 * - 全程无网络调用，数据始终在本地
 * - RSA-4096 SHA-256 签名验证（OpenSSL EVP API）
 * - 时间倒流检测（防止系统时间回拨）
 * - CRC 防篡改校验
 * - 无环境变量绕过（fail-closed 安全模型）
 */

#include "../include/license.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef AGENTOS_USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

static agentos_license_status_t g_license_status;
static int g_license_initialized = 0;

#ifdef _WIN32
static CRITICAL_SECTION g_license_cs;
#else
static pthread_mutex_t g_license_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static const unsigned char g_rsa_public_key[] = {
    0x30, 0x82, 0x02, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x02, 0x0f, 0x00,
    0x30, 0x82, 0x02, 0x0a, 0x02, 0x82, 0x02, 0x01, 0x00, 0xbc, 0x55, 0x23,
    0x33, 0x2b, 0x13, 0xd3, 0x5c, 0x68, 0x0e, 0x4e, 0x24, 0x18, 0xd1, 0x06,
    0x91, 0x0e, 0x5d, 0x1a, 0x7c, 0xbc, 0x5a, 0x3a, 0xec, 0x92, 0xcd, 0x28,
    0x9a, 0xc8, 0x41, 0xac, 0xe5, 0x0e, 0x1b, 0xab, 0x84, 0xaa, 0x4c, 0xff,
    0xbf, 0x20, 0x55, 0xb4, 0x08, 0x78, 0xdb, 0x7d, 0x58, 0x6a, 0x09, 0x73,
    0xe8, 0xd4, 0x67, 0x37, 0x10, 0xaa, 0xe6, 0x6f, 0xdd, 0xf6, 0xa0, 0xc8,
    0x06, 0xc0, 0x97, 0x9a, 0x7b, 0x4d, 0x7a, 0x26, 0xac, 0xbe, 0xf6, 0x47,
    0xe1, 0xad, 0x70, 0x92, 0x8b, 0x45, 0x10, 0x5a, 0x72, 0x5a, 0xe0, 0xa3,
    0xb3, 0xea, 0x1f, 0xbc, 0xc8, 0x54, 0x36, 0xa7, 0x7c, 0x43, 0x04, 0xd9,
    0x82, 0xa8, 0x03, 0xee, 0x75, 0xd1, 0xc7, 0x01, 0x06, 0x95, 0xda, 0x5e,
    0x66, 0xbe, 0xa5, 0x6a, 0xb7, 0x0e, 0x37, 0x40, 0xbf, 0xf5, 0x0f, 0xf9,
    0xf6, 0x09, 0x4b, 0xca, 0x66, 0xd3, 0x2c, 0xa2, 0xe2, 0xad, 0xf9, 0xad,
    0x83, 0x1b, 0x92, 0x28, 0xd8, 0xd3, 0xa7, 0xf7, 0x1d, 0xa0, 0x86, 0xef,
    0xeb, 0x13, 0x57, 0x5a, 0xbe, 0x0f, 0x18, 0xa7, 0x6e, 0x52, 0xc9, 0xd9,
    0x79, 0xae, 0x2e, 0xee, 0xe0, 0xc9, 0x97, 0xe7, 0x96, 0x5c, 0xa0, 0x92,
    0x7e, 0x5a, 0x4e, 0xe9, 0xc8, 0x36, 0x3f, 0x01, 0x27, 0xd5, 0xb3, 0x33,
    0x30, 0x4e, 0x40, 0xb8, 0x1d, 0x1d, 0xee, 0x88, 0xed, 0xdc, 0xba, 0xb1,
    0x83, 0x3a, 0x84, 0xed, 0x2d, 0x44, 0xd2, 0xc9, 0xb8, 0xc7, 0x5f, 0x6e,
    0x3b, 0x74, 0x67, 0x17, 0xf3, 0xd1, 0xc1, 0x41, 0xdb, 0xac, 0x25, 0x9d,
    0x0b, 0xe3, 0x8a, 0x63, 0xeb, 0xec, 0xed, 0x77, 0xd0, 0xfc, 0x2f, 0x3e,
    0xa4, 0xac, 0x2b, 0x99, 0x38, 0xad, 0x64, 0x4f, 0xdd, 0x5c, 0xf0, 0x81,
    0x9c, 0xb0, 0x36, 0x97, 0x08, 0x21, 0x71, 0x51, 0x14, 0xf5, 0x7b, 0x25,
    0x24, 0x23, 0x2d, 0xe8, 0x2f, 0x62, 0x06, 0x9a, 0xf5, 0xd1, 0xe4, 0x21,
    0xb2, 0xd8, 0xfc, 0x4f, 0x98, 0xbf, 0x42, 0x22, 0x3a, 0x68, 0x33, 0x96,
    0xeb, 0xaa, 0x66, 0x61, 0x60, 0x83, 0x1a, 0xb7, 0x32, 0xbb, 0x51, 0x89,
    0x90, 0x27, 0x54, 0x46, 0xab, 0xce, 0x0c, 0x03, 0xf8, 0xbf, 0xfd, 0x29,
    0x3b, 0x27, 0x83, 0x2d, 0xbe, 0x2a, 0x2f, 0x35, 0xee, 0xea, 0x3e, 0xe8,
    0x37, 0xad, 0x03, 0x81, 0xe6, 0x14, 0xda, 0x3e, 0x32, 0xb2, 0x4c, 0xff,
    0x96, 0x75, 0x1f, 0x63, 0x8d, 0x76, 0xfc, 0x3b, 0xc6, 0x51, 0x54, 0x3d,
    0x55, 0xfe, 0x5f, 0xb1, 0x7b, 0xe4, 0x6f, 0x25, 0xa4, 0x24, 0x2d, 0x12,
    0x6e, 0xf6, 0xc8, 0x1b, 0xab, 0xf5, 0x12, 0x96, 0x6d, 0xd9, 0x44, 0x8a,
    0x85, 0x69, 0x3a, 0x28, 0x7a, 0x0d, 0x4f, 0x1a, 0x66, 0xb9, 0x30, 0xbe,
    0x1a, 0xcc, 0x59, 0x4e, 0x4b, 0x26, 0x33, 0x05, 0x26, 0x90, 0x47, 0xff,
    0xc8, 0x79, 0x39, 0xbd, 0xa1, 0x6b, 0xb7, 0x66, 0x94, 0x4a, 0x57, 0x10,
    0xa8, 0x80, 0x25, 0x7a, 0x6d, 0x6b, 0xa6, 0x3f, 0x32, 0xa3, 0xb0, 0xff,
    0x22, 0x69, 0x51, 0xdb, 0x93, 0x4a, 0x47, 0x36, 0x4f, 0xe7, 0xe3, 0x84,
    0xcf, 0x69, 0xf5, 0x69, 0xef, 0xa1, 0x5d, 0xc9, 0xd1, 0xcd, 0x00, 0x72,
    0x45, 0x68, 0x13, 0x6c, 0x9d, 0xcd, 0xd3, 0xcf, 0x96, 0xe1, 0x6d, 0x29,
    0xd2, 0x75, 0x62, 0xa9, 0xc3, 0xc4, 0x2e, 0x0d, 0xcd, 0xd9, 0x0e, 0xde,
    0xba, 0x51, 0xfb, 0xc7, 0x5d, 0xfe, 0x88, 0x26, 0x12, 0x82, 0x31, 0x99,
    0x55, 0xdb, 0xf5, 0xc7, 0x9b, 0x49, 0xbd, 0xc0, 0x28, 0x87, 0x17, 0xe4,
    0x29, 0x96, 0x5d, 0x7b, 0x5a, 0x4f, 0x63, 0x2c, 0x0c, 0xb9, 0xa6, 0x84,
    0xf1, 0xe7, 0xbd, 0xed, 0xd3, 0x65, 0xd0, 0x3a, 0x5f, 0xca, 0x27, 0xa6,
    0x69, 0xbb, 0x1e, 0xc1, 0xeb, 0x02, 0x03, 0x01, 0x00, 0x01
};
static const size_t g_rsa_public_key_len = sizeof(g_rsa_public_key);

static const char* get_default_license_path(void) {
    const char* env_path = getenv("MEMORYROVOL_LICENSE");
    if (env_path && *env_path) return env_path;
#ifdef _WIN32
    return "C:\\ProgramData\\MemoryRovol\\license.lic";
#else
    return "/opt/memoryrovol/license.lic";
#endif
}

static uint32_t crc32_compute(const unsigned char* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

static size_t base64_decode(const char* src, size_t src_len,
                            unsigned char* dst, size_t dst_size) {
    static const unsigned char dtable[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
        ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
        ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
        ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
        ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63
    };

    size_t out = 0;
    unsigned int buf = 0;
    int bits = 0;

    for (size_t i = 0; i < src_len && out < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        if (dtable[c] == 0 && c != 'A') break;

        buf = (buf << 6) | dtable[c];
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            if (out < dst_size) {
                dst[out++] = (unsigned char)(buf >> bits) & 0xFF;
            }
        }
    }

    return out;
}

#ifdef AGENTOS_USE_OPENSSL

static int license_openssl_verify(const char* json_data,
                                  const unsigned char* signature,
                                  size_t sig_len) {
    if (!json_data || !signature || sig_len == 0) return 0;

    EVP_PKEY* pkey = NULL;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
    if (!ctx) return 0;

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("input-type", (char*)"DER", 0),
        OSSL_PARAM_construct_octet_string("pub", (void*)g_rsa_public_key,
                                          g_rsa_public_key_len),
        OSSL_PARAM_END
    };

    if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
        EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }
    EVP_PKEY_CTX_free(ctx);

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pkey);
        return 0;
    }

    int result = 0;
    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey) == 1 &&
        EVP_DigestVerifyUpdate(mdctx, json_data, strlen(json_data)) == 1) {
        result = (EVP_DigestVerifyFinal(mdctx, signature, sig_len) == 1);
    }

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    return result;
}

#endif

static int verify_signature(const char* json_data,
                           const unsigned char* signature_base64,
                           size_t sig_len) {
    if (!json_data || !signature_base64 || sig_len == 0) return 0;

#ifdef AGENTOS_USE_OPENSSL
    unsigned char sig_bytes[512];
    size_t decoded_len = base64_decode((const char*)signature_base64, sig_len,
                                       sig_bytes, sizeof(sig_bytes));
    if (decoded_len == 0) return 0;

    return license_openssl_verify(json_data, sig_bytes, decoded_len);
#else
    (void)base64_decode;
    return 0;
#endif
}

static int parse_license_json(const char* json_content,
                              agentos_license_status_t* status) {

    if (!json_content || !status) return -1;

    memset(status, 0, sizeof(*status));
    status->level = AGENTOS_LICENSE_INVALID;
    status->valid = 0;

    const char* p = json_content;

    while (*p && *p != '{') p++;
    if (*p != '{') return -1;

    char license_id[64] = {0};
    char customer[128] = {0};
    char expires_str[32] = {0};
    char level_str[16] = {0};

    const char* fields[] = {"license_id", "customer", "expires_at", "type"};
    char* targets[] = {license_id, customer, expires_str, level_str};
    size_t sizes[] = {sizeof(license_id), sizeof(customer), sizeof(expires_str), sizeof(level_str)};

    for (int f = 0; f < 4; f++) {
        const char* key = fields[f];
        const char* found = strstr(p, key);
        if (!found) continue;

        const char* colon = strchr(found, ':');
        if (!colon) continue;

        const char* val_start = colon + 1;
        while (*val_start && (*val_start == ' ' || *val_start == '\t' || *val_start == '"'))
            val_start++;

        const char* val_end = val_start;
        while (*val_end && *val_end != '"' && *val_end != ',' && *val_end != '}' && *val_end != '\n')
            val_end++;

        size_t len = (size_t)(val_end - val_start);
        if (len >= sizes[f]) len = sizes[f] - 1;
        memcpy(targets[f], val_start, len);
        targets[f][len] = '\0';
    }

    strncpy(status->license_id, license_id, sizeof(status->license_id) - 1);
    strncpy(status->customer, customer, sizeof(status->customer) - 1);

    if (strlen(expires_str) > 0) {
        status->expires_at = (time_t)atol(expires_str);
    } else {
        status->expires_at = 0;
    }

    if (strstr(level_str, "enterprise_plus") || strstr(level_str, "enterprise+")) {
        status->level = AGENTOS_LICENSE_ENTERPRISE_PLUS;
    } else if (strstr(level_str, "enterprise")) {
        status->level = AGENTOS_LICENSE_ENTERPRISE;
    } else if (strstr(level_str, "pro")) {
        status->level = AGENTOS_LICENSE_PRO;
    } else if (strstr(level_str, "trial") || strlen(level_str) == 0) {
        status->level = AGENTOS_LICENSE_TRIAL;
    } else {
        status->level = AGENTOS_LICENSE_INVALID;
    }

    switch (status->level) {
        case AGENTOS_LICENSE_TRIAL:
            status->feature_flags = AGENTOS_FEATURE_L1_RAW | AGENTOS_FEATURE_L2_FEATURE;
            break;
        case AGENTOS_LICENSE_PRO:
            status->feature_flags = AGENTOS_FEATURE_L1_RAW | AGENTOS_FEATURE_L2_FEATURE |
                                   AGENTOS_FEATURE_L3_STRUCTURE | AGENTOS_FEATURE_FORGETTING;
            break;
        case AGENTOS_LICENSE_ENTERPRISE:
            status->feature_flags = AGENTOS_FEATURE_L1_RAW | AGENTOS_FEATURE_L2_FEATURE |
                                   AGENTOS_FEATURE_L3_STRUCTURE | AGENTOS_FEATURE_L4_PATTERN |
                                   AGENTOS_FEATURE_FORGETTING | AGENTOS_FEATURE_ATTRACTOR |
                                   AGENTOS_FEATURE_PERSISTENCE | AGENTOS_FEATURE_ASYNC;
            break;
        case AGENTOS_LICENSE_ENTERPRISE_PLUS:
            status->feature_flags = 0xFFFFFFFFu;
            break;
        default:
            status->feature_flags = AGENTOS_FEATURE_L1_RAW;
            break;
    }

    return 0;
}

static int do_license_verify(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 64 * 1024) {
        fclose(fp);
        return -2;
    }

    char* content = (char*)AGENTOS_MALLOC((size_t)fsize + 1);
    if (!content) {
        fclose(fp);
        return -3;
    }

    size_t nread = fread(content, 1, (size_t)fsize, fp);
    content[nread] = '\0';
    fclose(fp);

    const char* sig_marker = "\"signature\":\"";
    const char* sig_start = strstr(content, sig_marker);
    if (!sig_start) {
        AGENTOS_FREE(content);
        return -6;
    }

    sig_start += strlen(sig_marker);
    const char* sig_end = strchr(sig_start, '"');
    if (!sig_end) {
        AGENTOS_FREE(content);
        return -6;
    }

    size_t sig_b64_len = (size_t)(sig_end - sig_start);
    unsigned char* sig_b64 = (unsigned char*)AGENTOS_MALLOC(sig_b64_len + 1);
    if (!sig_b64) {
        AGENTOS_FREE(content);
        return -3;
    }
    memcpy(sig_b64, sig_start, sig_b64_len);
    sig_b64[sig_b64_len] = '\0';

    char* json_end = strstr(content, ",\"signature\"");
    if (!json_end) json_end = strrchr(content, '}');
    size_t json_data_len = json_end ? (size_t)(json_end - content) : nread;

    char* json_data = (char*)AGENTOS_MALLOC(json_data_len + 1);
    if (!json_data) {
        AGENTOS_FREE(sig_b64);
        AGENTOS_FREE(content);
        return -3;
    }
    memcpy(json_data, content, json_data_len);
    json_data[json_data_len] = '\0';

    const char* crc_marker = "\"crc32\":\"";
    const char* crc_start = strstr(content, crc_marker);
    if (crc_start && crc_start < sig_start) {
        crc_start += strlen(crc_marker);
        const char* crc_end = strchr(crc_start, '"');
        if (crc_end) {
            size_t crc_str_len = (size_t)(crc_end - crc_start);
            char crc_str[16] = {0};
            if (crc_str_len < sizeof(crc_str)) {
                memcpy(crc_str, crc_start, crc_str_len);
                uint32_t expected_crc = (uint32_t)strtoul(crc_str, NULL, 16);
                uint32_t actual_crc = crc32_compute((const unsigned char*)json_data, json_data_len);
                if (actual_crc != expected_crc) {
                    AGENTOS_FREE(json_data);
                    AGENTOS_FREE(sig_b64);
                    AGENTOS_FREE(content);
                    return -8;
                }
            }
        }
    }

    int sig_result = verify_signature(json_data, sig_b64, sig_b64_len);

    AGENTOS_FREE(json_data);
    AGENTOS_FREE(sig_b64);

    if (!sig_result) {
        AGENTOS_FREE(content);
        return -7;
    }

    agentos_license_status_t temp_status;
    int parse_err = parse_license_json(content, &temp_status);
    AGENTOS_FREE(content);

    if (parse_err != 0) return parse_err;

    time_t now = time(NULL);

    if (temp_status.expires_at > 0 && temp_status.expires_at < now) {
        return -4;
    }

    static time_t s_last_check = 0;
    if (s_last_check > 0 && now < s_last_check - 300) {
        return -5;
    }
    s_last_check = now;

    memcpy(&g_license_status, &temp_status, sizeof(g_license_status));
    g_license_status.valid = 1;
    time(&g_license_status.checked_at);

    return 0;
}

int agentos_license_init(const char* license_path) {
#ifdef _WIN32
    InitializeCriticalSection(&g_license_cs);
#endif

    const char* path = license_path ? license_path : get_default_license_path();
    int result = do_license_verify(path);

    if (result != 0) {
        memset(&g_license_status, 0, sizeof(g_license_status));
        g_license_status.valid = 0;
        g_license_status.level = AGENTOS_LICENSE_TRIAL;
        g_license_status.feature_flags = AGENTOS_FEATURE_L1_RAW | AGENTOS_FEATURE_L2_FEATURE;
        strncpy(g_license_status.license_id, "UNLICENSED", sizeof(g_license_status.license_id) - 1);
        time(&g_license_status.checked_at);
    }

    g_license_initialized = 1;
    return result;
}

void agentos_license_shutdown(void) {
    g_license_initialized = 0;
    memset(&g_license_status, 0, sizeof(g_license_status));
#ifdef _WIN32
    DeleteCriticalSection(&g_license_cs);
#endif
}

const agentos_license_status_t* agentos_license_get_status(void) {
    if (!g_license_initialized) return NULL;
    return &g_license_status;
}

int agentos_license_check_feature(uint32_t feature_flag) {
    if (!g_license_initialized) return 0;
    return (g_license_status.feature_flags & feature_flag) ? 1 : 0;
}

int agentos_license_is_trial(void) {
    if (!g_license_initialized) return 1;
    return (g_license_status.level == AGENTOS_LICENSE_TRIAL) ? 1 : 0;
}

static uint32_t g_trial_write_count = 0;
static time_t g_trial_first_run = 0;

static const char* get_trial_marker_path(void) {
#ifdef _WIN32
    return "C:\\ProgramData\\MemoryRovol\\.trial_marker";
#else
    return "/opt/memoryrovol/.trial_marker";
#endif
}

static void load_trial_marker(void) {
    if (g_trial_first_run > 0) return;
    const char* path = get_trial_marker_path();
    FILE* fp = fopen(path, "r");
    if (fp) {
        char buf[32] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            g_trial_first_run = (time_t)strtoull(buf, NULL, 10);
        }
        fclose(fp);
    }
    if (g_trial_first_run == 0) {
        g_trial_first_run = time(NULL);
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "%llu", (unsigned long long)g_trial_first_run);
            fclose(fp);
        }
    }
}

int agentos_license_trial_expired(void) {
    if (!agentos_license_is_trial()) return 0;
    load_trial_marker();
    time_t now = time(NULL);
    double elapsed_days = difftime(now, g_trial_first_run) / 86400.0;
    return (elapsed_days > AGENTOS_TRIAL_DURATION_DAYS) ? 1 : 0;
}

uint32_t agentos_license_trial_record_count(void) {
    return g_trial_write_count;
}

int agentos_license_trial_check_write(void) {
    if (!agentos_license_is_trial()) return 0;
    if (agentos_license_trial_expired()) return -1;
    if (g_trial_write_count >= AGENTOS_TRIAL_MAX_RECORDS) return -2;
    g_trial_write_count++;
    return 0;
}

void agentos_license_trial_delay(void) {
    if (!agentos_license_is_trial()) return;
#ifdef _WIN32
    Sleep(AGENTOS_TRIAL_DELAY_SEC * 1000);
#else
    struct timespec ts;
    ts.tv_sec = AGENTOS_TRIAL_DELAY_SEC;
    ts.tv_nsec = 0;
    nanosleep(&ts, NULL);
#endif
}

int agentos_license_reload(const char* license_path) {
    const char* path = license_path ? license_path : get_default_license_path();
    return do_license_verify(path);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor(101)))
static void license_auto_init(void) {
    agentos_license_init(NULL);
}

__attribute__((destructor(101)))
static void license_auto_cleanup(void) {
    agentos_license_shutdown();
}
#elif defined(_WIN32)
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    (void)hinst; (void)reserved;
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            agentos_license_init(NULL);
            break;
        case DLL_PROCESS_DETACH:
            agentos_license_shutdown();
            break;
    }
    return TRUE;
}
#endif
