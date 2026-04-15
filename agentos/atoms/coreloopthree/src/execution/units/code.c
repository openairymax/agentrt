/**
 * @file code.c
 * @brief 代码执行单元（运行Python/JavaScript等）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <agentos/memory.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define AGENTOS_MAX_CODE_SIZE (4 * 1024 * 1024)

typedef struct code_unit_data {
    char* language;
    char* metadata_json;
} code_unit_data_t;

#ifdef _WIN32
static agentos_error_t create_temp_file_windows(
    const char* suffix, const char* content, size_t content_len, char** out_path) {
    char temp_dir[MAX_PATH];
    DWORD dir_len = GetTempPathA(MAX_PATH, temp_dir);
    if (dir_len == 0 || dir_len > MAX_PATH) return AGENTOS_EIO;

    char temp_path[MAX_PATH];
    UINT ret = GetTempFileNameA(temp_dir, "aos", 0, temp_path);
    if (ret == 0) return AGENTOS_EIO;

    if (suffix) {
        char final_path[MAX_PATH];
        snprintf(final_path, MAX_PATH, "%s%s", temp_path, suffix);
        if (!MoveFileA(temp_path, final_path)) {
            snprintf(final_path, MAX_PATH, "%s", temp_path);
        }
        snprintf(temp_path, MAX_PATH, "%s", final_path);
    }

    FILE* f = fopen(temp_path, "wb");
    if (!f) {
        DeleteFileA(temp_path);
        return AGENTOS_EIO;
    }

    size_t written = fwrite(content, 1, content_len, f);
    int close_result = fclose(f);

    if (written != content_len) {
        DeleteFileA(temp_path);
        return AGENTOS_EIO;
    }

    if (close_result != 0) {
        DeleteFileA(temp_path);
        return AGENTOS_EIO;
    }

    *out_path = AGENTOS_STRDUP(temp_path);
    if (!*out_path) {
        DeleteFileA(temp_path);
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}
#else
static agentos_error_t create_temp_file_unix(
    const char* suffix, const char* content, size_t content_len, char** out_path) {
    char temp_dir[256];
    const char* td = getenv("TMPDIR");
    if (!td) td = "/tmp";
    snprintf(temp_dir, sizeof(temp_dir), "%s", td);

    size_t temp_dir_len = strlen(temp_dir);
    if (temp_dir_len > 0 && temp_dir[temp_dir_len - 1] != '/') {
        if (temp_dir_len + 1 < sizeof(temp_dir)) {
            strncat(temp_dir, "/", sizeof(temp_dir) - temp_dir_len - 1);
        }
    }

    char temp_filename[512];
    int needed = snprintf(temp_filename, sizeof(temp_filename),
                          "%sagentos_code_XXXXXX%s",
                          temp_dir, suffix ? suffix : "");
    if (needed < 0 || (size_t)needed >= sizeof(temp_filename)) {
        return AGENTOS_EIO;
    }

    int fd = mkstemp(temp_filename);
    if (fd == -1) return AGENTOS_EIO;

    ssize_t written = write(fd, content, content_len);
    int close_result = close(fd);

    if (written < 0 || (size_t)written != content_len) {
        unlink(temp_filename);
        return AGENTOS_EIO;
    }

    if (close_result != 0) {
        unlink(temp_filename);
        return AGENTOS_EIO;
    }

    *out_path = AGENTOS_STRDUP(temp_filename);
    if (!*out_path) {
        unlink(temp_filename);
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}
#endif

static agentos_error_t create_temp_file(
    const char* suffix, const char* content, size_t content_len, char** out_path) {
    if (!content || !out_path) return AGENTOS_EINVAL;
#ifdef _WIN32
    return create_temp_file_windows(suffix, content, content_len, out_path);
#else
    return create_temp_file_unix(suffix, content, content_len, out_path);
#endif
}

static void remove_temp_file(const char* path) {
    if (!path) return;
#ifdef _WIN32
    DeleteFileA(path);
#else
    unlink(path);
#endif
}

static agentos_error_t execute_command_capture(const char* cmd, char** out_output) {
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
    if (!CreateProcessA(NULL, (LPSTR)cmd, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return AGENTOS_EIO;
    }
    CloseHandle(hWritePipe);

    size_t cap = 4096;
    size_t total = 0;
    char* output = (char*)AGENTOS_MALLOC(cap);
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
        if (total + len + 1 > cap) {
            cap *= 2;
            char* new_out = (char*)AGENTOS_REALLOC(output, cap);
            if (!new_out) {
                AGENTOS_FREE(output);
                CloseHandle(hReadPipe);
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return AGENTOS_ENOMEM;
            }
            output = new_out;
        }
        memcpy(output + total, buffer, len + 1);
        total += len;
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
    size_t total = 0;
    char* output = (char*)AGENTOS_MALLOC(cap);
    if (!output) {
        pclose(pipe);
        return AGENTOS_ENOMEM;
    }
    output[0] = '\0';

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t len = strlen(buffer);
        if (total + len + 1 > cap) {
            cap *= 2;
            char* new_out = (char*)AGENTOS_REALLOC(output, cap);
            if (!new_out) {
                AGENTOS_FREE(output);
                pclose(pipe);
                return AGENTOS_ENOMEM;
            }
            output = new_out;
        }
        memcpy(output + total, buffer, len + 1);
        total += len;
    }

    int status = pclose(pipe);
    *out_output = output;
    return (status == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
#endif
}

static agentos_error_t code_execute(
    agentos_execution_unit_t* unit, const void* input, void** out_output) {
    code_unit_data_t* data = (code_unit_data_t*)unit->execution_unit_data;
    if (!data || !input || !out_output) return AGENTOS_EINVAL;

    const char* code = (const char*)input;
    size_t code_len = strlen(code);

    if (code_len > AGENTOS_MAX_CODE_SIZE) {
        return AGENTOS_EINVAL;
    }

    if (strcmp(data->language, "python") != 0 &&
        strcmp(data->language, "javascript") != 0 &&
        strcmp(data->language, "node") != 0) {
        return AGENTOS_ENOTSUP;
    }

    const char* suffix = NULL;
    const char* interpreter = NULL;
    if (strcmp(data->language, "python") == 0) {
        suffix = ".py";
        interpreter = "python";
    } else if (strcmp(data->language, "javascript") == 0 ||
               strcmp(data->language, "node") == 0) {
        suffix = ".js";
        interpreter = "node";
    }

    char* temp_path = NULL;
    agentos_error_t err = create_temp_file(suffix, code, code_len, &temp_path);
    if (err != AGENTOS_SUCCESS) return err;

    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\"", interpreter, temp_path);
#else
    snprintf(cmd, sizeof(cmd), "%s '%s' 2>&1", interpreter, temp_path);
#endif

    char* output = NULL;
    err = execute_command_capture(cmd, &output);
    remove_temp_file(temp_path);
    AGENTOS_FREE(temp_path);

    if (err != AGENTOS_SUCCESS) {
        if (output) {
            *out_output = output;
            return AGENTOS_SUCCESS;
        }
        return err;
    }

    *out_output = output;
    return AGENTOS_SUCCESS;
}

static void code_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    code_unit_data_t* data = (code_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->language) AGENTOS_FREE(data->language);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t* agentos_code_unit_create(const char* language) {
    if (!language) return NULL;
    if (strlen(language) > 32) return NULL;

    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;
    memset(unit, 0, sizeof(*unit));

    code_unit_data_t* data = (code_unit_data_t*)AGENTOS_MALLOC(sizeof(code_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }

    data->language = AGENTOS_STRDUP(language);
    char meta[128];
    snprintf(meta, sizeof(meta), "{\"type\":\"code\",\"lang\":\"%s\"}", language);
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->language || !data->metadata_json) {
        if (data->language) AGENTOS_FREE(data->language);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = code_execute;
    unit->execution_unit_destroy = code_destroy;

    return unit;
}
