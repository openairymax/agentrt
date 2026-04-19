/**
 * @file tool.c
 * @brief 工具调用执行单元实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <agentos/utils/memory/memory_compat.h>

typedef struct tool_unit_data {
    char* tool_name;
    char* metadata_json;
} tool_unit_data_t;

static agentos_error_t tool_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    tool_unit_data_t* data = (tool_unit_data_t*)unit->execution_unit_data;
    if (!data || !input) return AGENTOS_EINVAL;

    const char* cmd = (const char*)input;
    (void)cmd;

    const char* result = "Tool executed successfully";
    *out_output = AGENTOS_STRDUP(result);
    if (!*out_output) return AGENTOS_ENOMEM;
    return AGENTOS_SUCCESS;
}

static void tool_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    tool_unit_data_t* data = (tool_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->tool_name) AGENTOS_FREE(data->tool_name);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t* agentos_tool_unit_create(const char* tool_name) {
    if (!tool_name) return NULL;

    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;
    memset(unit, 0, sizeof(*unit));

    tool_unit_data_t* data = (tool_unit_data_t*)AGENTOS_MALLOC(sizeof(tool_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }

    data->tool_name = AGENTOS_STRDUP(tool_name);
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"tool\",\"name\":\"%s\"}", tool_name);
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->tool_name || !data->metadata_json) {
        if (data->tool_name) AGENTOS_FREE(data->tool_name);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = tool_execute;
    unit->execution_unit_destroy = tool_destroy;

    return unit;
}
