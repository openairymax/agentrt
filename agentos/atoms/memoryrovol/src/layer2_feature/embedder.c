/**
 * @file embedder.c
 * @brief L2 特征层嵌入器：支持OpenAI、DeepSeek、Sentence Transformers
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../../include/layer2_feature.h"
#include "../../../corekern/include/agentos.h"
#include "../../include/logger.h"
#ifdef AGENTOS_HAS_CURL
#include <curl/curl.h>
#else
#include "../include/curl_stub.h"
#endif /* AGENTOS_HAS_CURL */
#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
else
/* cJSON stub - 简化实现（仅用于编译通过） */
typedef struct cJSON {
    int type;
    char* valuestring;
    struct cJSON* child;
    struct cJSON* next;
    struct cJSON* prev;
} cJSON;

#define cJSON_NULL 0
#define cJSON_False 1
#define cJSON_True 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6

static inline cJSON* cJSON_CreateObject(void) { return NULL; }
static inline void cJSON_AddStringToObject(cJSON* object, const char* name, const char* string) { (void)object; (void)name; (void)string; }
static inline char* cJSON_PrintUnformatted(const cJSON* item) { (void)item; return NULL; }
static inline void cJSON_Delete(cJSON* item) { (void)item; }
#endif /* AGENTOS_HAS_CJSON */
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <uthash.h>

/* ==================== 常量定义 ==================== */

/* JSON转义相关常量 */
#define ESCAPE_TABLE_SIZE 256
#define UNICODE_ESCAPE_LEN 6      /* \uXXXX 的长度 */
#define UNICODE_SNPRINTF_BUF 7    /* snprintf缓冲区大小（6+1） */

/* 嵌入器类型 */
typedef enum {
    EMBEDDER_OPENAI,
    EMBEDDER_DEEPSEEK,
    EMBEDDER_SENTENCE_TRANSFORMERS,
    EMBEDDER_LOCAL
} embedder_type_t;


/* 内存缓冲区（用于 HTTP 响应） */
typedef struct memory_buffer {
    char* data;
    size_t size;
} memory_buffer_t;

/* 回调函数：收集HTTP响应数据 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    memory_buffer_t* mem = (memory_buffer_t*)userp;
    char* ptr = (char*)AGENTOS_REALLOC(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0;
    }
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

/* 嵌入器实例 */
typedef struct embedder_instance {
    embedder_type_t type;
    char* api_key;
    char* base_url;
    int dimension;
    agentos_mutex_t* lock;
} embedder_instance_t;

static embedder_instance_t* g_embedder = NULL;

static agentos_mutex_t* g_init_lock = NULL;

/**
 * @brief JSON字符串转义（防止注入）
 *
 * 转义字符：", \, /, 退格, 换页, 换行, 回车, 制表符, 控制字符(0x00-0x1F)
 * @param src 原始字符串
 * @param out 输出缓冲区（需由调用者释放）
 * @return 转义后的字符串长度，失败返回 -1
 */
static ssize_t json_escape_string(const char* src, char** out) {
    if (!src || !out) return -1;

    // 转义查找表：每个字符的转义长度和转义字符串
    static const struct {
        const char* escape;  // 转义序列，NULL表示直接复制或Unicode转义
        int len;             // 转义后的长度
    } escape_table[ESCAPE_TABLE_SIZE] = {
        ['"']  = {"\\\"", 2},
        ['\\'] = {"\\\\", 2},
        ['\b'] = {"\\b", 2},
        ['\f'] = {"\\f", 2},
        ['\n'] = {"\\n", 2},
        ['\r'] = {"\\r", 2},
        ['\t'] = {"\\t", 2},
        // 控制字符(0x00-0x1F)由特殊逻辑处理，len=6, escape=NULL
    };

    size_t len = strlen(src);
    size_t escaped_len = 0;

    // 第一阶段：计算转义后的总长度
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20) {
            // 控制字符使用Unicode转义：\uXXXX (6个字符)
            escaped_len += UNICODE_ESCAPE_LEN;
        } else if (escape_table[c].len > 0) {
            // 特殊转义字符
            escaped_len += escape_table[c].len;
        } else {
            // 普通字符直接复制
            escaped_len += 1;
        }
    }

    char* escaped = (char*)AGENTOS_MALLOC(escaped_len + 1);
    if (!escaped) return -1;

    size_t pos = 0;
    // 第二阶段：填充转义后的字符串
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20) {
            // 控制字符：生成Unicode转义序列
            pos += snprintf(escaped + pos, UNICODE_SNPRINTF_BUF, "\\u%04x", c);
        } else if (escape_table[c].len > 0 && escape_table[c].escape != NULL) {
            // 特殊转义字符：复制转义序列
            const char* esc = escape_table[c].escape;
            size_t esc_len = escape_table[c].len;
            for (size_t j = 0; j < esc_len; j++) {
                escaped[pos++] = esc[j];
            }
        } else {
            // 普通字符：直接复制
            escaped[pos++] = (char)c;
        }
    }
    escaped[pos] = '\0';
    *out = escaped;
    return (ssize_t)pos;
}

/* 确保初始化 */
static void ensure_initialized(void) {
    if (g_embedder) return;

    if (!g_init_lock) {
        g_init_lock = agentos_mutex_create();
    }
    agentos_mutex_lock(g_init_lock);

    if (!g_embedder) {
        g_embedder = (embedder_instance_t*)AGENTOS_CALLOC(1, sizeof(embedder_instance_t));
        if (g_embedder) {
            g_embedder->type = EMBEDDER_OPENAI;
            g_embedder->dimension = 1536;
            g_embedder->lock = agentos_mutex_create();
        }
    }

    agentos_mutex_unlock(g_init_lock);
}

/* 生成 OpenAI 嵌入请求 */
static size_t generate_openai_embedding(const char* text, float** out_embedding) {
    ensure_initialized();

    if (!g_embedder || !text) return 0;

    CURL* curl = curl_easy_init();
    if (!curl) return 0;

    memory_buffer_t chunk = {0};
    chunk.data = (char*)AGENTOS_MALLOC(1);
    if (!chunk.data) {
        curl_easy_cleanup(curl);
        return 0;
    }

    char* escaped_text = NULL;
    if (json_escape_string(text, &escaped_text) < 0) {
        AGENTOS_FREE(chunk.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    char* json_data;
    /* 计算所需缓冲区大小：固定部分43字符 + escaped_text长度 + 1个空终止符 */
    size_t json_len = 43 + strlen(escaped_text) + 1;
    json_data = (char*)AGENTOS_MALLOC(json_len);
    if (!json_data) {
        AGENTOS_FREE(escaped_text);
        AGENTOS_FREE(chunk.data);
        curl_easy_cleanup(curl);
        return 0;
    }
    int ret = snprintf(json_data, json_len,
             "{\"input\":\"%s\",\"model\":\"text-embedding-ada-002\"}", escaped_text);
    AGENTOS_FREE(escaped_text);
    if (ret < 0 || (size_t)ret >= json_len) {
        AGENTOS_FREE(json_data);
        AGENTOS_FREE(chunk.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             g_embedder->api_key ? g_embedder->api_key : "");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, g_embedder->base_url ? g_embedder->base_url : DEFAULT_OPENAI_ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    CURLcode res = curl_easy_perform(curl);

    AGENTOS_FREE(json_data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        AGENTOS_FREE(chunk.data);
        return 0;
    }

    /* 简单解析 JSON（实际应用中应使用完整 JSON 解析器） */
    cJSON* root = cJSON_Parse(chunk.data);
    if (!root) {
        AGENTOS_FREE(chunk.data);
        return 0;
    }

    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsArray(data)) {
        cJSON_Delete(root);
        AGENTOS_FREE(chunk.data);
        return 0;
    }

    cJSON* embedding_obj = cJSON_GetArrayItem(data, 0);
    cJSON* embedding_arr = cJSON_GetObjectItem(embedding_obj, "embedding");

    size_t count = cJSON_GetArraySize(embedding_arr);
    float* emb = (float*)AGENTOS_MALLOC(count * sizeof(float));

    if (emb) {
        for (size_t i = 0; i < count; i++) {
            cJSON* val = cJSON_GetArrayItem(embedding_arr, i);
            emb[i] = (float)val->valuedouble;
        }
        *out_embedding = emb;
    }

    cJSON_Delete(root);
    AGENTOS_FREE(chunk.data);

    return emb ? count : 0;
}

/* 生成 DeepSeek 嵌入 */
static size_t generate_deepseek_embedding(const char* text, float** out_embedding) {
    ensure_initialized();

    if (!g_embedder || !text) return 0;

    CURL* curl = curl_easy_init();
    if (!curl) return 0;

    memory_buffer_t chunk = {0};
    chunk.data = (char*)AGENTOS_MALLOC(1);
    if (!chunk.data) {
        curl_easy_cleanup(curl);
        return 0;
    }

    char* escaped_text = NULL;
    if (json_escape_string(text, &escaped_text) < 0) {
        AGENTOS_FREE(chunk.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    char* json_data;
    /* 计算所需缓冲区大小：固定部分28字符 + escaped_text长度 + 1个空终止符 */
    size_t json_len = 28 + strlen(escaped_text) + 1;
    json_data = (char*)AGENTOS_MALLOC(json_len);
    if (!json_data) {
        AGENTOS_FREE(escaped_text);
        AGENTOS_FREE(chunk.data);
        curl_easy_cleanup(curl);
        return 0;
    }
    int ret = snprintf(json_data, json_len,
             "{\"input\":\"%s\",\"model\":\"embedding\"}", escaped_text);
    AGENTOS_FREE(escaped_text);
    if (ret < 0 || (size_t)ret >= json_len) {
        AGENTOS_FREE(json_data);
        AGENTOS_FREE(chunk.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             g_embedder->api_key ? g_embedder->api_key : "");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, g_embedder->base_url ? g_embedder->base_url : DEFAULT_DEEPSEEK_ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    CURLcode res = curl_easy_perform(curl);

    AGENTOS_FREE(json_data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (chunk.data) AGENTOS_FREE(chunk.data);
        return 0;
    }

    cJSON* root = cJSON_Parse(chunk.data);
    if (!root) {
        AGENTOS_FREE(chunk.data);
        return 0;
    }

    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsArray(data)) {
        cJSON_Delete(root);
        AGENTOS_FREE(chunk.data);
        return 0;
    }

    cJSON* embedding_obj = cJSON_GetArrayItem(data, 0);
    cJSON* embedding_arr = cJSON_GetObjectItem(embedding_obj, "embedding");

    size_t count = cJSON_GetArraySize(embedding_arr);
    float* emb = (float*)AGENTOS_MALLOC(count * sizeof(float));

    if (emb) {
        for (size_t i = 0; i < count; i++) {
            cJSON* val = cJSON_GetArrayItem(embedding_arr, i);
            emb[i] = (float)val->valuedouble;
        }
        *out_embedding = emb;
    }

    cJSON_Delete(root);
    AGENTOS_FREE(chunk.data);

    return emb ? count : 0;
}

/* 生成本地嵌入（随机向量，仅用于测试） */
static size_t generate_local_embedding(const char* text, float** out_embedding, int dimension) {
    if (!text || dimension <= 0) return 0;

    float* emb = (float*)AGENTOS_MALLOC(dimension * sizeof(float));
    if (!emb) return 0;

    srand((unsigned int)strlen(text));
    for (int i = 0; i < dimension; i++) {
        emb[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }

    *out_embedding = emb;
    return (size_t)dimension;
}

/* 公共 API */
agentos_error_t agentos_embedder_init(const char* api_key, const char* base_url, int dimension) {
    ensure_initialized();

    if (!g_embedder) return AGENTOS_ENOMEM;

    if (api_key) {
        if (g_embedder->api_key) AGENTOS_FREE(g_embedder->api_key);
        g_embedder->api_key = AGENTOS_STRDUP(api_key);
    }

    if (base_url) {
        if (g_embedder->base_url) AGENTOS_FREE(g_embedder->base_url);
        g_embedder->base_url = AGENTOS_STRDUP(base_url);
    }

    if (dimension > 0) {
        g_embedder->dimension = dimension;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_embedder_embed(const char* text, float** out_embedding, size_t* out_dim) {
    if (!text || !out_embedding || !out_dim) return AGENTOS_EINVAL;

    ensure_initialized();

    if (!g_embedder) return AGENTOS_ENOMEM;

    size_t dim = 0;
    float* emb = NULL;

    switch (g_embedder->type) {
        case EMBEDDER_OPENAI:
            dim = generate_openai_embedding(text, &emb);
            break;
        case EMBEDDER_DEEPSEEK:
            dim = generate_deepseek_embedding(text, &emb);
            break;
        case EMBEDDER_LOCAL:
        default:
            dim = generate_local_embedding(text, &emb, g_embedder->dimension);
            break;
    }

    if (dim == 0 || !emb) {
        return AGENTOS_FAILURE;
    }

    *out_embedding = emb;
    *out_dim = dim;

    return AGENTOS_SUCCESS;
}

void agentos_embedder_cleanup(void) {
    if (g_embedder) {
        if (g_embedder->api_key) {
            AGENTOS_FREE(g_embedder->api_key);
            g_embedder->api_key = NULL;
        }
        if (g_embedder->base_url) {
            AGENTOS_FREE(g_embedder->base_url);
            g_embedder->base_url = NULL;
        }
        if (g_embedder->lock) {
            agentos_mutex_destroy(g_embedder->lock);
            g_embedder->lock = NULL;
        }
        AGENTOS_FREE(g_embedder);
        g_embedder = NULL;
    }

    if (g_init_lock) {
        agentos_mutex_destroy(g_init_lock);
        g_init_lock = NULL;
    }
}
