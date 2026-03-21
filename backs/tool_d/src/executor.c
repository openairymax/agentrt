/**
 * @file executor.c
 * @brief 工具执行器实现（调用 domes 沙箱）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "executor.h"
#include "svc_logger.h"
#include "svc_error.h"
#include "utils/tool_errors.h"
#include "domes.h"  /* 假设 domes 提供客户端库 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <uv.h>

struct tool_executor {
    int worker_count;
    uv_loop_t* loop;
    uv_work_t* workers;
    /* 线程池用于同步执行，此处简化，直接使用 fork/exec */
    /* 实际可用 libuv 工作队列 */
};

/* 全局 domes 客户端句柄（简化，实际应从 context 获取） */
static domes_client_t* g_domes = NULL;

/* 初始化 domes 客户端（应在服务启动时设置） */
void tool_executor_set_domes(domes_client_t* client) {
    g_domes = client;
}

tool_executor_t* tool_executor_create(const tool_config_t* cfg) {
    tool_executor_t* exec = calloc(1, sizeof(tool_executor_t));
    if (!exec) return NULL;
    exec->worker_count = cfg->executor_workers > 0 ? cfg->executor_workers : 4;
    exec->loop = uv_loop_new();
    if (!exec->loop) {
        free(exec);
        return NULL;
    }
    /* 此处可初始化线程池，但简化处理，同步执行 */
    return exec;
}

void tool_executor_destroy(tool_executor_t* exec) {
    if (!exec) return;
    uv_loop_delete(exec->loop);
    free(exec);
}

/* 实际执行（同步 fork） */
static int run_process(const tool_metadata_t* meta,
                       const char* params_json,
                       tool_result_t** out_result) {
    /* 1. 通过 domes 检查权限 */
    if (g_domes) {
        int allowed = domes_permission_check(g_domes, meta->permission_rule, params_json);
        if (!allowed) {
            SVC_LOG_WARN("Permission denied for tool %s", meta->id);
            return TOOL_ERR_PERMISSION_DENIED;
        }
    }

    /* 2. 构造命令行 */
    /* 简单实现：将参数 JSON 作为 stdin 传入，或通过环境变量传递 */
    int pipe_stdin[2];
    int pipe_stdout[2];
    int pipe_stderr[2];
    if (pipe(pipe_stdin) < 0 || pipe(pipe_stdout) < 0 || pipe(pipe_stderr) < 0) {
        return TOOL_ERR_IO;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return TOOL_ERR_FORK;
    }

    if (pid == 0) {
        /* 子进程 */
        dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        dup2(pipe_stderr[1], STDERR_FILENO);
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        close(pipe_stderr[0]);

        /* 执行可执行文件 */
        execlp(meta->executable, meta->executable, (char*)NULL);
        _exit(127);
    }

    /* 父进程 */
    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
    close(pipe_stderr[1]);

    /* 写入参数 JSON 到 stdin */
    write(pipe_stdin[1], params_json, strlen(params_json));
    close(pipe_stdin[1]);

    /* 读取输出 */
    char stdout_buf[4096] = {0};
    char stderr_buf[4096] = {0};
    ssize_t n = read(pipe_stdout[0], stdout_buf, sizeof(stdout_buf)-1);
    if (n > 0) stdout_buf[n] = '\0';
    n = read(pipe_stderr[0], stderr_buf, sizeof(stderr_buf)-1);
    if (n > 0) stderr_buf[n] = '\0';

    close(pipe_stdout[0]);
    close(pipe_stderr[0]);

    int status;
    waitpid(pid, &status, 0);

    tool_result_t* res = calloc(1, sizeof(tool_result_t));
    if (!res) return TOOL_ERR_NOMEM;
    res->success = WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
    res->exit_code = WEXITSTATUS(status);
    res->output = strdup(stdout_buf);
    res->error = strdup(stderr_buf);
    /* 实际应测量耗时，此处省略 */

    *out_result = res;
    return 0;
}

int tool_executor_run(tool_executor_t* exec,
                      const tool_metadata_t* meta,
                      const char* params_json,
                      tool_result_t** out_result) {
    /* 简化：直接同步运行，未用线程池 */
    (void)exec;
    return run_process(meta, params_json, out_result);
}