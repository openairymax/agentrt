/**
 * @file code.c
 * @brief 代码执行单元（运行Python/JavaScript等）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include "execution.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)

#endif

#define AGENTOS_MAX_CODE_SIZE (4 * 1024 * 1024)

typedef struct code_unit_data {
    char *language;
    char *metadata_json;
} code_unit_data_t;

#ifdef _WIN32
static agentos_error_t create_temp_file_windows(const char *suffix, const char *content,
                                                size_t content_len, char **out_path)
{
    char temp_dir[MAX_PATH];
    DWORD dir_len = GetTempPathA(MAX_PATH, temp_dir);
    if (dir_len == 0 || dir_len > MAX_PATH)
        ATM_RET_ERR(AGENTOS_EIO);

    char temp_path[MAX_PATH];
    UINT ret = GetTempFileNameA(temp_dir, "aos", 0, temp_path);
    if (ret == 0)
        ATM_RET_ERR(AGENTOS_EIO);

    if (suffix) {
        char final_path[MAX_PATH];
        snprintf(final_path, MAX_PATH, "%s%s", temp_path, suffix);
        if (!MoveFileA(temp_path, final_path)) {
            snprintf(final_path, MAX_PATH, "%s", temp_path);
        }
        snprintf(temp_path, MAX_PATH, "%s", final_path);
    }

    FILE *f = fopen(temp_path, "wb");
    if (!f) {
        DeleteFileA(temp_path);
        ATM_RET_ERR(AGENTOS_EIO);
    }

    size_t written = fwrite(content, 1, content_len, f);
    int close_result = fclose(f);

    if (written != content_len) {
        DeleteFileA(temp_path);
        ATM_RET_ERR(AGENTOS_EIO);
    }

    if (close_result != 0) {
        DeleteFileA(temp_path);
        ATM_RET_ERR(AGENTOS_EIO);
    }

    *out_path = AGENTOS_STRDUP(temp_path);
    if (!*out_path) {
        DeleteFileA(temp_path);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    return AGENTOS_SUCCESS;
}
#else
static agentos_error_t create_temp_file_unix(const char *suffix, const char *content,
                                             size_t content_len, char **out_path)
{
    char temp_dir[256];
    const char *td = getenv("TMPDIR");
    if (!td)
        td = "/tmp";
    snprintf(temp_dir, sizeof(temp_dir), "%s", td);

    size_t temp_dir_len = strlen(temp_dir);
    if (temp_dir_len > 0 && temp_dir[temp_dir_len - 1] != '/') {
        if (temp_dir_len + 1 < sizeof(temp_dir)) {
            strncat(temp_dir, "/", sizeof(temp_dir) - temp_dir_len - 1);
        }
    }

    char temp_filename[512];
    int needed = snprintf(temp_filename, sizeof(temp_filename), "%sagentos_code_XXXXXX%s", temp_dir,
                          suffix ? suffix : "");
    if (needed < 0 || (size_t)needed >= sizeof(temp_filename)) {
        ATM_RET_ERR(AGENTOS_EIO);
    }

    int fd = mkstemp(temp_filename);
    if (fd == -1)
        ATM_RET_ERR(AGENTOS_EIO);

    ssize_t written = write(fd, content, content_len);
    int close_result = close(fd);

    if (written < 0 || (size_t)written != content_len) {
        unlink(temp_filename);
        ATM_RET_ERR(AGENTOS_EIO);
    }

    if (close_result != 0) {
        unlink(temp_filename);
        ATM_RET_ERR(AGENTOS_EIO);
    }

    *out_path = AGENTOS_STRDUP(temp_filename);
    if (!*out_path) {
        unlink(temp_filename);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    return AGENTOS_SUCCESS;
}
#endif

static agentos_error_t create_temp_file(const char *suffix, const char *content, size_t content_len,
                                        char **out_path)
{
    if (!content || !out_path)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to create temp file: null content or out_path");
#ifdef _WIN32
    return create_temp_file_windows(suffix, content, content_len, out_path);
#else
    return create_temp_file_unix(suffix, content, content_len, out_path);
#endif
}

static void remove_temp_file(const char *path)
{
    if (!path)
        return;
#ifdef _WIN32
    DeleteFileA(path);
#else
    unlink(path);
#endif
}

/**
 * @brief 直接通过 argv 执行子进程并捕获 stdout/stderr（不经过 shell）
 * @param argv 以 NULL 结尾的参数数组（argv[0] 为可执行程序，由 execvp 按 PATH 解析）
 * @param out_output 输出缓冲区（调用者负责释放，包含 stdout+stderr 合并内容）
 * @return AGENTOS_SUCCESS 或错误码
 * @note 安全：直接 execvp，消除 popen/system 命令注入风险（BAN-211/235）；
 *       stderr 通过 dup2 重定向到与 stdout 相同的管道，等价于 `2>&1` 但无需 shell 介入
 */
static agentos_error_t execute_command_capture_argv(char *const argv[], char **out_output)
{
    if (!argv || !argv[0] || !out_output)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to execute: null argv or out_output");
    *out_output = NULL;

#ifdef _WIN32
    /* Windows: CreateProcessA 需要命令行字符串，按 Windows 命令行规则转义 argv */
    char cmd_line[8192];
    size_t pos = 0;
    for (int i = 0; argv[i] != NULL; i++) {
        if (i > 0 && pos < sizeof(cmd_line) - 1)
            cmd_line[pos++] = ' ';
        if (pos < sizeof(cmd_line) - 1)
            cmd_line[pos++] = '"';
        for (size_t j = 0; argv[i][j] != '\0'; j++) {
            /* Windows 命令行双引号转义：连续两个双引号 */
            if (argv[i][j] == '"') {
                if (pos < sizeof(cmd_line) - 2) {
                    cmd_line[pos++] = '"';
                    cmd_line[pos++] = '"';
                }
                continue;
            }
            if (pos < sizeof(cmd_line) - 1)
                cmd_line[pos++] = argv[i][j];
        }
        if (pos < sizeof(cmd_line) - 1)
            cmd_line[pos++] = '"';
    }
    cmd_line[pos] = '\0';

    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        ATM_RET_ERR(AGENTOS_EIO);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessA(NULL, (LPSTR)cmd_line, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        ATM_RET_ERR(AGENTOS_EIO);
    }
    CloseHandle(hWritePipe);

    size_t cap = 4096;
    size_t total = 0;
    char *output = (char *)AGENTOS_MALLOC(cap);
    if (!output) {
        CloseHandle(hReadPipe);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }
    output[0] = '\0';

    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        size_t len = bytesRead;
        if (total + len + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)AGENTOS_REALLOC(output, cap);
            if (!new_out) {
                AGENTOS_FREE(output);
                CloseHandle(hReadPipe);
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                ATM_RET_ERR(AGENTOS_ENOMEM);
            }
            output = new_out;
        }
        __builtin_memcpy(output + total, buffer, len + 1);
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
    /* POSIX: fork + execvp + pipe，直接执行不经过 shell（消除命令注入风险 BAN-211/235） */
    int pipe_out[2];
    if (pipe(pipe_out) == -1)
        ATM_RET_ERR(AGENTOS_EIO);

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_out[0]);
        close(pipe_out[1]);
        ATM_RET_ERR(AGENTOS_EIO);
    }

    if (pid == 0) {
        /* 子进程：stdout 和 stderr 都重定向到同一管道（等价于 2>&1，但无需 shell 介入） */
        close(pipe_out[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_out[1], STDERR_FILENO);
        close(pipe_out[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    close(pipe_out[1]);

    size_t cap = 4096;
    size_t total = 0;
    char *output = (char *)AGENTOS_MALLOC(cap);
    if (!output) {
        close(pipe_out[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }
    output[0] = '\0';

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        size_t len = (size_t)bytes_read;
        if (total + len + 1 > cap) {
            cap *= 2;
            char *new_out = (char *)AGENTOS_REALLOC(output, cap);
            if (!new_out) {
                AGENTOS_FREE(output);
                close(pipe_out[0]);
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                ATM_RET_ERR(AGENTOS_ENOMEM);
            }
            output = new_out;
        }
        __builtin_memcpy(output + total, buffer, len + 1);
        total += len;
    }
    close(pipe_out[0]);

    int wstatus = 0;
    waitpid(pid, &wstatus, 0);

    *out_output = output;
    return (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) ? AGENTOS_SUCCESS : AGENTOS_EIO;
#endif
}

static agentos_error_t code_execute(agentos_execution_unit_t *unit, const void *input,
                                    void **out_output)
{
    code_unit_data_t *data = (code_unit_data_t *)unit->execution_unit_data;
    if (!data || !input || !out_output)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to execute code: null data, input, or out_output");

    const char *code = (const char *)input;
    size_t code_len = strlen(code);

    if (code_len > AGENTOS_MAX_CODE_SIZE) {
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    if (strcmp(data->language, "python") != 0 && strcmp(data->language, "javascript") != 0 &&
        strcmp(data->language, "node") != 0) {
        ATM_RET_ERR(AGENTOS_EPROTONOSUPPORT);
    }

    const char *suffix = NULL;
    const char *interpreter = NULL;
    if (strcmp(data->language, "python") == 0) {
        suffix = ".py";
        interpreter = "python";
    } else if (strcmp(data->language, "javascript") == 0 || strcmp(data->language, "node") == 0) {
        suffix = ".js";
        interpreter = "node";
    }

    char *temp_path = NULL;
    agentos_error_t err = create_temp_file(suffix, code, code_len, &temp_path);
    if (err != AGENTOS_SUCCESS)
        return err;

    /* 直接构建 argv 数组：["interpreter", "temp_path", NULL]
     * 不经过 shell，无需 escape_shell_arg，无命令注入风险（BAN-211/235）
     * stderr 合并由 execute_command_capture_argv 内部 dup2 完成（等价于 2>&1，但无 shell 介入） */
    char *argv[3];
    argv[0] = (char *)interpreter;
    argv[1] = temp_path;
    argv[2] = NULL;

    char *output = NULL;
    err = execute_command_capture_argv(argv, &output);
    remove_temp_file(temp_path);
    AGENTOS_FREE(temp_path);

    if (err != AGENTOS_SUCCESS) {
        if (output)
            *out_output = output;
        return err;
    }

    *out_output = output;
    return AGENTOS_SUCCESS;
}

static void code_destroy(agentos_execution_unit_t *unit)
{
    if (!unit)
        return;
    code_unit_data_t *data = (code_unit_data_t *)unit->execution_unit_data;
    if (data) {
        if (data->language)
            AGENTOS_FREE(data->language);
        if (data->metadata_json)
            AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t *agentos_code_unit_create(const char *language)
{
    if (!language) return NULL;
    if (strlen(language) > 32) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");
        return NULL;
    }

    agentos_execution_unit_t *unit =
        (agentos_execution_unit_t *)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;
    __builtin_memset(unit, 0, sizeof(*unit));

    code_unit_data_t *data = (code_unit_data_t *)AGENTOS_MALLOC(sizeof(code_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    data->language = AGENTOS_STRDUP(language);
    char safe_lang[64];
    size_t lj = 0;
    for (size_t li = 0; language[li] && lj < sizeof(safe_lang) - 1; li++) {
        char c = language[li];
        if (c == '"' || c == '\\') {
            if (lj + 1 < sizeof(safe_lang) - 1)
                safe_lang[lj++] = '\\';
        }
        if (lj < sizeof(safe_lang) - 1 && (unsigned char)c >= 0x20) {
            safe_lang[lj++] = c;
        }
    }
    safe_lang[lj] = '\0';
    char meta[128];
    snprintf(meta, sizeof(meta), "{\"type\":\"code\",\"lang\":\"%s\"}", safe_lang);
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->language || !data->metadata_json) {
        if (data->language)
            AGENTOS_FREE(data->language);
        if (data->metadata_json)
            AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = code_execute;
    unit->execution_unit_destroy = code_destroy;

    return unit;
}
