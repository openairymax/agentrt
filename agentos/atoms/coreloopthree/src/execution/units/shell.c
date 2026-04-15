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
#include <agentos/memory.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

typedef struct shell_unit_data {
    char* metadata_json;
} shell_unit_data_t;

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
        if (pos + len + 1 > cap) {
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
    int pipe_out[2];
    int pipe_err[2];
    if (pipe(pipe_out) == -1 || pipe(pipe_err) == -1) {
        if (pipe_out[0] != -1) { close(pipe_out[0]); close(pipe_out[1]); }
        if (pipe_err[0] != -1) { close(pipe_err[0]); close(pipe_err[1]); }
        return AGENTOS_EIO;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_out[0]); close(pipe_out[1]);
        close(pipe_err[0]); close(pipe_err[1]);
        return AGENTOS_EIO;
    }

    if (pid == 0) {
        close(pipe_out[0]);
        close(pipe_err[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_err[1], STDERR_FILENO);
        close(pipe_out[1]);
        close(pipe_err[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }

    close(pipe_out[1]);
    close(pipe_err[1]);

    size_t cap = 4096;
    size_t pos = 0;
    char* output = (char*)AGENTOS_MALLOC(cap);
    if (!output) {
        close(pipe_out[0]);
        close(pipe_err[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return AGENTOS_ENOMEM;
    }
    output[0] = '\0';

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        size_t len = (size_t)bytes_read;
        if (pos + len + 1 > cap) {
            cap *= 2;
            char* new_out = (char*)AGENTOS_REALLOC(output, cap);
            if (!new_out) {
                AGENTOS_FREE(output);
                close(pipe_out[0]);
                close(pipe_err[0]);
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                return AGENTOS_ENOMEM;
            }
            output = new_out;
        }
        memcpy(output + pos, buffer, len + 1);
        pos += len;
    }
    close(pipe_out[0]);
    close(pipe_err[0]);

    int wstatus = 0;
    waitpid(pid, &wstatus, 0);

    *out_output = output;
    return (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
#endif
}

static void shell_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    shell_unit_data_t* data = (shell_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t* agentos_shell_unit_create(void) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_CALLOC(1, sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    shell_unit_data_t* data = (shell_unit_data_t*)AGENTOS_CALLOC(1, sizeof(shell_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }

    data->metadata_json = AGENTOS_STRDUP("{\"type\":\"shell\"}");
    if (!data->metadata_json) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = shell_execute;
    unit->execution_unit_destroy = shell_destroy;

    return unit;
}
