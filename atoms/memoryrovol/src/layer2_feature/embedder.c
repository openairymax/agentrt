/**
 * @file embedder.c
 * @brief L2 特征层嵌入器：支�?OpenAI、DeepSeek、Sentence Transformers
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

/* 嵌入器类�?*/
typedef enum {
    EMBEDDER_OPENAI,
    EMBEDDER_DEEPSEEK,
    EMBEDDER_SENTENCE_TRANSFORMERS,
    EMBEDDER_LOCAL
} embedder_type_t;


// From data intelligence emerges. by spharx
/* 内存缓冲区（用于HTTP响应�?*/
typedef struct memory_buffer {
    char* data;
    size_t size;
} memory_buffer_t;

/* 回调函数：收集HTTP响应数据 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    memory_buffer_t* mem = (memory_buffer_t*)userp;
    char* ptr = AGENTOS_REALLOC(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->data = ptr;
    memcpy(&mem->data[mem->size], contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}

/* 嵌入器句柄结�?*/
typedef struct embedder_handle {
    embedder_type_t type;
    char* model_name;
    char* api_key;
    char* api_base;
    size_t dimension;
    // 本地模型句柄（预留）
    void* local_model;
} embedder_handle_t;

/* ========== 嵌入函数声明 ========== */
static agentos_error_t embed_openai(embedder_handle_t* h, const char* text, float** out_vec, size_t* out_dim);
static agentos_error_t embed_deepseek(embedder_handle_t* h, const char* text, float** out_vec, size_t* out_dim);
static agentos_error_t embed_sentence_transformers(embedder_handle_t* h, const char* text, float** out_vec, size_t* out_dim);

/* ========== 嵌入器创�?销�?========== */
static embedder_handle_t* embedder_create(const agentos_layer2_feature_config_t* manager) {
    if (!manager || !manager->embedding_model) return NULL;

    embedder_handle_t* h = AGENTOS_CALLOC(1, sizeof(embedder_handle_t));
    if (!h) return NULL;

    h->model_name = AGENTOS_STRDUP(manager->embedding_model);
    if (manager->api_key) h->api_key = AGENTOS_STRDUP(manager->api_key);
    if (manager->api_base) h->api_base = AGENTOS_STRDUP(manager->api_base);

    // 根据模型名判断类�?
    if (strstr(manager->embedding_model, "text-embedding-") == manager->embedding_model) {
        h->type = EMBEDDER_OPENAI;
        if (manager->dimension > 0) {
            h->dimension = manager->dimension;
        } else if (strstr(manager->embedding_model, "3-small")) {
            h->dimension = 1536;
        } else if (strstr(manager->embedding_model, "3-large")) {
            h->dimension = 3072;
        } else {
            h->dimension = 1536;
        }
    } else if (strstr(manager->embedding_model, "deepseek") != NULL ||
               strstr(manager->embedding_model, "DeepSeek") != NULL) {
        h->type = EMBEDDER_DEEPSEEK;
        h->dimension = manager->dimension > 0 ? manager->dimension : 1024;
    } else if (strstr(manager->embedding_model, "sentence-transformers/") == manager->embedding_model ||
               strstr(manager->embedding_model, "all-") == manager->embedding_model ||
               strstr(manager->embedding_model, "multi-") == manager->embedding_model) {
        h->type = EMBEDDER_SENTENCE_TRANSFORMERS;
        h->dimension = manager->dimension > 0 ? manager->dimension : 384;
    } else {
        h->type = EMBEDDER_LOCAL;
        h->dimension = manager->dimension > 0 ? manager->dimension : 384;
    }
    return h;
}

static void embedder_destroy(embedder_handle_t* h) {
    if (!h) return;
    AGENTOS_FREE(h->model_name);
    AGENTOS_FREE(h->api_key);
    AGENTOS_FREE(h->api_base);
    // 释放本地模型（如果加载）
    AGENTOS_FREE(h);
}

/* ========== OpenAI 嵌入实现 ========== */
static agentos_error_t embed_openai(embedder_handle_t* h, const char* text, float** out_vec, size_t* out_dim) {
    if (!h->api_key) {
        AGENTOS_LOG_ERROR("OpenAI API key not set");
        return AGENTOS_EPERM;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        AGENTOS_LOG_ERROR("Failed to initialize CURL");
        return AGENTOS_ENOMEM;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", h->model_name);
    cJSON_AddStringToObject(root, "input", text);
    if (h->dimension > 0) {
        cJSON_AddNumberToObject(root, "dimensions", h->dimension);
    }
    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[256];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", h->api_key);
    headers = curl_slist_append(headers, auth);

    const char* base = h->api_base ? h->api_base : "https://api.openai.com/v1";
    char url[512];
    snprintf(url, sizeof(url), "%s/embeddings", base);

    memory_buffer_t resp = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    AGENTOS_FREE(json_str);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !resp.data) {
        if (resp.data) AGENTOS_FREE(resp.data);
        AGENTOS_LOG_ERROR("CURL error: %d", res);
        return AGENTOS_EIO;
    }

    cJSON* json = cJSON_Parse(resp.data);
    AGENTOS_FREE(resp.data);
    if (!json) {
        AGENTOS_LOG_ERROR("Failed to parse JSON response");
        return AGENTOS_EINVAL;
    }

    cJSON* data = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) {
        cJSON_Delete(json);
        AGENTOS_LOG_ERROR("Invalid response format: missing data array");
        return AGENTOS_EINVAL;
    }
    cJSON* first = cJSON_GetArrayItem(data, 0);
    cJSON* embedding = cJSON_GetObjectItem(first, "embedding");
    if (!cJSON_IsArray(embedding)) {
        cJSON_Delete(json);
        AGENTOS_LOG_ERROR("Invalid response format: missing embedding");
        return AGENTOS_EINVAL;
    }

    size_t dim = cJSON_GetArraySize(embedding);
    float* vec = (float*)AGENTOS_MALLOC(dim * sizeof(float));
    if (!vec) {
        cJSON_Delete(json);
        return AGENTOS_ENOMEM;
    }
    for (size_t i = 0; i < dim; i++) {
        cJSON* item = cJSON_GetArrayItem(embedding, i);
        vec[i] = (float)cJSON_GetNumberValue(item);
    }

    cJSON_Delete(json);
    *out_vec = vec;
    *out_dim = dim;
    return AGENTOS_SUCCESS;
}

/* ========== DeepSeek 嵌入实现 ========== */
static agentos_error_t embed_deepseek(embedder_handle_t* h, const char* text, float** out_vec, size_t* out_dim) {
    if (!h->api_key) {
        AGENTOS_LOG_ERROR("DeepSeek API key not set");
        return AGENTOS_EPERM;
    }
    CURL* curl = curl_easy_init();
    if (!curl) return AGENTOS_ENOMEM;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", h->model_name);
    cJSON_AddStringToObject(root, "input", text);
    cJSON_AddStringToObject(root, "encoding_format", "float");
    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[256];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", h->api_key);
    headers = curl_slist_append(headers, auth);

    const char* base = h->api_base ? h->api_base : "https://api.deepseek.com/v1";
    char url[512];
    snprintf(url, sizeof(url), "%s/embeddings", base);

    memory_buffer_t resp = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    AGENTOS_FREE(json_str);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !resp.data) {
        if (resp.data) AGENTOS_FREE(resp.data);
        return AGENTOS_EIO;
    }

    cJSON* json = cJSON_Parse(resp.data);
    AGENTOS_FREE(resp.data);
    if (!json) return AGENTOS_EINVAL;

    cJSON* data = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) {
        cJSON_Delete(json);
        return AGENTOS_EINVAL;
    }
    cJSON* first = cJSON_GetArrayItem(data, 0);
    cJSON* embedding = cJSON_GetObjectItem(first, "embedding");
    if (!cJSON_IsArray(embedding)) {
        cJSON_Delete(json);
        return AGENTOS_EINVAL;
    }

    size_t dim = cJSON_GetArraySize(embedding);
    float* vec = (float*)AGENTOS_MALLOC(dim * sizeof(float));
    if (!vec) {
        cJSON_Delete(json);
        return AGENTOS_ENOMEM;
    }
    for (size_t i = 0; i < dim; i++) {
        cJSON* item = cJSON_GetArrayItem(embedding, i);
        vec[i] = (float)cJSON_GetNumberValue(item);
    }

    cJSON_Delete(json);
    *out_vec = vec;
    *out_dim = dim;
    return AGENTOS_SUCCESS;
}

/* ========== Sentence Transformers 嵌入实现（需链接库） ========== */
#ifdef HAVE_SENTENCE_TRANSFORMERS
#include <sentence_transformers.h>
static agentos_error_t embed_sentence_transformers(embedder_handle_t* h, const char* text, float** out_vec, size_t* out_dim) {
    if (!h->local_model) {
        h->local_model = sentence_transformers_load(h->model_name);
        if (!h->local_model) {
            AGENTOS_LOG_ERROR("Failed to load Sentence Transformers model %s", h->model_name);
            return AGENTOS_ENOENT;
        }
    }
    size_t dim;
    float* vec = sentence_transformers_encode(h->local_model, text, &dim);
    if (!vec) return AGENTOS_EIO;
    *out_vec = vec;
    *out_dim = dim;
    return AGENTOS_SUCCESS;
}
#else
static agentos_error_t embed_sentence_transformers(embedder_handle_t* h, const char* text, float** out_vec, size_t* out_dim) {
    AGENTOS_LOG_ERROR("Sentence Transformers support not compiled");
    return AGENTOS_ENOTSUP;
}
#endif

/* ========== 公共嵌入接口 ========== */
agentos_error_t agentos_embedder_encode(
    embedder_handle_t* embedder,
    const char* text,
    float** out_vec,
    size_t* out_dim) {

    if (!embedder || !text || !out_vec || !out_dim) return AGENTOS_EINVAL;

    switch (embedder->type) {
        case EMBEDDER_OPENAI:
            return embed_openai(embedder, text, out_vec, out_dim);
        case EMBEDDER_DEEPSEEK:
            return embed_deepseek(embedder, text, out_vec, out_dim);
        case EMBEDDER_SENTENCE_TRANSFORMERS:
            return embed_sentence_transformers(embedder, text, out_vec, out_dim);
        default:
            return AGENTOS_ENOTSUP;
    }
}
