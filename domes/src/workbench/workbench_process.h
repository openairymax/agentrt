/**
 * @file workbench_process.h
 * @brief 进程管理内部接口
 * @author Spharx
 * @date 2024
 */

#ifndef DOMAIN_WORKBENCH_PROCESS_H
#define DOMAIN_WORKBENCH_PROCESS_H

#include "../platform/platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 进程属性 */
typedef struct domes_process_attr {
    const char* working_dir;
    const char** env;
    bool redirect_stdin;
    bool redirect_stdout;
    bool redirect_stderr;
    domes_pipe_t stdin_pipe;
    domes_pipe_t stdout_pipe;
    domes_pipe_t stderr_pipe;
} domes_process_attr_t;

/* 进程退出状态 */
typedef struct domes_exit_status {
    int code;
    bool signaled;
    int signal;
} domes_exit_status_t;

/**
 * @brief 创建子进程
 * @param proc 进程句柄输出
 * @param path 可执行文件路径
 * @param argv 参数数组
 * @param attr 进程属性
 * @return 0 成功，其他失败
 */
int domes_process_spawn(domes_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const domes_process_attr_t* attr);

/**
 * @brief 等待进程结束
 * @param proc 进程句柄
 * @param status 退出状态输出
 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
 * @return 0 成功，DOMES_ERROR_TIMEOUT 超时，其他失败
 */
int domes_process_wait(domes_process_t proc, domes_exit_status_t* status, uint32_t timeout_ms);

/**
 * @brief 终止进程
 * @param proc 进程句柄
 * @param signal 信号（Windows 下忽略）
 * @return 0 成功，其他失败
 */
int domes_process_terminate(domes_process_t proc, int signal);

/**
 * @brief 关闭进程句柄
 * @param proc 进程句柄
 * @return 0 成功，其他失败
 */
int domes_process_close(domes_process_t proc);

/**
 * @brief 获取进程 ID
 * @param proc 进程句柄
 * @return 进程 ID
 */
domes_pid_t domes_process_getpid(domes_process_t proc);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_WORKBENCH_PROCESS_H */
