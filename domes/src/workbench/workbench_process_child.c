/**
 * @file workbench_process_child.c
 * @brief 进程模式子进程执行核心（隔离、资源限制、命令循环）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#define _GNU_SOURCE
#include "workbench_process.h"
#include <sys/resource.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>

/* 设置资源限制 */
static void setup_limits(uint64_t mem_limit, float cpu_quota) {
    if (mem_limit > 0) {
        struct rlimit mem = {mem_limit, mem_limit};
        if (setrlimit(RLIMIT_AS, &mem) != 0) {
            // 记录错误，但继续执行
        }
    }
    if (cpu_quota > 0) {
        // 简单 CPU 时间限制（秒）
        rlim_t cpu_sec = (rlim_t)(cpu_quota * 3600); // 1小时配额
        struct rlimit cpu = {cpu_sec, cpu_sec};
        setrlimit(RLIMIT_CPU, &cpu);
    }
}

/* 从文件描述符读取参数（每行一个，直到 EOF） */
static char** read_argv(int fd) {
    FILE* fp = fdopen(fd, "r");
    if (!fp) return NULL;
    char** argv = NULL;
    int argc = 0;
    char line[1024];

    while (fgets(line, sizeof(line), fp) != NULL) {
        // 去除末尾换行符
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (len == 0) continue; // 跳过空行

        char** new_argv = realloc(argv, (argc + 2) * sizeof(char*));
        if (!new_argv) goto fail;
        argv = new_argv;
        argv[argc] = strdup(line);
        if (!argv[argc]) goto fail;
        argc++;
    }
    fclose(fp);
    if (argc == 0) {
        free(argv);
        return NULL;
    }
    argv[argc] = NULL;
    return argv;

fail:
    if (argv) {
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
    }
    fclose(fp);
    return NULL;
}

/* 子进程主函数（由 clone 调用） */
int process_child_main(void* arg) {
    process_workbench_t* wb = (process_workbench_t*)arg;

    // 当父进程死亡时自动终止
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) {
        // 忽略错误
    }

    // 设置资源限制
    setup_limits(wb->memory_limit, wb->cpu_quota);

    // 关闭父进程端的管道
    close(wb->pipe_stdin[1]);  // 子进程不写 stdin
    close(wb->pipe_stdout[0]); // 子进程不从 stdout 读
    close(wb->pipe_stderr[0]); // 子进程不从 stderr 读
    // control_pipe[1] 在父进程端已关闭，子进程保留 control_pipe[0]

    // 重定向标准输入输出
    dup2(wb->pipe_stdin[0], STDIN_FILENO);
    dup2(wb->pipe_stdout[1], STDOUT_FILENO);
    dup2(wb->pipe_stderr[1], STDERR_FILENO);

    // 关闭已复制的原始描述符（可选，但建议）
    close(wb->pipe_stdin[0]);
    close(wb->pipe_stdout[1]);
    close(wb->pipe_stderr[1]);

    // 等待父进程发送命令
    char cmd;
    ssize_t n = read(wb->control_pipe[0], &cmd, 1);
    if (n != 1) {
        // 父进程异常退出或未发送命令
        _exit(1);
    }

    if (cmd == 'x') {
        // 从 control_pipe 读取参数（父进程已写入）
        char** argv = read_argv(wb->control_pipe[0]);
        close(wb->control_pipe[0]); // 不再需要

        if (!argv || !argv[0]) {
            dprintf(STDERR_FILENO, "No arguments received\n");
            _exit(2);
        }

        execvp(argv[0], argv);
        // 如果 exec 失败
        int err = errno;
        dprintf(STDERR_FILENO, "execvp failed: %s\n", strerror(err));
        _exit(127);
    } else {
        dprintf(STDERR_FILENO, "Unknown command: %c\n", cmd);
        _exit(3);
    }

    // 不应到达
    return 0;
}