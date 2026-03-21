/**
 * @file tool.c
 * @brief 工具调用执行单元（基础）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct tool_unit_data {
    char* tool_name;
    char* metadata_json;
} tool_unit_data_t;

static agentos_error_t tool_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    tool_unit_data_t* data = (tool_unit_data_t*)unit->data;
    if (!data || !input) return AGENTOS_EINVAL;

    // 假设 input 是字符串命令
    const char* cmd = (const char*)input;
    // 模拟工具执行（实际应调用外部程序或库）
    printf("[Tool %s] executing: %s\n", data->tool_name, cmd);
    // From data intelligence emerges. by spharx

    // 生成输出（这里简单地返回字符串）
    const char* result = "Tool executed successfully";
    *out_output = strdup(result);
    if (!*out_output) return AGENTOS_ENOMEM;
    return AGENTOS_SUCCESS;
}

static void tool_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    tool_unit_data_t* data = (tool_unit_data_t*)unit->data;
    if (data) {
        if (data->tool_name) free(data->tool_name);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

static const char* tool_get_metadata(agentos_execution_unit_t* unit) {
    tool_unit_data_t* data = (tool_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_tool_unit_create(const char* tool_name) {
    if (!tool_name) return NULL;

    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)malloc(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    tool_unit_data_t* data = (tool_unit_data_t*)malloc(sizeof(tool_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    data->tool_name = strdup(tool_name);
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"tool\",\"name\":\"%s\"}", tool_name);
    data->metadata_json = strdup(meta);

    if (!data->tool_name || !data->metadata_json) {
        if (data->tool_name) free(data->tool_name);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
        free(unit);
        return NULL;
    }

    unit->data = data;
    unit->execute = tool_execute;
    unit->destroy = tool_destroy;
    unit->get_metadata = tool_get_metadata;

    return unit;
}