/**
 * @file execution_common.c
 * @brief 执行单元通用功能实现
 * 
 * 提供执行单元共享的功能，包括命令执行、结果处理等
 * 减少执行单元之间的代码重复
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution_common.h"
#include <platform.h>
#include <memory_common.h>
#include <logging_common.h>
#include <agentos_time.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

/**
 * @brief 初始化执行结果
 * @param result 执行结果指针
 * @return 0 成功，非0 失败
 */
int execution_result_init(execution_result_t* result) {
    if (!result) {
        return -1;
    }
    
    result->status = 0;
    result->output = NULL;
    result->output_size = 0;
    result->error = NULL;
    result->error_size = 0;
    result->execution_time = 0;
    
    return 0;
}

/**
 * @brief 清理执行结果
 * @param result 执行结果指针
 */
void execution_result_cleanup(execution_result_t* result) {
    if (!result) {
        return;
    }
    
    if (result->output) {
        memory_safe_free(result->output);
        result->output = NULL;
    }
    
    if (result->error) {
        memory_safe_free(result->error);
        result->error = NULL;
    }
    
    result->status = 0;
    result->output_size = 0;
    result->error_size = 0;
    result->execution_time = 0;
}

/**
 * @brief 设置执行结果
 * @param result 执行结果指针
 * @param status 状态码
 * @param output 输出内容
 * @param output_size 输出大小
 * @param error 错误信息
 * @param error_size 错误大小
 * @param execution_time 执行时间
 */
void execution_set_result(execution_result_t* result, int status, 
                         const char* output, size_t output_size, 
                         const char* error, size_t error_size, 
                         uint64_t execution_time) {
    if (!result) {
        return;
    }
    
    result->status = status;
    result->execution_time = execution_time;
    
    if (output && output_size > 0) {
        result->output = memory_safe_alloc(output_size + 1);
        if (result->output) {
            memcpy(result->output, output, output_size);
            result->output[output_size] = '\0';
            result->output_size = output_size;
        }
    }
    
    if (error && error_size > 0) {
        result->error = memory_safe_alloc(error_size + 1);
        if (result->error) {
            memcpy(result->error, error, error_size);
            result->error[error_size] = '\0';
            result->error_size = error_size;
        }
    }
}

/**
 * @brief 执行命令
 * @param command 命令字符串
 * @param manager 执行配置
 * @param result 执行结果
 * @return 0 成功，非0 失败
 */
int execution_execute_command(const char* command,
                             const execution_config_t* manager,
                             execution_result_t* result) {
    if (!command || !manager || !result) {
        return -1;
    }

    if (!execution_validate_command(command)) {
        execution_set_result(result, -1, NULL, 0, "Command validation failed", 23, 0);
        return -1;
    }

    uint64_t start_time = agentos_time_monotonic_ms();

    int status = 0;
    char* output = NULL;
    size_t output_size = 0;
    char* error = NULL;
    size_t error_size = 0;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE h_out_read, h_out_write, h_err_read, h_err_write;
    if (!CreatePipe(&h_out_read, &h_out_write, &sa, 0) ||
        !CreatePipe(&h_err_read, &h_err_write, &sa, 0)) {
        execution_set_result(result, -1, NULL, 0, "Failed to create pipes", 22, 0);
        return -1;
    }
    SetHandleInformation(h_out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(h_err_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = h_out_write;
    si.hStdError = h_err_write;
    PROCESS_INFORMATION pi = {0};

    char cmd_buf[4096];
    snprintf(cmd_buf, sizeof(cmd_buf), "cmd /c %s", command);

    BOOL created = CreateProcessA(NULL, cmd_buf, NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(h_out_write);
    CloseHandle(h_err_write);

    if (!created) {
        CloseHandle(h_out_read);
        CloseHandle(h_err_read);
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "CreateProcess failed: %lu", GetLastError());
        execution_set_result(result, -1, NULL, 0, err_msg, strlen(err_msg), 0);
        return -1;
    }

    char out_buf[8192] = {0};
    char err_buf[4096] = {0};
    DWORD out_read = 0, err_read = 0;
    ReadFile(h_out_read, out_buf, sizeof(out_buf) - 1, &out_read, NULL);
    ReadFile(h_err_read, err_buf, sizeof(err_buf) - 1, &err_read, NULL);
    out_buf[out_read] = '\0';
    err_buf[err_read] = '\0';

    WaitForSingleObject(pi.hProcess, manager->timeout_enabled ? manager->timeout_ms : INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    status = (int)exit_code;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(h_out_read);
    CloseHandle(h_err_read);

    if (out_read > 0) {
        output = memory_safe_strdup(out_buf);
        output_size = out_read;
    }
    if (err_read > 0) {
        error = memory_safe_strdup(err_buf);
        error_size = err_read;
    }
#else
    int pipe_out[2], pipe_err[2];
    if (pipe(pipe_out) != 0 || pipe(pipe_err) != 0) {
        execution_set_result(result, -1, NULL, 0, "Failed to create pipes", 22, 0);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_out[0]); close(pipe_out[1]);
        close(pipe_err[0]); close(pipe_err[1]);
        execution_set_result(result, -1, NULL, 0, "Fork failed", 11, 0);
        return -1;
    }

    if (pid == 0) {
        close(pipe_out[0]);
        close(pipe_err[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_err[1], STDERR_FILENO);
        close(pipe_out[1]);
        close(pipe_err[1]);
        execl("/bin/sh", "sh", "-c", command, NULL);
        _exit(127);
    }

    close(pipe_out[1]);
    close(pipe_err[1]);

    char out_buf[8192] = {0};
    char err_buf[4096] = {0};
    ssize_t out_total = 0, err_total = 0;
    ssize_t n;
    while ((n = read(pipe_out[0], out_buf + out_total, sizeof(out_buf) - out_total - 1)) > 0) {
        out_total += n;
        if ((size_t)out_total >= sizeof(out_buf) - 1) break;
    }
    while ((n = read(pipe_err[0], err_buf + err_total, sizeof(err_buf) - err_total - 1)) > 0) {
        err_total += n;
        if ((size_t)err_total >= sizeof(err_buf) - 1) break;
    }
    out_buf[out_total] = '\0';
    err_buf[err_total] = '\0';
    close(pipe_out[0]);
    close(pipe_err[0]);

    int wstatus;
    waitpid(pid, &wstatus, 0);
    if (WIFEXITED(wstatus)) {
        status = WEXITSTATUS(wstatus);
    } else if (WIFSIGNALED(wstatus)) {
        status = 128 + WTERMSIG(wstatus);
    } else {
        status = -1;
    }

    if (out_total > 0) {
        output = memory_safe_strdup(out_buf);
        output_size = (size_t)out_total;
    }
    if (err_total > 0) {
        error = memory_safe_strdup(err_buf);
        error_size = (size_t)err_total;
    }
#endif

    uint64_t end_time = agentos_time_monotonic_ms();
    uint64_t execution_time = end_time - start_time;

    execution_set_result(result, status, output, output_size, error, error_size, execution_time);

    if (output) memory_safe_free(output);
    if (error) memory_safe_free(error);

    return 0;
}

/**
 * @brief 验证命令安全性
 * @param command 命令字符串
 * @return true 安全，false 不安全
 */
bool execution_validate_command(const char* command) {
    if (!command) {
        return false;
    }
    
    // 简单的命令安全验证
    // 实际项目中可能需要更复杂的验证
    const char* dangerous_commands[] = {
        "rm -rf", "format", "del /f", "erase",
        "shutdown", "reboot", "halt", "poweroff",
        NULL
    };
    
    for (int i = 0; dangerous_commands[i]; i++) {
        if (strstr(command, dangerous_commands[i])) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 格式化执行结果为JSON
 * @param result 执行结果指针
 * @return JSON字符串，需要手动释放
 */
char* execution_format_result_json(const execution_result_t* result) {
    if (!result) {
        return NULL;
    }
    
    // 简单的JSON格式化
    // 实际项目中可能需要使用JSON库
    size_t buffer_size = 1024;
    char* buffer = memory_safe_alloc(buffer_size);
    if (!buffer) {
        return NULL;
    }
    
    int written = snprintf(buffer, buffer_size, 
        "{\"status\":%d,\"execution_time\":%" PRIu64 ",\"output\":\"%s\",\"error\":\"%s\"}",
        result->status, result->execution_time,
        result->output ? result->output : "",
        result->error ? result->error : ""
    );
    
    if (written >= buffer_size) {
        // 缓冲区不足，重新分配
        buffer_size = written + 1;
        char* new_buffer = memory_safe_realloc(buffer, buffer_size);
        if (!new_buffer) {
            memory_safe_free(buffer);
            return NULL;
        }
        buffer = new_buffer;
        
        snprintf(buffer, buffer_size, 
            "{\"status\":%d,\"execution_time\":%" PRIu64 ",\"output\":\"%s\",\"error\":\"%s\"}",
            result->status, result->execution_time,
            result->output ? result->output : "",
            result->error ? result->error : ""
        );
    }
    
    return buffer;
}

/**
 * @brief 初始化默认执行配置
 * @param manager 执行配置指针
 */
void execution_config_init(execution_config_t* manager) {
    if (!manager) {
        return;
    }
    
    manager->capture_output = true;
    manager->capture_error = true;
    manager->timeout_enabled = false;
    manager->timeout_ms = 30000; // 默认30秒
    manager->shell_enabled = false;
}