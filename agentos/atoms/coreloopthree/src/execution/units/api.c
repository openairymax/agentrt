/**
 * @file api.c
 * @brief API调用执行单元实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#else
#include <unistd.h>
#endif

#define MAX_URL_LENGTH 2048
#define MAX_HEADERS 16
#define MAX_BODY_SIZE 4096
#define DEFAULT_TIMEOUT_MS 30000

#define DEFAULT_API_KEY ""
#define DEFAULT_API_BASE "https://api.example.com"

#define DEFAULT_API_VERSION "v1"

typedef struct api_config {
    char base_url[256];
    char api_key[128];
    int timeout_ms;
    int max_retries;
    float retry_delay_ms;
} api_config_t;

typedef struct api_unit {
    agentos_execution_unit_t base;
    api_config_t manager;
    char** cached_responses;
    size_t cached_count;
    size_t cache_capacity;
    pthread_mutex_t mutex;
} api_unit_t;

static agentos_error_t api_execute(agentos_execution_unit_t* self,
                                    const char* input,
                                    char** output) {
    (void)self;
    (void)input;
    (void)output;
    return AGENTOS_ENOTSUP;
}

static void api_destroy(agentos_execution_unit_t* self) {
    if (!self) return;

    api_unit_t* unit = (api_unit_t*)self;

    if (unit->base.id) {
        AGENTOS_FREE((void*)unit->base.id);
        unit->base.id = NULL;
    }
    if (unit->base.description) {
        AGENTOS_FREE((void*)unit->base.description);
        unit->base.description = NULL;
    }

    if (unit->cached_responses) {
        for (size_t i = 0; i < unit->cached_count; i++) {
            if (unit->cached_responses[i]) {
                AGENTOS_FREE(unit->cached_responses[i]);
            }
        }
        AGENTOS_FREE(unit->cached_responses);
        unit->cached_responses = NULL;
    }

    pthread_mutex_destroy(&unit->mutex);

    AGENTOS_FREE(unit);
}

static const char* api_get_info(agentos_execution_unit_t* self) {
    (void)self;
    return "API Execution Unit v1.0";
}

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
    if (!unit->base.id) {
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }
    unit->base.description = AGENTOS_STRDUP("Execute HTTP/HTTPS API calls");
    if (!unit->base.description) {
        AGENTOS_FREE((void*)unit->base.id);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }
    unit->base.execute = api_execute;
    unit->base.destroy = api_destroy;
    unit->base.get_info = api_get_info;

    memcpy(&unit->manager, manager, sizeof(api_config_t));

    unit->cached_responses = (char**)AGENTOS_CALLOC(10, sizeof(char*));
    if (!unit->cached_responses) {
        AGENTOS_FREE((void*)unit->base.id);
        AGENTOS_FREE((void*)unit->base.description);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }
    unit->cache_capacity = 10;
    unit->cached_count = 0;

    pthread_mutex_init(&unit->mutex, NULL);

    *out_unit = (agentos_execution_unit_t*)unit;
    return AGENTOS_SUCCESS;
}
