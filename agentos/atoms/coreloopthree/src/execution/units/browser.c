/**
 * @file browser.c
 * @brief 浏览器控制单元（基于Playwright的简化模拟）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include <agentos/memory.h>

typedef struct browser_unit_data {
    char* metadata_json;
} browser_unit_data_t;

static int is_safe_url(const char* url) {
    if (!url) return 0;
    if (strncasecmp(url, "http://", 7) != 0 &&
        strncasecmp(url, "https://", 8) != 0 &&
        strncasecmp(url, "about:blank", 11) != 0) {
        return 0;
    }
    if (strstr(url, "javascript:") != NULL) return 0;
    if (strstr(url, "data:") != NULL) return 0;
    return 1;
}

static agentos_error_t browser_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    (void)unit;
    if (!input || !out_output) return AGENTOS_EINVAL;

    const char* cmd = (const char*)input;
    if (strstr(cmd, "navigate") != NULL) {
        const char* url_start = strstr(cmd, "http");
        if (url_start && !is_safe_url(url_start)) {
            *out_output = AGENTOS_STRDUP("{\"error\":\"unsafe_url\"}");
            return AGENTOS_EPERM;
        }
        *out_output = AGENTOS_STRDUP("{\"status\":\"navigated\"}");
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "click") != NULL) {
        *out_output = AGENTOS_STRDUP("{\"status\":\"clicked\"}");
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "screenshot") != NULL) {
        *out_output = AGENTOS_STRDUP("{\"status\":\"screenshot_taken\",\"data\":\"base64_...\"}");
        return AGENTOS_SUCCESS;
    }
    return AGENTOS_ENOTSUP;
}

static void browser_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    browser_unit_data_t* data = (browser_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t* agentos_browser_unit_create(void) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;
    memset(unit, 0, sizeof(*unit));

    browser_unit_data_t* data = (browser_unit_data_t*)AGENTOS_MALLOC(sizeof(browser_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }

    char meta[128];
    snprintf(meta, sizeof(meta), "{\"type\":\"browser\"}");
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->metadata_json) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = browser_execute;
    unit->execution_unit_destroy = browser_destroy;

    return unit;
}
