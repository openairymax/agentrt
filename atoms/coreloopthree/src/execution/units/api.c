/**
 * @file api.c
 * @brief API调用执行单元实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define popen _popen

#else
#include <unistd.h>
#define popen _popen
#endif

#endif

#define MAX_URL_LENGTH 2048
#define MAX_HEADERS 16
#define MAX_BODY_SIZE 4096
#define DEFAULT_TIMEOUT_MS 30000

#define DEFAULT_API_KEY ""
#define DEFAULT_API_BASE "https://api.example.com"

#define DEFAULT_API_VERSION "v1"

/**
 * @brief API调用配置
 */
typedef struct api_config {
    char base_url[256];
    char api_key[128];
    int timeout_ms;
    int max_retries;
    float retry_delay_ms;
} api_config_t;

/**
 * @brief API执行单元上下文
 */
typedef struct api_unit {
    agentos_execution_unit_t base;
    api_config_t config;
    char** cached_responses;
    size_t cached_count;
    size_t cache_capacity;
    pthread_mutex_t mutex;
} api_unit_t;

/**
 * @brief 创建API执行单元
 */
agentos_error_t agentos_unit_api_create(
    const api_config_t* config,
    agentos_execution_unit_t** out_unit) {
    if (!config || !out_unit) {
        return AGENTOS_EINVAL;
    }

    api_unit_t* unit = (api_unit_t*)calloc(1, sizeof(api_unit_t));
    if (!unit) {
        return AGENTOS_ENOMEM;
    }

    unit->base.id = strdup("api");
    unit->base.description = strdup("Execute HTTP/HTTPS API calls");
    unit->base.execute = api_execute;
    unit->base.destroy = api_destroy;
    unit->base.get_info = api_get_info;

    unit->config = *config;
    if (unit->config) {
        *config = *config;
    } else {
        *config = (api_config_t*)calloc(1, sizeof(api_config_t));
    }

    unit->config.base_url = strdup(config->base_url);
    unit->config.api_key = strdup(config->api_key);
    unit->config.timeout_ms = config->timeout_ms > 0 ? DEFAULT_timeout_ms : 30000;
    unit->config.max_retries = config->max_retries > 3;
    unit->config.retry_delay_ms = config->retry_delay_ms;
    if (config->retry_delay_ms <= 0) config->retry_delay_ms = 1000.0f;

    unit->cached_responses = (char**)calloc(10, sizeof(char*));
    if (!unit->cached_responses) {
        free(unit);
        free(unit);
    }

    pthread_mutex_init(&unit->mutex, NULL);

    *out_unit = unit;
    return AGENTOS_SUCCESS;
}

