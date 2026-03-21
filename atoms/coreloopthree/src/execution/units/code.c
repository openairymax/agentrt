/**
 * @file code.c
 * @brief 代码执行单元（运行Python/JavaScript等）
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
#include <sys/wait.h>
#endif

typedef struct code_unit_data {
    char* language;          // "python", "javascript", etc.
    char* metadata_json;
} code_unit_data_t;

static agentos_error_t code_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    code_unit_data_t* data = (code_unit_data_t*)unit->data;
    if (!data || !input) return AGENTOS_EINVAL;

    const char* code = (const char*)input;
    // 根据语言选择执行方式（示例仅支持 Python）
    if (strcmp(data->language, "python") != 0) {
        return AGENTOS_ENOTSUP;
    }

    // 将代码写入临时文件并调用 Python 解释器
    char temp_filename[] = "/tmp/agentos_code_XXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd == -1) return AGENTOS_EIO;
    FILE* f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(temp_filename);
        return AGENTOS_EIO;
    }
    fprintf(f, "%s", code);
    fclose(f);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "python %s 2>&1", temp_filename);  // 捕获错误输出
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        unlink(temp_filename);
        return AGENTOS_EIO;
    }

    char buffer[4096];
    size_t total = 0;
    size_t cap = 4096;
    char* output = (char*)malloc(cap);
    if (!output) {
        pclose(pipe);
        unlink(temp_filename);
        return AGENTOS_ENOMEM;
    }
    output[0] = '\0';

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t len = strlen(buffer);
        if (total + len + 1 > cap) {
            cap *= 2;
            char* new_out = (char*)realloc(output, cap);
            if (!new_out) {
                free(output);
                pclose(pipe);
                unlink(temp_filename);
                return AGENTOS_ENOMEM;
            }
            output = new_out;
        }
        strcpy(output + total, buffer);
        total += len;
    }

    int status = pclose(pipe);
    unlink(temp_filename);

    if (status != 0) {
        // 执行失败，但输出可能包含错误信息
        *out_output = output;  // 让调用者看到错误输出
        return AGENTOS_SUCCESS;  // 返回成功但输出为错误信息？可根据需要返回失败
    }

    *out_output = output;
    return AGENTOS_SUCCESS;
}

static void code_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    code_unit_data_t* data = (code_unit_data_t*)unit->data;
    if (data) {
        if (data->language) free(data->language);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

static const char* code_get_metadata(agentos_execution_unit_t* unit) {
    code_unit_data_t* data = (code_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_code_unit_create(const char* language) {
    if (!language) return NULL;

    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)malloc(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    code_unit_data_t* data = (code_unit_data_t*)malloc(sizeof(code_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    data->language = strdup(language);
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"code\",\"language\":\"%s\"}", language);
    data->metadata_json = strdup(meta);

    if (!data->language || !data->metadata_json) {
        if (data->language) free(data->language);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
        free(unit);
        return NULL;
    }

    unit->data = data;
    unit->execute = code_execute;
    unit->destroy = code_destroy;
    unit->get_metadata = code_get_metadata;

    return unit;
}