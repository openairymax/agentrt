/**
 * @file embedder.c
 * @brief L2 特征层嵌入器：支持OpenAI、DeepSeek、Sentence Transformers
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer2_feature.h"
#include "agentos.h"
#include "logger.h"
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <uthash.h>

/* 嵌入器类型 */
typedef enum {
    EMBEDDER_OPENAI,
    EMBEDDER_DEEPSEEK,
    EMBEDDER_SENTENCE_TRANSFORMERS,
    EMBEDDER_LOCAL
} embedder_type_t;


// From data intelligence emerges. by spharx
/* 内存缓冲区（用于HTTP响应） */
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

/* 全局初始化锁 */
static agentos_mutex_t* g_init_lock = NULL;

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

    char* json_data;
    asprintf(&json_data,
             "{\"input\":\"%s\",\"model\":\"text-embedding-ada-002\"}", text);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             g_embedder->api_key ? g_embedder->api_key : "");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, g_embedder->base_url ? g_embedder->base_url : "https://api.openai.com/v1/embeddings");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    CURLcode res = curl_easy_perform(curl);

    curl_free(json_data);
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

    char* json_data;
    asprintf(&json_data,
             "{\"input\":\"%s\",\"model\":\"embedding\"}", text);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             g_embedder->api_key ? g_embedder->api_key : "");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, g_embedder->base_url ? g_embedder->base_url : "https://api.deepseek.com/embeddings");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    CURLcode res = curl_easy_perform(curl);

    curl_free(json_data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        AGENTOS_FREE(chunk.data);
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
