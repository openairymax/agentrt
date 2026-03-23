/**
 * @file shell.c
 * @brief Shell命令执行单元（跨平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

typedef struct shell_unit_data {
    char* metadata_json;
} shell_unit_data_t;

/**
 * @brief 跨平台执行命令并捕获输出
 */
static agentos_error_t shell_execute(
    agentos_execution_unit_t* unit, const void* input, void** out_output) {

    (void)unit;
    const char* cmd = (const char*)input;
    if (!cmd || !out_output) return AGENTOS_EINVAL;

#ifdef _WIN32
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return AGENTOS_EIO;

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = { 0 };
    char cmdBuf[4096];
    snprintf(cmdBuf, sizeof(cmdBuf), "cmd /c %s", cmd);

    if (!CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return AGENTOS_EIO;
    }
    CloseHandle(hWritePipe);

    size_t cap = 4096;
    size_t pos = 0;
    char* output = (char*)malloc(cap);
    if (!output) {
        CloseHandle(hReadPipe);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return AGENTOS_ENOMEM;
    }
    output[0] = '\0';

    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        size_t len = bytesRead;
        if (pos + len + 1 > cap) {
            cap *= 2;
            char* new_out = (char*)realloc(output, cap);
            if (!new_out) {
                free(output);
                CloseHandle(hReadPipe);
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return AGENTOS_ENOMEM;
            }
            output = new_out;
        }
        memcpy(output + pos, buffer, len + 1);
        pos += len;
    }

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    *out_output = output;
    return (exitCode == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
#else
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return AGENTOS_EIO;

    size_t cap = 4096;
    size_t pos = 0;
    char* output = (char*)malloc(cap);
    if (!output) {
        pclose(pipe);
        return AGENTOS_ENOMEM;
    }
    output[0] = '\0';

    char buffer[4096];
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
        memcpy(output + pos, buffer, len + 1);
        pos += len;
    }

    int status = pclose(pipe);
    *out_output = output;
    return (status == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
#endif
}

/**
 * @brief 销毁Shell执行单元
 */
static void shell_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    shell_unit_data_t* data = (shell_unit_data_t*)unit->data;
    if (data) {
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

/**
 * @brief 获取执行单元元数据
 */
static const char* shell_get_metadata(agentos_execution_unit_t* unit) {
    shell_unit_data_t* data = (shell_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

/**
 * @brief 创建Shell命令执行单元
 */
agentos_execution_unit_t* agentos_shell_unit_create(void) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)calloc(1, sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    shell_unit_data_t* data = (shell_unit_data_t*)calloc(1, sizeof(shell_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    data->metadata_json = strdup("{\"type\":\"shell\"}");
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
