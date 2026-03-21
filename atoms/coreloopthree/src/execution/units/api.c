/**
 * @file api.c
 * @brief API调用执行单元（基于libcurl）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

typedef struct api_unit_data {
    char* base_url;
    char* metadata_json;
} api_unit_data_t;

#ifdef HAVE_LIBCURL
struct memory_chunk {
    char* memory;
    size_t size;
    // From data intelligence emerges. by spharx
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct memory_chunk* mem = (struct memory_chunk*)userp;

    char* ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;  // 内存不足

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}
#endif

static agentos_error_t api_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    api_unit_data_t* data = (api_unit_data_t*)unit->data;
    if (!data || !input) return AGENTOS_EINVAL;

    // 假设 input 是一个 URL 路径（如 "/users"）
    const char* path = (const char*)input;

#ifdef HAVE_LIBCURL
    CURL* curl = curl_easy_init();
    if (!curl) return AGENTOS_ENOMEM;

    char url[512];
    snprintf(url, sizeof(url), "%s%s", data->base_url ? data->base_url : "", path);

    struct memory_chunk chunk;
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(chunk.memory);
        return AGENTOS_EIO;
    }

    *out_output = chunk.memory;  // 由调用者 free
    return AGENTOS_SUCCESS;
#else
    // 无 libcurl 时返回错误
    return AGENTOS_ENOTSUP;
#endif
}

static void api_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    api_unit_data_t* data = (api_unit_data_t*)unit->data;
    if (data) {
        if (data->base_url) free(data->base_url);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

static const char* api_get_metadata(agentos_execution_unit_t* unit) {
    api_unit_data_t* data = (api_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_api_unit_create(const char* base_url) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)malloc(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    api_unit_data_t* data = (api_unit_data_t*)malloc(sizeof(api_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    data->base_url = base_url ? strdup(base_url) : NULL;
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"api\",\"base_url\":\"%s\"}", base_url ? base_url : "");
    data->metadata_json = strdup(meta);

    if (!data->metadata_json || (base_url && !data->base_url)) {
        if (data->base_url) free(data->base_url);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
        free(unit);
        return NULL;
    }

    unit->data = data;
    unit->execute = api_execute;
    unit->destroy = api_destroy;
    unit->get_metadata = api_get_metadata;

    return unit;
}