/**
 * @file agentos_lite.c
 * @brief AgentOS Lite 内核主入口实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/agentos_lite.h"
#include <string.h>

#define AGENTOS_LITE_VERSION_MAJOR 1
#define AGENTOS_LITE_VERSION_MINOR 0
#define AGENTOS_LITE_VERSION_PATCH 0

static const char* g_version_string = "1.0.0";
static int g_initialized = 0;

AGENTOS_LITE_API agentos_lite_error_t agentos_lite_init(void) {
    if (g_initialized) {
        return AGENTOS_LITE_SUCCESS;
    }
    
    agentos_lite_error_t err;
    
    err = agentos_lite_task_init();
    if (err != AGENTOS_LITE_SUCCESS) {
        return err;
    }
    
    err = agentos_lite_mem_init(0);
    if (err != AGENTOS_LITE_SUCCESS) {
        agentos_lite_task_cleanup();
        return err;
    }
    
    err = agentos_lite_time_init();
    if (err != AGENTOS_LITE_SUCCESS) {
        agentos_lite_mem_cleanup();
        agentos_lite_task_cleanup();
        return err;
    }
    
    g_initialized = 1;
    return AGENTOS_LITE_SUCCESS;
}

AGENTOS_LITE_API void agentos_lite_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    
    agentos_lite_time_cleanup();
    agentos_lite_mem_cleanup();
    agentos_lite_task_cleanup();
    
    g_initialized = 0;
}

AGENTOS_LITE_API const char* agentos_lite_version(void) {
    return g_version_string;
}

AGENTOS_LITE_API int agentos_lite_api_version(void) {
    return AGENTOS_LITE_API_VERSION;
}
