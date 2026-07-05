/**
 * @file tool.c
 * @brief 工具调用执行单元实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "execution.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


typedef struct tool_unit_data {
    char *tool_name;
    char *metadata_json;
} tool_unit_data_t;

static agentrt_error_t tool_execute(agentrt_execution_unit_t *unit, const void *input,
                                    void **out_output)
{
    tool_unit_data_t *data = (tool_unit_data_t *)unit->execution_unit_data;
    if (!data || !input)
        ATM_RET_ERR(AGENTRT_EINVAL);

    const char *cmd = (const char *)input;

    if (strncmp(cmd, "invoke ", 7) == 0) {
        const char *tool_args = cmd + 7;
        size_t args_len = strlen(tool_args);
        char *result = (char *)AGENTRT_MALLOC(args_len + 128);
        if (!result)
            ATM_RET_ERR(AGENTRT_ENOMEM);
        snprintf(result, args_len + 128, "{\"tool\":\"%s\",\"status\":\"invoked\",\"args\":\"%s\"}",
                 data->tool_name, tool_args);
        *out_output = result;
        return AGENTRT_SUCCESS;
    } else if (strncmp(cmd, "validate", 8) == 0) {
        size_t name_len = strlen(data->tool_name);
        char *result = (char *)AGENTRT_MALLOC(name_len + 64);
        if (!result)
            ATM_RET_ERR(AGENTRT_ENOMEM);
        snprintf(result, name_len + 64, "{\"tool\":\"%s\",\"status\":\"valid\"}", data->tool_name);
        *out_output = result;
        return AGENTRT_SUCCESS;
    } else if (strncmp(cmd, "describe", 8) == 0) {
        const char *meta = data->metadata_json ? data->metadata_json : "{}";
        size_t meta_len = strlen(meta);
        char *result = (char *)AGENTRT_MALLOC(meta_len + 64);
        if (!result)
            ATM_RET_ERR(AGENTRT_ENOMEM);
        snprintf(result, meta_len + 64,
                 "{\"tool\":\"%s\",\"status\":\"described\",\"metadata\":%s}", data->tool_name,
                 meta);
        *out_output = result;
        return AGENTRT_SUCCESS;
    }

    *out_output = AGENTRT_STRDUP("{\"error\":\"unsupported_tool_command\",\"status\":\"failed\"}");
    return *out_output ? AGENTRT_EPROTONOSUPPORT : AGENTRT_ENOMEM;
}

static void tool_destroy(agentrt_execution_unit_t *unit)
{
    if (!unit)
        return;
    tool_unit_data_t *data = (tool_unit_data_t *)unit->execution_unit_data;
    if (data) {
        if (data->tool_name)
            AGENTRT_FREE(data->tool_name);
        if (data->metadata_json)
            AGENTRT_FREE(data->metadata_json);
        AGENTRT_FREE(data);
    }
    AGENTRT_FREE(unit);
}

agentrt_execution_unit_t *agentrt_tool_unit_create(const char *tool_name)
{
    if (!tool_name) return NULL;

    agentrt_execution_unit_t *unit =
        (agentrt_execution_unit_t *)AGENTRT_MALLOC(sizeof(agentrt_execution_unit_t));
    if (!unit) return NULL;
    __builtin_memset(unit, 0, sizeof(*unit));

    tool_unit_data_t *data = (tool_unit_data_t *)AGENTRT_MALLOC(sizeof(tool_unit_data_t));
    if (!data) {
        AGENTRT_FREE(unit);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    data->tool_name = AGENTRT_STRDUP(tool_name);
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"tool\",\"name\":\"%s\"}", tool_name);
    data->metadata_json = AGENTRT_STRDUP(meta);

    if (!data->tool_name || !data->metadata_json) {
        if (data->tool_name)
            AGENTRT_FREE(data->tool_name);
        if (data->metadata_json)
            AGENTRT_FREE(data->metadata_json);
        AGENTRT_FREE(data);
        AGENTRT_FREE(unit);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = tool_execute;
    unit->execution_unit_destroy = tool_destroy;

    return unit;
}
