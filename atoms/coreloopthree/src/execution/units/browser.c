/**
 * @file browser.c
 * @brief 浏览器控制单元（基于Playwright的简化模拟）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct browser_unit_data {
    char* metadata_json;
} browser_unit_data_t;

static agentos_error_t browser_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    // 模拟浏览器操作
    const char* cmd = (const char*)input;
    if (strstr(cmd, "navigate") != NULL) {
        *out_output = strdup("Navigated to example.com");
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "click") != NULL) {
        *out_output = strdup("Clicked element");
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "screenshot") != NULL) {
        *out_output = strdup("screenshot_data_base64");
        return AGENTOS_SUCCESS;
    }
    return AGENTOS_ENOTSUP;
}

static void browser_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    browser_unit_data_t* data = (browser_unit_data_t*)unit->data;
    if (data) {
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

static const char* browser_get_metadata(agentos_execution_unit_t* unit) {
    browser_unit_data_t* data = (browser_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_browser_unit_create(void) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)malloc(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    browser_unit_data_t* data = (browser_unit_data_t*)malloc(sizeof(browser_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    char meta[128];
    snprintf(meta, sizeof(meta), "{\"type\":\"browser\"}");
    data->metadata_json = strdup(meta);

    if (!data->metadata_json) {
        free(data);
        free(unit);
        return NULL;
    }

    unit->data = data;
    unit->execute = browser_execute;
    unit->destroy = browser_destroy;
    unit->get_metadata = browser_get_metadata;

    return unit;
}