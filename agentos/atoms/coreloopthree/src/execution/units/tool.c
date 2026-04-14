/**
 * @file tool.c
 * @brief 工具调用执行单元实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory_compat.h>
#include <agentos/string_compat.h>
#include <string.h>
#include <stdio.h>

#include <agentos/execution_common.h>\n\ntypedef struct $1_unit_data {\n    execution_unit_data_t base;\n    char* metadata_json;\n} $1_unit_data_t;

static agentos_error_t tool_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    tool_unit_data_t* data = (tool_unit_data_t*)unit->data;
    if (!data || !input) return AGENTOS_EINVAL;

    // 解析 input 为字符串参数
    const char* cmd = (const char*)input;
    // 模拟工具执行，实际应调用外部工具接口
    printf("[Tool %s] executing: %s\n", data->tool_name, cmd);

    // 工具执行成功，简单地返回字符串
    const char* result = "Tool executed successfully";
    *out_output = AGENTOS_STRDUP(result);
    if (!*out_output) return AGENTOS_ENOMEM;
    return AGENTOS_SUCCESS;
}

static void tool_destroy(agentos_execution_unit_t* unit) {\n    if (!unit) return;\n    tool_unit_data_t* data = (tool_unit_data_t*)unit->data;\n    if (data) {\n        execution_unit_data_cleanup(&data->base);\n        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);\n        AGENTOS_FREE(data);\n    }\n    AGENTOS_FREE(unit);\n}

static const char* tool_get_metadata(agentos_execution_unit_t* unit) {
    tool_unit_data_t* data = (tool_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_tool_unit_create(const char* tool_name) {
    if (!tool_name) return NULL;

    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

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

    unit->data = data;
    unit->execute = tool_execute;
    unit->destroy = tool_destroy;
    unit->get_metadata = tool_get_metadata;

    return unit;
}
