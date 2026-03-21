/**
 * @file shell.c
 * @brief Shell命令执行单元
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

typedef struct shell_unit_data {
    char* metadata_json;
} shell_unit_data_t;

static agentos_error_t shell_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    const char* cmd = (const char*)input;
    if (!cmd) return AGENTOS_EINVAL;

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return AGENTOS_EIO;

    char buffer[4096];
    size_t cap = 4096;
    char* output = (char*)malloc(cap);
    if (!output) {
        pclose(pipe);
        return AGENTOS_ENOMEM;
    }
    size_t pos = 0;
    output[0] = '\0';

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t len = strlen(buffer);
        if (pos + len + 1 > cap) {
            cap *= 2;
            char* new_out = (char*)realloc(output, cap);
            if (!new_out) {
                free(output);
                pclose(pipe);
                return AGENTOS_ENOMEM;
            }
            output = new_out;
        }
        strcpy(output + pos, buffer);
        pos += len;
    }

    int status = pclose(pipe);
    if (status != 0) {
        // 命令执行失败，但仍返回输出
    }
    *out_output = output;
    return AGENTOS_SUCCESS;
}

static void shell_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    shell_unit_data_t* data = (shell_unit_data_t*)unit->data;
    if (data) {
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

static const char* shell_get_metadata(agentos_execution_unit_t* unit) {
    shell_unit_data_t* data = (shell_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_shell_unit_create(void) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)malloc(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    shell_unit_data_t* data = (shell_unit_data_t*)malloc(sizeof(shell_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    char meta[128];
    snprintf(meta, sizeof(meta), "{\"type\":\"shell\"}");
    data->metadata_json = strdup(meta);

    if (!data->metadata_json) {
        free(data);
        free(unit);
        return NULL;
    }

    unit->data = data;
    unit->execute = shell_execute;
    unit->destroy = shell_destroy;
    unit->get_metadata = shell_get_metadata;

    return unit;
}