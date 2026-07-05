/**
 * @file shell.c
 * @brief Shell命令执行单元（跨平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "execution.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)

#endif

typedef struct shell_unit_data {
    char *metadata_json;
} shell_unit_data_t;

static const char *ALLOWED_SHELL_COMMANDS[] = {
    "ls",    "cat", "echo", "pwd",  "whoami", "date", "hostname", "df",   "du",   "free", "uptime",
    "uname", "id",  "env",  "head", "tail",   "wc",   "sort",     "uniq", "grep", "find", NULL};

static int is_shell_command_allowed(const char *cmd)
{
    if (!cmd)
        return 0;
    while (*cmd == ' ' || *cmd == '\t')
        cmd++;
    if (*cmd == '\0')
        return 0;
    if (*cmd == '/' || *cmd == '\\' || *cmd == '-')
        return 0;
    for (int i = 0; ALLOWED_SHELL_COMMANDS[i] != NULL; i++) {
        size_t len = strlen(ALLOWED_SHELL_COMMANDS[i]);
        if (strncmp(cmd, ALLOWED_SHELL_COMMANDS[i], len) == 0) {
            if (cmd[len] == ' ' || cmd[len] == '\0' || cmd[len] == '\t' || cmd[len] == '\n') {
                if (!strchr(cmd, ';') && !strstr(cmd, "&&") && !strstr(cmd, "||") &&
                    !strchr(cmd, '|') && !strstr(cmd, "$(") && !strstr(cmd, "${") &&
                    !strchr(cmd, '`') && !strchr(cmd, '>') && !strchr(cmd, '<') &&
                    !strchr(cmd, '&') && !strchr(cmd, '\n') && !strchr(cmd, '\r') &&
                    !strstr(cmd, "..")) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/**
 * @brief 将命令字符串解析为 argv 数组（按空格分割，不经过 shell）
 * @param cmd 命令字符串（已经过白名单验证和元字符过滤）
 * @param argv 输出 argv 数组（调用者负责释放 cmd_buf）
 * @param argv_max argv 数组最大容量
 * @param cmd_buf 输出命令缓冲区副本（调用者负责释放）
 * @return 参数个数，-1 表示失败
 */
static int parse_cmd_to_argv(const char *cmd, char **argv, int argv_max, char **cmd_buf)
{
    if (!cmd || !argv || !cmd_buf || argv_max < 2)
        return -1;

    /* 复制命令字符串（strtok 会修改原字符串） */
    *cmd_buf = AGENTRT_STRDUP(cmd);
    if (!*cmd_buf)
        return -1;

    int argc = 0;
    char *saveptr = NULL;
    char *token = strtok_r(*cmd_buf, " \t", &saveptr);
    while (token != NULL && argc < argv_max - 1) {
        argv[argc++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    argv[argc] = NULL;

    if (argc == 0) {
        AGENTRT_FREE(*cmd_buf);
        *cmd_buf = NULL;
        return -1;
    }

    return argc;
}

static agentrt_error_t shell_execute(agentrt_execution_unit_t *unit, const void *input,
                                     void **out_output)
{
    if (!unit || !unit->execution_unit_data)
        ATM_RET_ERR(AGENTRT_EINVAL);
    const char *cmd = (const char *)input;
    if (!cmd || !out_output)
        ATM_RET_ERR(AGENTRT_EINVAL);

    if (!is_shell_command_allowed(cmd)) {
        *out_output = AGENTRT_STRDUP("{\"error\":\"command_not_allowed\"}");
        return *out_output ? AGENTRT_EPERM : AGENTRT_ENOMEM;
    }

#ifdef _WIN32
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        ATM_RET_ERR(AGENTRT_EIO);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {0};
    char cmdBuf[4096];
    snprintf(cmdBuf, sizeof(cmdBuf), "cmd /c %s", cmd);

    if (!CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE, CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        ATM_RET_ERR(AGENTRT_EIO);
    }
    CloseHandle(hWritePipe);

    size_t cap = 4096;
    size_t pos = 0;
    char *output = (char *)AGENTRT_MALLOC(cap);
    if (!output) {
        CloseHandle(hReadPipe);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }
    output[0] = '\0';

    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        size_t len = bytesRead;
        if (pos + len + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)AGENTRT_REALLOC(output, cap);
            if (!new_out) {
                AGENTRT_FREE(output);
                CloseHandle(hReadPipe);
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                ATM_RET_ERR(AGENTRT_ENOMEM);
            }
            output = new_out;
        }
        __builtin_memcpy(output + pos, buffer, len + 1);
        pos += len;
    }

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    *out_output = output;
    return (exitCode == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
#else
    int pipe_out[2];
    int pipe_err[2];
    if (pipe(pipe_out) == -1 || pipe(pipe_err) == -1) {
        if (pipe_out[0] != -1) {
            close(pipe_out[0]);
            close(pipe_out[1]);
        }
        if (pipe_err[0] != -1) {
            close(pipe_err[0]);
            close(pipe_err[1]);
        }
        ATM_RET_ERR(AGENTRT_EIO);
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_out[0]);
        close(pipe_out[1]);
        close(pipe_err[0]);
        close(pipe_err[1]);
        ATM_RET_ERR(AGENTRT_EIO);
    }

    if (pid == 0) {
        close(pipe_out[0]);
        close(pipe_err[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_err[1], STDERR_FILENO);
        close(pipe_out[1]);
        close(pipe_err[1]);
        /* 安全：直接执行命令，不经过 shell（消除命令注入风险）
         * cmd 已经过 is_shell_command_allowed 白名单验证和元字符过滤 */
        char *argv[64];
        char *cmd_buf = NULL;
        int argc = parse_cmd_to_argv(cmd, argv, 64, &cmd_buf);
        if (argc < 0) {
            _exit(127);
        }
        execvp(argv[0], argv);
        /* execvp 失败，释放并退出 */
        if (cmd_buf) {
            /* 子进程退出时内存自动释放，此处显式释放仅为代码规范 */
        }
        _exit(127);
    }

    close(pipe_out[1]);
    close(pipe_err[1]);

    size_t cap = 4096;
    size_t pos = 0;
    char *output = (char *)AGENTRT_MALLOC(cap);
    if (!output) {
        close(pipe_out[0]);
        close(pipe_err[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }
    output[0] = '\0';

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        size_t len = (size_t)bytes_read;
        if (pos + len + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)AGENTRT_REALLOC(output, cap);
            if (!new_out) {
                AGENTRT_FREE(output);
                close(pipe_out[0]);
                close(pipe_err[0]);
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                ATM_RET_ERR(AGENTRT_ENOMEM);
            }
            output = new_out;
        }
        __builtin_memcpy(output + pos, buffer, len + 1);
        pos += len;
    }
    close(pipe_out[0]);
    close(pipe_err[0]);

    int wstatus = 0;
    waitpid(pid, &wstatus, 0);

    *out_output = output;
    return (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) ? AGENTRT_SUCCESS : AGENTRT_EIO;
#endif
}

static void shell_destroy(agentrt_execution_unit_t *unit)
{
    if (!unit)
        return;
    shell_unit_data_t *data = (shell_unit_data_t *)unit->execution_unit_data;
    if (data) {
        if (data->metadata_json)
            AGENTRT_FREE(data->metadata_json);
        AGENTRT_FREE(data);
    }
    AGENTRT_FREE(unit);
}

agentrt_execution_unit_t *agentrt_shell_unit_create(void)
{
    agentrt_execution_unit_t *unit =
        (agentrt_execution_unit_t *)AGENTRT_CALLOC(1, sizeof(agentrt_execution_unit_t));
    if (!unit) return NULL;

    shell_unit_data_t *data = (shell_unit_data_t *)AGENTRT_CALLOC(1, sizeof(shell_unit_data_t));
    if (!data) {
        AGENTRT_FREE(unit);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    data->metadata_json = AGENTRT_STRDUP("{\"type\":\"shell\"}");
    if (!data->metadata_json) {
        AGENTRT_FREE(data);
        AGENTRT_FREE(unit);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = shell_execute;
    unit->execution_unit_destroy = shell_destroy;

    return unit;
}
