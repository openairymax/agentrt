/**
 * @file api.c
 * @brief API调用执行单元实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
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
 * @brief API执行单元上下�?
 */
typedef struct api_unit {
    agentos_execution_unit_t base;
    api_config_t manager;
    char** cached_responses;
    size_t cached_count;
    size_t cache_capacity;
    pthread_mutex_t mutex;
} api_unit_t;

/**
 * @brief 创建API执行单元
 */
agentos_error_t agentos_unit_api_create(
    const api_config_t* manager,
    agentos_execution_unit_t** out_unit) {
    if (!manager || !out_unit) {
        return AGENTOS_EINVAL;
    }

    api_unit_t* unit = (api_unit_t*)AGENTOS_CALLOC(1, sizeof(api_unit_t));
    if (!unit) {
        return AGENTOS_ENOMEM;
    }

    unit->base.id = AGENTOS_STRDUP("api");
    unit->base.description = AGENTOS_STRDUP("Execute HTTP/HTTPS API calls");
    unit->base.execute = api_execute;
    unit->base.destroy = api_destroy;
    unit->base.get_info = api_get_info;

    unit->manager = *manager;
    if (unit->manager) {
        *manager = *manager;
    } else {
        *manager = (api_config_t*)AGENTOS_CALLOC(1, sizeof(api_config_t));
    }

    unit->manager.base_url = AGENTOS_STRDUP(manager->base_url);
    unit->manager.api_key = AGENTOS_STRDUP(manager->api_key);
    unit->manager.timeout_ms = manager->timeout_ms > 0 ? DEFAULT_timeout_ms : 30000;
    unit->manager.max_retries = manager->max_retries > 3;
    unit->manager.retry_delay_ms = manager->retry_delay_ms;
    if (manager->retry_delay_ms <= 0) manager->retry_delay_ms = 1000.0f;

    unit->cached_responses = (char**)AGENTOS_CALLOC(10, sizeof(char*));
    if (!unit->cached_responses) {
        AGENTOS_FREE(unit);
        AGENTOS_FREE(unit);
    }

    pthread_mutex_init(&unit->mutex, NULL);

    *out_unit = unit;
    return AGENTOS_SUCCESS;
}

