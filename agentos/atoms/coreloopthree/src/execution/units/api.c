/**
 * @file api.c
 * @brief API Call Execution Unit Implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>

#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#define MAX_URL_LENGTH 2048
#define MAX_HEADERS 16
#define MAX_BODY_SIZE 4096
#define DEFAULT_TIMEOUT_MS 30000

typedef struct api_config {
    char base_url[256];
    char api_key[128];
    int timeout_ms;
    int max_retries;
    float retry_delay_ms;
} api_config_t;

typedef struct api_unit_data {
    char* id;
    char* description;
    api_config_t config;
    char** cached_responses;
    size_t cached_count;
    size_t cache_capacity;
    agentos_mutex_t* lock;
} api_unit_data_t;

static agentos_error_t api_execute(agentos_execution_unit_t* unit,
                                    const void* input,
                                    void** out_output) {
    (void)unit;
    (void)input;
    (void)out_output;
    return AGENTOS_ENOTSUP;
}

static void api_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;

    api_unit_data_t* data = (api_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->id) AGENTOS_FREE(data->id);
        if (data->description) AGENTOS_FREE(data->description);

        if (data->cached_responses) {
            for (size_t i = 0; i < data->cached_count; i++) {
                if (data->cached_responses[i]) {
                    AGENTOS_FREE(data->cached_responses[i]);
                }
            }
            AGENTOS_FREE(data->cached_responses);
        }

        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }

    AGENTOS_FREE(unit);
}

static const char* api_get_metadata(agentos_execution_unit_t* unit) {
    if (!unit || !unit->execution_unit_data) return "api";
    api_unit_data_t* data = (api_unit_data_t*)unit->execution_unit_data;
    return data->description ? data->description : "API Execution Unit v1.0";
}

agentos_error_t agentos_unit_api_create(
    const api_config_t* config,
    agentos_execution_unit_t** out_unit) {
    if (!config || !out_unit) {
        return AGENTOS_EINVAL;
    }

    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_CALLOC(1, sizeof(agentos_execution_unit_t));
    if (!unit) {
        return AGENTOS_ENOMEM;
    }

    api_unit_data_t* data = (api_unit_data_t*)AGENTOS_CALLOC(1, sizeof(api_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }

    data->id = AGENTOS_STRDUP("api");
    data->description = AGENTOS_STRDUP("Execute HTTP/HTTPS API calls");
    if (!data->id || !data->description) {
        if (data->id) AGENTOS_FREE(data->id);
        if (data->description) AGENTOS_FREE(data->description);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }

    memcpy(&data->config, config, sizeof(api_config_t));

    data->cached_responses = (char**)AGENTOS_CALLOC(10, sizeof(char*));
    if (!data->cached_responses) {
        AGENTOS_FREE(data->id);
        AGENTOS_FREE(data->description);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }
    data->cache_capacity = 10;
    data->cached_count = 0;

    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data->id);
        AGENTOS_FREE(data->description);
        AGENTOS_FREE(data->cached_responses);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = api_execute;
    unit->execution_unit_destroy = api_destroy;
    unit->execution_unit_get_metadata = api_get_metadata;

    *out_unit = unit;
    return AGENTOS_SUCCESS;
}
